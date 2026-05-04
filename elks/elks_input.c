/*
 * ELKS Digger port support code.
 *
 * Copyright (C) 2026 Denis Vasilyev <Vutshi>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
/* ELKS terminal keyboard backend for the Digger CGA-only port.
 *
 * ELKS console input gives bytes, not PC keyboard make/break events.  The
 * original Digger input code expects held directions to remain visible through
 * leftpressed/rightpressed/uppressed/downpressed until a break code arrives.
 * This backend therefore keeps a tiny synthetic key state and releases held
 * keys after typematic repeats stop for a few input service ticks.
 */

#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>
#include <errno.h>
#include "def.h"
#include "hardware.h"

#define KEY_P1_RIGHT 0x14d
#define KEY_P1_UP    0x148
#define KEY_P1_LEFT  0x14b
#define KEY_P1_DOWN  0x150
#define KEY_P1_FIRE  0x13b
#define KEY_P2_FIRE  9
#define KEY_EXIT     324
#define KEY_PAUSE    32

/* checkkeyb() is normally called twice in the current ELKS frame path
 * (before and after the short sleep).  Eight service ticks gives roughly a
 * 3-6 rendered-frame release delay while still stopping promptly after keyup.
 */
#define ELKS_HOLD_TIMEOUT      8
#define ELKS_ONESHOT_TIMEOUT   8

/* Power-of-two queue length so wraparound is a cheap mask, not modulo. */
#define ELKS_KEYQ_LEN          8
#define ELKS_KEYQ_MASK         (ELKS_KEYQ_LEN - 1)

/* Do not let typematic repeat backlog become unbounded hot-path work.
 * checkkeyb() calls elks_input_service() before consuming queued keys; cap
 * the OS-byte drain there so one slow frame cannot make the next frame drain
 * a large terminal repeat backlog.
 */
#define ELKS_INPUT_SERVICE_DRAIN_MAX 8

extern bool leftpressed, rightpressed, uppressed, downpressed, f1pressed;
extern bool left2pressed, right2pressed, up2pressed, down2pressed, f12pressed;
extern int keycodes[17][5];

static struct termios saved_termios;
static bool termios_saved;
static bool keyboard_active;
static bool cleanup_installed;
static bool cleanup_running;

static Sint4 keyq[ELKS_KEYQ_LEN];
static unsigned char keyq_head;
static unsigned char keyq_tail;

static unsigned char age_left, age_right, age_up, age_down, age_fire;
static unsigned char age_left2, age_right2, age_up2, age_down2, age_fire2;
static unsigned char age_pause, age_exit;
static bool pause_down, exit_down;
static bool service_poll_active;
static bool text_input_mode;

#ifdef DIGGER_CGA_PROFILE
unsigned int elks_input_read_count;
unsigned int elks_input_queued_count;
unsigned int elks_input_dup_count;
unsigned int elks_input_overflow_count;
unsigned int elks_input_maxq;
unsigned int elks_input_reset_count;
unsigned int elks_input_discard_count;
#endif

static bool queue_empty(void)
{
  return keyq_head == keyq_tail;
}

static void queue_clear(void)
{
  keyq_head = keyq_tail = 0;
}

static void queue_key(Sint4 key)
{
  unsigned char next;

  next = (unsigned char)((keyq_head + 1) & ELKS_KEYQ_MASK);
  if (next == keyq_tail) {
#ifdef DIGGER_CGA_PROFILE
    elks_input_overflow_count++;
#endif
    return;                    /* Drop newest on overflow; held state remains. */
  }

  keyq[keyq_head] = key;
  keyq_head = next;
#ifdef DIGGER_CGA_PROFILE
  {
    unsigned char qd = (unsigned char)((keyq_head - keyq_tail) & ELKS_KEYQ_MASK);
    elks_input_queued_count++;
    if ((unsigned int)qd > elks_input_maxq)
      elks_input_maxq = (unsigned int)qd;
  }
#endif
}

static bool dequeue_key(Sint4 *key)
{
  if (queue_empty())
    return FALSE;

  *key = keyq[keyq_tail];
  keyq_tail = (unsigned char)((keyq_tail + 1) & ELKS_KEYQ_MASK);
  return TRUE;
}

static int read_byte_now(unsigned char *ch)
{
  int r;

  do {
    r = read(0, ch, 1);
  } while (r < 0 && errno == EINTR);

  if (r == 1) {
#ifdef DIGGER_CGA_PROFILE
    elks_input_read_count++;
#endif
    return TRUE;
  }

  return FALSE;
}

static int wait_byte(unsigned char *ch)
{
  fd_set rfds;
  int r;

  do {
    FD_ZERO(&rfds);
    FD_SET(0, &rfds);
    r = select(1, &rfds, 0, 0, 0);
  } while (r < 0 && errno == EINTR);

  if (r <= 0)
    return FALSE;

  return read_byte_now(ch);
}

static bool set_held(int index)
{
  bool was_down = FALSE;

  switch (index) {
    case 0: was_down = rightpressed;  rightpressed = TRUE;  age_right = 0;  break;
    case 1: was_down = uppressed;     uppressed = TRUE;     age_up = 0;     break;
    case 2: was_down = leftpressed;   leftpressed = TRUE;   age_left = 0;   break;
    case 3: was_down = downpressed;   downpressed = TRUE;   age_down = 0;   break;
    case 4: was_down = f1pressed;     f1pressed = TRUE;     age_fire = 0;   break;
    case 5: was_down = right2pressed; right2pressed = TRUE; age_right2 = 0; break;
    case 6: was_down = up2pressed;    up2pressed = TRUE;    age_up2 = 0;    break;
    case 7: was_down = left2pressed;  left2pressed = TRUE;  age_left2 = 0;  break;
    case 8: was_down = down2pressed;  down2pressed = TRUE;  age_down2 = 0;  break;
    case 9: was_down = f12pressed;    f12pressed = TRUE;    age_fire2 = 0;  break;
  }

  return !was_down;
}

static bool apply_held_key(Sint4 key, bool *newpress)
{
  int i, j;

  for (i = 0; i < 10; i++) {
    for (j = 0; j < 5; j++) {
      if (key == keycodes[i][j]) {
        *newpress = set_held(i);
        return TRUE;
      }
    }
  }

  return FALSE;
}

static void age_held_one(bool *flag, unsigned char *age)
{
  if (!*flag)
    return;

  if (*age < 255)
    (*age)++;

  if (*age >= ELKS_HOLD_TIMEOUT) {
    *flag = FALSE;
    *age = 0;
  }
}

static void age_burst_one(bool *down, unsigned char *age)
{
  if (!*down)
    return;

  if (*age < 255)
    (*age)++;

  if (*age >= ELKS_ONESHOT_TIMEOUT) {
    *down = FALSE;
    *age = 0;
  }
}

static void age_keys(void)
{
  age_held_one(&leftpressed, &age_left);
  age_held_one(&rightpressed, &age_right);
  age_held_one(&uppressed, &age_up);
  age_held_one(&downpressed, &age_down);
  age_held_one(&f1pressed, &age_fire);
  age_held_one(&left2pressed, &age_left2);
  age_held_one(&right2pressed, &age_right2);
  age_held_one(&up2pressed, &age_up2);
  age_held_one(&down2pressed, &age_down2);
  age_held_one(&f12pressed, &age_fire2);
  age_burst_one(&pause_down, &age_pause);
  age_burst_one(&exit_down, &age_exit);
}

static void queue_one_shot(Sint4 key, bool *down, unsigned char *age)
{
  if (!*down) {
    queue_key(key);
    *down = TRUE;
  }
  *age = 0;
}

static void emit_key(Sint4 key)
{
  bool newpress = TRUE;

  /* Repeated terminal typematic bytes only need to refresh the synthetic
   * held-key state.  Queueing every repeat makes checkkeyb() walk the same
   * direction/fire key many times during slow frames.  Queue the first
   * make/re-make so menus and start-screen detection still see an event, but
   * suppress duplicate queued events while the key is already held.
   */
  if (apply_held_key(key, &newpress)) {
    if (newpress)
      queue_key(key);
#ifdef DIGGER_CGA_PROFILE
    else
      elks_input_dup_count++;
#endif
    return;
  }

  queue_key(key);
}

static void emit_pause(void)
{
  queue_one_shot(KEY_PAUSE, &pause_down, &age_pause);
}

static void emit_exit(void)
{
  queue_one_shot(KEY_EXIT, &exit_down, &age_exit);
}

static void decode_escape(void)
{
  unsigned char a, b;

  if (!read_byte_now(&a)) {
    emit_exit();
    return;
  }

  if (a == '[' || a == 'O') {
    if (!read_byte_now(&b))
      return;

    switch (b) {
      case 'A': emit_key(KEY_P1_UP);    return;
      case 'B': emit_key(KEY_P1_DOWN);  return;
      case 'C': emit_key(KEY_P1_RIGHT); return;
      case 'D': emit_key(KEY_P1_LEFT);  return;
      default: return;
    }
  }

  /* ALT/meta combinations are not useful for Digger on ELKS. */
}

static void decode_text_escape(void)
{
  unsigned char a, b;

  /* In text-entry mode, cursor-key escape sequences must not become
   * movement events or printable initials.  Drain the common ANSI form and
   * ignore it.
   */
  if (!read_byte_now(&a))
    return;

  if (a == '[' || a == 'O') {
    read_byte_now(&b);
    return;
  }
}

static void handle_text_byte(unsigned char ch)
{
  if (ch == 0x1b) {
    decode_text_escape();
    return;
  }

  if (ch == 8 || ch == 127 || (ch >= 32 && ch <= 126))
    queue_key((Sint4)ch);
}

static void handle_byte(unsigned char ch)
{
  if (text_input_mode) {
    handle_text_byte(ch);
    return;
  }

  switch (ch) {
    case 0x00:                  /* Ctrl-Space on many terminals. */
    case 0x01:                  /* Ctrl-A fallback. */
    case ' ':
      emit_key(KEY_P1_FIRE);
      break;

    case 0x03:                  /* Ctrl-C in raw mode. */
    case 'q': case 'Q':
      emit_exit();
      break;

    case 0x1b:
      decode_escape();
      break;

    case 'p': case 'P':
      emit_pause();
      break;

    case '8':
    case 'w': case 'W':
      emit_key(KEY_P1_UP);
      break;

    case '2':
    case 's': case 'S':
      emit_key(KEY_P1_DOWN);
      break;

    case '4':
    case 'a': case 'A':
      emit_key(KEY_P1_LEFT);
      break;

    case '6':
    case 'd': case 'D':
      emit_key(KEY_P1_RIGHT);
      break;

    case 0x09:                  /* Tab: keep the original player-2 fire key. */
      emit_key(KEY_P2_FIRE);
      break;

    default:
      queue_key((Sint4)ch);
      break;
  }
}

static void drain_input_limited(unsigned char maxbytes)
{
  unsigned char ch;

  while (maxbytes != 0 && read_byte_now(&ch)) {
    handle_byte(ch);
    maxbytes--;
  }
}

static void drain_input_all(void)
{
  unsigned char ch;

  while (read_byte_now(&ch))
    handle_byte(ch);
}

void elks_input_service(void)
{
  drain_input_limited(ELKS_INPUT_SERVICE_DRAIN_MAX);
  age_keys();
  service_poll_active = TRUE;
}

void elks_input_set_text_mode(bool enabled)
{
  text_input_mode = enabled;
}

void elks_input_reset(void)
{
#ifdef DIGGER_CGA_PROFILE
  elks_input_reset_count++;
#endif
  service_poll_active = FALSE;
  queue_clear();

  leftpressed = rightpressed = uppressed = downpressed = f1pressed = FALSE;
  left2pressed = right2pressed = up2pressed = down2pressed = f12pressed = FALSE;

  age_left = age_right = age_up = age_down = age_fire = 0;
  age_left2 = age_right2 = age_up2 = age_down2 = age_fire2 = 0;
  age_pause = age_exit = 0;
  pause_down = exit_down = FALSE;
}

void elks_input_flush_pending(void)
{
  unsigned char ch;

  elks_input_reset();

  /* Only raw/nonblocking mode is safe to drain here.  initkeyb() deliberately
   * uses elks_input_reset(), not this function, before terminal raw mode is
   * installed.
   */
  if (!keyboard_active)
    return;

  while (read_byte_now(&ch)) {
#ifdef DIGGER_CGA_PROFILE
    elks_input_discard_count++;
#endif
  }

  elks_input_reset();
}

static void cleanup_terminal_and_video(void)
{
  if (cleanup_running)
    return;
  cleanup_running = TRUE;
  restorekeyb();
  graphicsoff();
  cleanup_running = FALSE;
}

static void signal_cleanup(int sig)
{
  cleanup_terminal_and_video();
  exit(128 + sig);
}

static void install_cleanup(void)
{
  if (cleanup_installed)
    return;
  cleanup_installed = TRUE;
  atexit(cleanup_terminal_and_video);
#ifdef SIGINT
  signal(SIGINT, signal_cleanup);
#endif
#ifdef SIGTERM
  signal(SIGTERM, signal_cleanup);
#endif
#ifdef SIGHUP
  signal(SIGHUP, signal_cleanup);
#endif
#ifdef SIGQUIT
  signal(SIGQUIT, signal_cleanup);
#endif
}

void initkeyb(void)
{
  struct termios raw;

  install_cleanup();
  elks_input_reset();

  if (tcgetattr(0, &saved_termios) == 0) {
    termios_saved = TRUE;
    raw = saved_termios;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_iflag &= ~(ICRNL | INPCK | ISTRIP | IXON | BRKINT);
    raw.c_cflag |= CS8;
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSAFLUSH, &raw) == 0)
      keyboard_active = TRUE;
  }
}

void restorekeyb(void)
{
  if (keyboard_active && termios_saved)
    tcsetattr(0, TCSANOW, &saved_termios);
  keyboard_active = FALSE;
  elks_input_reset();
}

bool kbhit(void)
{
  if (!queue_empty())
    return TRUE;

  if (service_poll_active) {
    service_poll_active = FALSE;
    return FALSE;
  }

  drain_input_all();
  return !queue_empty();
}

Sint4 getkey(void)
{
  Sint4 key;
  unsigned char ch;

  service_poll_active = FALSE;

  if (dequeue_key(&key))
    return key;

  /* Blocking getkey() is used by pause/name-entry style code, not by the
   * hot frame path.  Clear one-shot burst latches so a fresh keypress can be
   * accepted after gameplay has stopped consuming input frames.
   */
  pause_down = exit_down = FALSE;
  age_pause = age_exit = 0;
  text_input_mode = FALSE;

  do {
    if (!wait_byte(&ch))
      return 0;
    handle_byte(ch);
    drain_input_all();
  } while (!dequeue_key(&key));

  return key;
}
