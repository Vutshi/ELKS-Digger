/*
 * ELKS Digger port support code.
 *
 * Optional ELKS CONFIG_AUDIO 8254 sequencer backend.
 *
 * This backend deliberately does not program PIT channel 2 or the speaker
 * gate port from user space. It feeds compact tone/rest events to the
 * ELKS console/tty AUDIO_ sequencer with KIOCSNDSEQ.
 *
 * Music support:
 *   music(0) bonus tune
 *   music(1) Popcorn background tune
 *   music(2) dirge/death tune
 *
 * SFX support is intentionally approximate. Short foreground SFX packets
 * flush the current sequencer queue, play a compact tone/rest pattern, then
 * allow background music to resume on later soundint() calls. There is no
 * mixer, PCM, timer-0 PWM, or direct PIT/speaker-port access here.
 */

#include "def.h"
#include "sound.h"
#include "hardware.h"
#include "digger.h"
#include "input.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linuxmt/kd.h>

/*
 * Requires the updated ELKS CONFIG_AUDIO public ABI from <linuxmt/kd.h>:
 * KIOCSNDSEQ, struct audio_event, struct audio_seq, and AUDIO_* flags.
 */

#ifdef DIGGER_CGA_PROFILE
unsigned int elks_soundint_count;
#define SOUND_PROF_TICK() (++elks_soundint_count)
#else
#define SOUND_PROF_TICK() ((void)0)
#endif

#define SEQ_FD_CLOSED       (-1)
#define SEQ_BURST_EVENTS    8u
#define SEQ_SFX_EVENTS      8u
#define SEQ_LEVELDONE_NOTES 11u
#define SEQ_LEVELDONE_DIGGER_TICKS 21u

#define SEQ_TUNE_BONUS      0u
#define SEQ_TUNE_BACKGROUND 1u
#define SEQ_TUNE_DIRGE      2u

#define SEQ_SHORT_WIDTH     9u
#define SEQ_LONG_WIDTH      18u
#define SEQ_REST_DIVISOR    0x7d00u

/*
 * ELKS sequencer runs at kernel HZ. Original Digger music timing is close
 * to 72.8 Hz, so convert Digger ticks to kernel ticks by approximately 100/73.
 * 11/8 = 1.375, close enough, using only shifts/adds.
 */
#define SEQ_TEMPO_SHIFT     3u

/*
 * Avoid hammering the kernel with one music-refill ioctl per Digger sound tick.
 */
#define SEQ_REFILL_WAIT_FULL     4u
#define SEQ_REFILL_WAIT_PARTIAL  1u

#define SEQ_SFX_PRIO_LOW         1u
#define SEQ_SFX_PRIO_MED         2u
#define SEQ_SFX_PRIO_HIGH        3u

bool soundflag = TRUE;
bool musicflag = TRUE;
Sint4 volume = 1;
Sint4 timerrate = 0x4000;
Uint4 timercount = 0;

static void noop_void(void) {}
static void noop_u16(Uint4 v) { (void)v; }
static void seq_backend_setup(void);
static void seq_backend_kill(void);
static void seq_backend_soundoff(void);

void (*setupsound)(void) = seq_backend_setup;
void (*killsound)(void) = seq_backend_kill;
void (*fillbuffer)(void) = noop_void;
void (*initint8)(void) = noop_void;
void (*restoreint8)(void) = noop_void;
void (*soundoff)(void) = seq_backend_soundoff;
void (*setspkrt2)(void) = noop_void;
void (*settimer0)(Uint4 t0v) = noop_u16;
void (*timer0)(Uint4 t0v) = noop_u16;
void (*settimer2)(Uint4 t2v) = noop_u16;
void (*timer2)(Uint4 t2v) = noop_u16;
void (*soundkillglob)(void) = seq_backend_kill;

static int seq_fd = SEQ_FD_CLOSED;
static unsigned char seq_kernel_active;
static unsigned char seq_paused;
static unsigned char seq_music_on;
static unsigned char seq_tune;
static unsigned short seq_sfx_ticks;
static unsigned char seq_refill_wait;
static unsigned char seq_sfx_prio;

static const unsigned char *seq_cur_seq;
static const unsigned short *seq_cur_freqs;
static const unsigned char *seq_cur_durs;
static unsigned char seq_cur_widthsub;

struct seq_music_state {
  unsigned int pos;
  unsigned char phase;          /* 0: need note, 1: tone pending, 2: rest pending */
  unsigned short divisor;
  unsigned short tone_ticks;
  unsigned short rest_ticks;
};

static struct seq_music_state seq_music;
static struct audio_event seq_events[SEQ_BURST_EVENTS];
static struct audio_event seq_sfx_events[SEQ_SFX_EVENTS];
static struct audio_event seq_level_events[SEQ_LEVELDONE_NOTES];

static const unsigned short bonusfreqs[10] = {
  0x11d1u, 0x0d59u, 0x0be4u, 0x0a98u, 0x0e24u,
  0x08e8u, 0x0a00u, 0x07f0u, 0x0fdfu, 0x0970u
};

/* Original bonus durations {2,4,10} pre-scaled by 3. */
static const unsigned char bonusdurs[3] = {
  6u, 12u, 30u
};

static const unsigned char bonusjingle[161] = {
  0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x11u, 0x21u, 0x31u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x01u, 0x10u,
  0x30u, 0x21u, 0x41u, 0x01u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x11u, 0x21u, 0x31u, 0x10u, 0x30u, 0x52u,
  0x60u, 0x30u, 0x20u, 0x11u, 0x31u, 0x11u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x11u, 0x21u, 0x31u, 0x00u,
  0x00u, 0x01u, 0x00u, 0x00u, 0x01u, 0x10u, 0x30u, 0x21u, 0x41u, 0x01u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u,
  0x11u, 0x21u, 0x31u, 0x10u, 0x30u, 0x52u, 0x60u, 0x30u, 0x20u, 0x11u, 0x31u, 0x11u, 0x30u, 0x30u, 0x31u, 0x30u, 0x30u, 0x31u,
  0x30u, 0x30u, 0x31u, 0x71u, 0x31u, 0x71u, 0x31u, 0x71u, 0x31u, 0x21u, 0x11u, 0x41u, 0x81u, 0x30u, 0x30u, 0x31u, 0x30u, 0x30u,
  0x31u, 0x30u, 0x30u, 0x31u, 0x71u, 0x31u, 0x71u, 0x31u, 0x71u, 0x51u, 0x91u, 0x51u, 0x91u, 0x51u, 0x30u, 0x30u, 0x31u, 0x30u,
  0x30u, 0x31u, 0x30u, 0x30u, 0x31u, 0x71u, 0x31u, 0x71u, 0x31u, 0x71u, 0x31u, 0x21u, 0x11u, 0x41u, 0x81u, 0x30u, 0x30u, 0x31u,
  0x30u, 0x30u, 0x31u, 0x30u, 0x30u, 0x31u, 0x71u, 0x31u, 0x71u, 0x31u, 0x71u, 0x51u, 0x91u, 0x51u, 0x91u, 0x51u, 0xffu
};

static const unsigned short backgfreqs[13] = {
  0x0fdfu, 0x11d1u, 0x1530u, 0x1ab2u, 0x1fbfu, 0x0e24u, 0x0d59u,
  0x1400u, 0x0a98u, 0x0be4u, 0x0970u, 0x08e8u, 0x07f0u
};

/* Original background durations {2,4} pre-scaled by 6. */
static const unsigned char backgdurs[2] = {
  12u, 24u
};

static const unsigned char backgjingle[146] = {
  0x00u, 0x10u, 0x00u, 0x20u, 0x30u, 0x20u, 0x41u, 0x00u, 0x10u,
  0x00u, 0x20u, 0x30u, 0x20u, 0x41u, 0x00u, 0x50u, 0x60u, 0x50u,
  0x60u, 0x00u, 0x50u, 0x00u, 0x50u, 0x10u, 0x00u, 0x10u, 0x00u,
  0x70u, 0x01u, 0x00u, 0x10u, 0x00u, 0x20u, 0x30u, 0x20u, 0x41u,
  0x00u, 0x10u, 0x00u, 0x20u, 0x30u, 0x20u, 0x41u, 0x00u, 0x50u,
  0x60u, 0x50u, 0x60u, 0x00u, 0x50u, 0x00u, 0x50u, 0x10u, 0x00u,
  0x10u, 0x00u, 0x50u, 97u, 0x80u, 0x90u, 0x80u, 0x60u, 0x10u,
  0x60u, 0x21u, 0x80u, 0x90u, 0x80u, 0x60u, 0x10u, 0x60u, 0x21u,
  0x80u, 0xa0u, 0xb0u, 0xa0u, 0xb0u, 0x80u, 0xa0u, 0x80u, 0xa0u,
  0x90u, 0x80u, 0x90u, 0x80u, 0x60u, 0x81u, 0x80u, 0x90u, 0x80u,
  0x60u, 0x10u, 0x60u, 0x21u, 0x80u, 0x90u, 0x80u, 0x60u, 0x10u,
  0x60u, 0x21u, 0x80u, 0xa0u, 0xb0u, 0xa0u, 0xb0u, 0x80u, 0xa0u,
  0x80u, 0xa0u, 0x90u, 0x80u, 0x90u, 0x80u, 0x60u, 0x81u, 0xc0u,
  0xb0u, 0x80u, 0x60u, 0x10u, 0x60u, 0x21u, 0x80u, 0x90u, 0x80u,
  0x60u, 0x10u, 0x60u, 0x21u, 0x80u, 0xa0u, 0xb0u, 0xa0u, 0xb0u,
  0x80u, 0xa0u, 0x80u, 0xa0u, 0x90u, 0x80u, 0x90u, 0x60u, 0x90u,
  0x81u, 0xffu
};

static const unsigned short dirgefreqs[5] = {
  0x7d00u, 0x11d1u, 0x0efbu, 0x0fdfu, 0x12e0u
};

/* Original dirge durations {2,6,4,12,16} pre-scaled by 10. */
static const unsigned char dirgedurs[5] = {
  20u, 60u, 40u, 120u, 160u
};

static const unsigned char dirge[25] = {
  0x00u, 0x11u, 0x12u, 0x10u, 0x11u, 0x22u, 0x30u, 0x32u,
  0x10u, 0x12u, 0x40u, 0x13u, 0x04u, 0x04u, 0x04u, 0x04u,
  0x04u, 0x04u, 0x04u, 0x04u, 0x04u, 0x04u, 0x04u, 0x04u,
  0xffu
};

static const unsigned short leveldone_divs[11] = {
  0x08e8u, 0x0712u, 0x05f2u, 0x07f0u, 0x06acu, 0x054cu,
  0x0712u, 0x05f2u, 0x04b8u, 0x0474u, 0x0474u
};

static const unsigned short seq_emerald_freqs[8] = {
  0x08e8u, 0x07f0u, 0x0712u, 0x06acu,
  0x05f2u, 0x054cu, 0x04b8u, 0x0474u
};

static int seq_open_fd(void)
{
  if (seq_fd >= 0)
    return seq_fd;

  seq_fd = open("/dev/console", O_WRONLY);
  if (seq_fd < 0)
    seq_fd = open("/dev/tty", O_WRONLY);

  return seq_fd;
}

static int seq_ioctl_events(struct audio_event *events, unsigned short count,
                            unsigned short flags)
{
  struct audio_seq req;

  if (seq_open_fd() < 0)
    return -1;

  req.events = events;
  req.count = count;
  req.rate_hz = 0;
  req.flags = flags;

  return ioctl(seq_fd, KIOCSNDSEQ, &req);
}

static void seq_stop_flush(void)
{
  seq_refill_wait = 0;
  seq_sfx_prio = 0;
  (void)seq_ioctl_events((struct audio_event *)0, 0,
                         AUDIO_SEQ_F_STOP | AUDIO_SEQ_F_FLUSH);
  seq_kernel_active = 0;
}

static void seq_reset_music_state(void)
{
  seq_music.pos = 0;
  seq_music.phase = 0;
  seq_music.divisor = 0;
  seq_music.tone_ticks = 0;
  seq_music.rest_ticks = 0;
}

static void seq_backend_setup(void)
{
  (void)seq_open_fd();
  seq_stop_flush();
}

static void seq_backend_soundoff(void)
{
  seq_stop_flush();
}

static void seq_backend_kill(void)
{
  seq_stop_flush();
  if (seq_fd >= 0) {
    close(seq_fd);
    seq_fd = SEQ_FD_CLOSED;
  }
}

static unsigned short seq_digger_ticks_to_kernel(unsigned short ticks)
{
  unsigned short out;

  if (ticks == 0u)
    return 1u;

  out = (unsigned short)((ticks << 3) + (ticks << 1) + ticks + 4u);
  out = (unsigned short)(out >> SEQ_TEMPO_SHIFT);
  if (out == 0u)
    out = 1u;
  return out;
}

static void seq_select_tune(unsigned char tune)
{
  seq_tune = tune;

  switch (tune) {
    case SEQ_TUNE_BONUS:
      seq_cur_seq = bonusjingle;
      seq_cur_freqs = bonusfreqs;
      seq_cur_durs = bonusdurs;
      seq_cur_widthsub = 3u;
      break;

    case SEQ_TUNE_DIRGE:
      seq_cur_seq = dirge;
      seq_cur_freqs = dirgefreqs;
      seq_cur_durs = dirgedurs;
      seq_cur_widthsub = 10u;
      break;

    case SEQ_TUNE_BACKGROUND:
    default:
      seq_cur_seq = backgjingle;
      seq_cur_freqs = backgfreqs;
      seq_cur_durs = backgdurs;
      seq_cur_widthsub = 0u;
      seq_tune = SEQ_TUNE_BACKGROUND;
      break;
  }
}

static void seq_prepare_note(struct seq_music_state *st)
{
  unsigned char e;
  unsigned short total_digger;
  unsigned short width_digger;
  unsigned short total_kernel;

  e = seq_cur_seq[st->pos++];
  if (e == 0xffu) {
    st->pos = 0;
    e = seq_cur_seq[st->pos++];
  }
  if (seq_cur_seq[st->pos] == 0xffu)
    st->pos = 0;

  st->divisor = seq_cur_freqs[e >> 4];
  total_digger = (unsigned short)seq_cur_durs[e & 15u];
  /*
   * Original Digger loads a note when noteduration == 0, plays that same
   * update, then counts down the loaded duration on following updates.
   * Therefore a nominal duration N is effectively heard for N+1 sound
   * updates.  Level-done uses the same convention.
   */
  if (total_digger != 0u)
    total_digger++;
  else
    total_digger = 1u;

  if (st->divisor == SEQ_REST_DIVISOR) {
    st->tone_ticks = seq_digger_ticks_to_kernel(total_digger);
    st->rest_ticks = 0u;
    st->phase = 1u;
    return;
  }

  if (seq_tune == SEQ_TUNE_BACKGROUND) {
    width_digger = (total_digger <= 12u) ? SEQ_SHORT_WIDTH : SEQ_LONG_WIDTH;
  }
  else if (total_digger > (unsigned short)seq_cur_widthsub) {
    width_digger = (unsigned short)(total_digger - (unsigned short)seq_cur_widthsub);
  }
  else {
    width_digger = total_digger;
  }

  if (width_digger == 0u)
    width_digger = 1u;
  if (width_digger > total_digger)
    width_digger = total_digger;

  total_kernel = seq_digger_ticks_to_kernel(total_digger);
  st->tone_ticks = seq_digger_ticks_to_kernel(width_digger);
  if (st->tone_ticks > total_kernel)
    st->tone_ticks = total_kernel;

  st->rest_ticks = (unsigned short)(total_kernel - st->tone_ticks);
  st->phase = 1;
}

static unsigned char seq_next_event(struct seq_music_state *st,
                                    struct audio_event *ev)
{
  if (st->phase == 0)
    seq_prepare_note(st);

  if (st->phase == 1) {
    ev->divisor = (st->divisor == SEQ_REST_DIVISOR) ? 0u : st->divisor;
    ev->ticks = st->tone_ticks ? st->tone_ticks : 1u;
    ev->flags = (st->divisor == SEQ_REST_DIVISOR) ? AUDIO_F_REST : AUDIO_F_TONE;
    ev->priority = 0;
    st->phase = st->rest_ticks ? 2u : 0u;
    return 1;
  }

  ev->divisor = 0;
  ev->ticks = st->rest_ticks ? st->rest_ticks : 1u;
  ev->flags = AUDIO_F_REST;
  ev->priority = 0;
  st->phase = 0;
  return 1;
}

static void seq_advance_music(unsigned short accepted)
{
  struct audio_event dummy;

  while (accepted != 0) {
    (void)seq_next_event(&seq_music, &dummy);
    accepted--;
  }
}

static void seq_refill_music(void)
{
  struct seq_music_state tmp;
  struct audio_event *ev;
  unsigned short count;
  int ret;

  if (!seq_music_on || !musicflag)
    return;

  if (seq_refill_wait != 0u) {
    seq_refill_wait--;
    return;
  }

  tmp = seq_music;
  ev = seq_events;
  for (count = 0; count < SEQ_BURST_EVENTS; count++) {
    (void)seq_next_event(&tmp, ev);
    ev++;
  }

  ret = seq_ioctl_events(seq_events, count, 0);
  if (ret > 0) {
    if ((unsigned int)ret > count)
      ret = (int)count;

    seq_advance_music((unsigned short)ret);
    seq_refill_wait = ((unsigned short)ret == count) ?
                      SEQ_REFILL_WAIT_FULL :
                      SEQ_REFILL_WAIT_PARTIAL;
    seq_kernel_active = 1;
  }
  else {
    seq_refill_wait = SEQ_REFILL_WAIT_PARTIAL;
  }
}

static void seq_sfx_put(struct audio_event **evp, unsigned short divisor,
                        unsigned short digger_ticks)
{
  struct audio_event *ev;

  ev = *evp;
  ev->divisor = divisor;
  ev->ticks = seq_digger_ticks_to_kernel(digger_ticks);
  ev->flags = divisor ? AUDIO_F_TONE : AUDIO_F_REST;
  ev->priority = 1u;
  *evp = ev + 1;
}

static void seq_sfx_cancel(void)
{
  if (seq_sfx_ticks != 0u) {
    seq_sfx_ticks = 0;
    seq_sfx_prio = 0;
    seq_stop_flush();
  }
}

static void seq_sfx_queue_prio(unsigned char count, unsigned short hold_ticks,
                               unsigned char prio)
{
  int ret;

  if (!soundflag || seq_paused || count == 0u)
    return;

  if (seq_sfx_ticks != 0u && prio < seq_sfx_prio)
    return;

  if (count > SEQ_SFX_EVENTS)
    count = SEQ_SFX_EVENTS;

  seq_sfx_ticks = (unsigned short)(hold_ticks + 2u);
  seq_sfx_prio = prio;
  seq_refill_wait = 0;

  ret = seq_ioctl_events(seq_sfx_events, (unsigned short)count,
                         AUDIO_SEQ_F_FLUSH);
  if (ret > 0)
    seq_kernel_active = 1;
  else {
    seq_sfx_ticks = 0;
    seq_sfx_prio = 0;
  }
}

static void seq_sfx_queue(unsigned char count, unsigned short hold_ticks)
{
  seq_sfx_queue_prio(count, hold_ticks, SEQ_SFX_PRIO_MED);
}

static void seq_sfx_single(unsigned short divisor, unsigned short ticks)
{
  struct audio_event *ev;

  ev = seq_sfx_events;
  seq_sfx_put(&ev, divisor, ticks);
  seq_sfx_queue(1u, ticks);
}

static void seq_sfx_fire(void)
{
  struct audio_event *ev;

  ev = seq_sfx_events;
  seq_sfx_put(&ev, 600u, 1u);
  seq_sfx_put(&ev, 900u, 1u);
  seq_sfx_put(&ev, 1300u, 1u);
  seq_sfx_queue_prio(3u, 3u, SEQ_SFX_PRIO_LOW);
}

static void seq_sfx_explode(void)
{
  struct audio_event *ev;

  ev = seq_sfx_events;
  seq_sfx_put(&ev, 1500u, 2u);
  seq_sfx_put(&ev, 1312u, 2u);
  seq_sfx_put(&ev, 1148u, 2u);
  seq_sfx_put(&ev, 1004u, 2u);
  seq_sfx_put(&ev, 878u, 2u);
  seq_sfx_put(&ev, 768u, 2u);
  seq_sfx_put(&ev, 672u, 2u);
  seq_sfx_put(&ev, 588u, 2u);
  seq_sfx_queue_prio(8u, 16u, SEQ_SFX_PRIO_HIGH);
}

static void seq_sfx_emerald(unsigned char n)
{
  struct audio_event *ev;
  unsigned short divisor;

  divisor = seq_emerald_freqs[n & 7u];

  ev = seq_sfx_events;
  seq_sfx_put(&ev, divisor, 2u);
  seq_sfx_put(&ev, 0u, 2u);
  seq_sfx_put(&ev, divisor, 2u);
  seq_sfx_queue_prio(3u, 6u, SEQ_SFX_PRIO_LOW);
}

static void seq_sfx_gold(void)
{
  struct audio_event *ev;

  ev = seq_sfx_events;
  seq_sfx_put(&ev, 500u, 3u);
  seq_sfx_put(&ev, 4000u, 3u);
  seq_sfx_put(&ev, 625u, 3u);
  seq_sfx_put(&ev, 3500u, 3u);
  seq_sfx_put(&ev, 780u, 3u);
  seq_sfx_put(&ev, 3100u, 3u);
  seq_sfx_put(&ev, 975u, 3u);
  seq_sfx_put(&ev, 2700u, 3u);
  seq_sfx_queue(8u, 24u);
}

static void seq_sfx_eatm(void)
{
  struct audio_event *ev;

  ev = seq_sfx_events;
  seq_sfx_put(&ev, 2000u, 2u);
  seq_sfx_put(&ev, 1875u, 2u);
  seq_sfx_put(&ev, 1500u, 2u);
  seq_sfx_put(&ev, 1406u, 2u);
  seq_sfx_put(&ev, 1125u, 2u);
  seq_sfx_put(&ev, 1055u, 2u);
  seq_sfx_put(&ev, 844u, 2u);
  seq_sfx_put(&ev, 791u, 2u);
  seq_sfx_queue(8u, 16u);
}

static void seq_sfx_fall(void)
{
  struct audio_event *ev;

  ev = seq_sfx_events;
  seq_sfx_put(&ev, 1000u, 2u);
  seq_sfx_put(&ev, 0u, 2u);
  seq_sfx_put(&ev, 1300u, 2u);
  seq_sfx_put(&ev, 0u, 2u);
  seq_sfx_put(&ev, 1700u, 2u);
  seq_sfx_put(&ev, 0u, 2u);
  seq_sfx_put(&ev, 2200u, 2u);
  seq_sfx_put(&ev, 0u, 2u);
  seq_sfx_queue(8u, 16u);
}

static void seq_sfx_wobble(void)
{
  struct audio_event *ev;

  ev = seq_sfx_events;
  seq_sfx_put(&ev, 0x07d0u, 3u);
  seq_sfx_put(&ev, 0x09c4u, 3u);
  seq_sfx_put(&ev, 0x0bb8u, 3u);
  seq_sfx_put(&ev, 0x09c4u, 3u);
  seq_sfx_queue_prio(4u, 12u, SEQ_SFX_PRIO_LOW);
}

static void seq_sfx_bonus(void)
{
  struct audio_event *ev;

  ev = seq_sfx_events;
  seq_sfx_put(&ev, 0x04ceu, 3u);
  seq_sfx_put(&ev, 0u, 2u);
  seq_sfx_put(&ev, 0x05e9u, 3u);
  seq_sfx_put(&ev, 0u, 2u);
  seq_sfx_put(&ev, 0x04ceu, 3u);
  seq_sfx_put(&ev, 0u, 2u);
  seq_sfx_put(&ev, 0x05e9u, 3u);
  seq_sfx_queue(7u, 18u);
}

static void seq_queue_leveldone(void)
{
  unsigned int i;
  unsigned short ticks;

  ticks = seq_digger_ticks_to_kernel(SEQ_LEVELDONE_DIGGER_TICKS);

  for (i = 0; i < SEQ_LEVELDONE_NOTES; i++) {
    seq_level_events[i].divisor = leveldone_divs[i];
    seq_level_events[i].ticks = ticks;
    seq_level_events[i].flags = AUDIO_F_TONE;
    seq_level_events[i].priority = SEQ_SFX_PRIO_HIGH;
  }

  if (seq_ioctl_events(seq_level_events, SEQ_LEVELDONE_NOTES,
                       AUDIO_SEQ_F_FLUSH) > 0)
    seq_kernel_active = 1;
}

void initsound(void)
{
  inittimer();
  curtime = 0;
  soundflag = TRUE;
  musicflag = TRUE;
  seq_paused = 0;
  seq_music_on = 0;
  seq_sfx_ticks = 0;
  seq_sfx_prio = 0;
  seq_refill_wait = 0;
  seq_select_tune(SEQ_TUNE_BACKGROUND);
  seq_reset_music_state();
  seq_backend_setup();
}

void soundstop(void)
{
  seq_music_on = 0;
  seq_sfx_ticks = 0;
  seq_sfx_prio = 0;
  seq_reset_music_state();
  seq_stop_flush();
}

void music(Sint4 tune)
{
  if ((unsigned char)tune > SEQ_TUNE_DIRGE)
    return;

  seq_sfx_ticks = 0;
  seq_sfx_prio = 0;
  seq_select_tune((unsigned char)tune);
  seq_music_on = 1;
  seq_reset_music_state();
  seq_stop_flush();
}

void musicoff(void)
{
  seq_music_on = 0;
  seq_sfx_ticks = 0;
  seq_sfx_prio = 0;
  seq_reset_music_state();
  seq_stop_flush();
}

void soundlevdone(void)
{
  unsigned short n;

  if (!soundflag)
    return;

  seq_music_on = 0;
  seq_sfx_ticks = 0;
  seq_sfx_prio = 0;
  seq_reset_music_state();
  seq_stop_flush();

  seq_queue_leveldone();

  for (n = 0;
       n < (unsigned short)(SEQ_LEVELDONE_NOTES * SEQ_LEVELDONE_DIGGER_TICKS) &&
       !escape;
       n++) {
    fillbuffer();
    checkkeyb();
    olddelay(14);
  }

  seq_stop_flush();
}

void sound1up(void) {}

void soundpause(void)
{
  seq_paused = 1;
  seq_sfx_ticks = 0;
  seq_sfx_prio = 0;
  seq_stop_flush();
}

void soundpauseoff(void)
{
  seq_paused = 0;
  seq_refill_wait = 0;
}

void setsoundt2(void) {}
void sett2val(Sint4 t2v) { (void)t2v; }
void startint8(void) {}
void stopint8(void) { seq_stop_flush(); }

void soundbonus(void) { seq_sfx_bonus(); }
void soundbonusoff(void) { seq_sfx_cancel(); }
void soundfire(int n) { (void)n; seq_sfx_fire(); }
void soundexplode(int n) { (void)n; seq_sfx_explode(); }
void soundfireoff(int n) { (void)n; }
void soundem(void) { seq_sfx_single(1000u, 3u); }
void soundemerald(int emn) { seq_sfx_emerald((unsigned char)emn); }
void soundeatm(void) { seq_sfx_eatm(); }

void soundddie(void)
{
  /*
   * Keep dynamic death SFX simple. The actual death tune starts when
   * gameplay calls music(2).
   */
  seq_sfx_ticks = 0;
  seq_sfx_prio = 0;
  seq_stop_flush();
}

void soundwobble(void) { seq_sfx_wobble(); }
void soundwobbleoff(void) { seq_sfx_cancel(); }
void soundfall(void) { seq_sfx_fall(); }
void soundfalloff(void) { seq_sfx_cancel(); }
void soundbreak(void) { seq_sfx_single(15000u, 4u); }
void soundgold(void) { seq_sfx_gold(); }

void soundint(void)
{
  SOUND_PROF_TICK();
  timercount++;

  if (!soundflag || seq_paused) {
    if (seq_kernel_active)
      seq_stop_flush();
    seq_sfx_ticks = 0;
    return;
  }

  if (seq_sfx_ticks != 0u) {
    seq_sfx_ticks--;
    if (seq_sfx_ticks == 0u) {
      seq_sfx_prio = 0;
      seq_stop_flush();
    }
    return;
  }

  if (!musicflag) {
    if (seq_kernel_active)
      seq_stop_flush();
    return;
  }

  seq_refill_music();
}
