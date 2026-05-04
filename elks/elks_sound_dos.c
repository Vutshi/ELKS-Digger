/*
 * ELKS Digger port support code.
 *
 * Copyright (C) 2026 Denis Vasilyev <Vutshi>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
/* ELKS PC speaker backend using the original DOS Digger sound state machine.
 *
 * The original DOS code drives the PC speaker from a timer interrupt and can
 * use timer-0 PWM-style speaker modes.  ELKS already advances sound from the
 * game frame path, so this file keeps the original sound/music sequencing and
 * feeds the selected PIT divisors to the existing ELKS direct speaker backend.
 *
 * Build modes:
 *   make              -> DOS sound state machine + direct ELKS PC speaker
 *   make SOUND=direct -> older small ELKS sound approximation
 *   make SOUND=0      -> silent/stub backend
 */

#include "def.h"
#include "sound.h"
#include "hardware.h"
#include "main.h"
#include "digger.h"
#include "input.h"

#ifdef DIGGER_CGA_PROFILE
unsigned int elks_soundint_count;
#define SOUND_PROF_TICK() (++elks_soundint_count)
#else
#define SOUND_PROF_TICK() ((void)0)
#endif

#if defined(DIGGER_ELKS_PCSPK_DIRECT) && !defined(DIGGER_SOUND_STUB)
#include <autoconf.h>
#include "arch/io.h"
#include "arch/ports.h"

#ifndef SPEAKER_PORT
#define SPEAKER_PORT 0x61u
#endif
#ifndef TIMER_CMDS_PORT
#define TIMER_CMDS_PORT 0x43u
#endif
#ifndef TIMER2_PORT
#ifdef TIMER2_DATA_PORT
#define TIMER2_PORT TIMER2_DATA_PORT
#else
#define TIMER2_PORT 0x42u
#endif
#endif
#endif

#define ELKS_TIMER2_MODE3        0xb6u
#define ELKS_PCSPK_MIN_DIV       64u
#define ELKS_PCSPK_MAX_DIV       30000u
#define ELKS_PCSPK_REST_DIV      0x7d00u

/* ELKS user space does not use PIT timer 0 for Digger sound.  All physical
 * output goes through PIT channel 2, mode 3, with cached divisor/gate writes.
 * Music and SFX remain separate state machines, but they both feed a single
 * final t2val chosen in soundint(); SFX has priority over music output.
 */
#define ELKS_POPCORN_SHORT_WIDTH 9
#define ELKS_POPCORN_LONG_WIDTH  18

static unsigned char elks_speaker_on;
static unsigned short elks_last_divisor;

static void elks_pcspk_force_off(void)
{
#if defined(DIGGER_ELKS_PCSPK_DIRECT) && !defined(DIGGER_SOUND_STUB) && defined(CONFIG_ARCH_IBMPC)
  if (elks_speaker_on) {
    unsigned int tmp = inb(SPEAKER_PORT) & 0xfcu;
    outb(tmp, SPEAKER_PORT);
  }
#endif
  elks_speaker_on = 0;
  elks_last_divisor = 0;
}

static void elks_pcspk_set_divisor(unsigned short divisor)
{
#if defined(DIGGER_ELKS_PCSPK_DIRECT) && !defined(DIGGER_SOUND_STUB) && defined(CONFIG_ARCH_IBMPC)
  unsigned int tmp;
#endif

  if (divisor == 0 || divisor == 40u || divisor == ELKS_PCSPK_REST_DIV ||
      divisor < ELKS_PCSPK_MIN_DIV || divisor > ELKS_PCSPK_MAX_DIV) {
    elks_pcspk_force_off();
    return;
  }

  if (elks_speaker_on && divisor == elks_last_divisor)
    return;

#if defined(DIGGER_ELKS_PCSPK_DIRECT) && !defined(DIGGER_SOUND_STUB) && defined(CONFIG_ARCH_IBMPC)
  outb((unsigned int)ELKS_TIMER2_MODE3, TIMER_CMDS_PORT);
  outb((unsigned int)divisor, TIMER2_PORT);
  outb((unsigned int)(divisor >> 8), TIMER2_PORT);

  if (!elks_speaker_on) {
    tmp = inb(SPEAKER_PORT);
    if (tmp != (tmp | 3u))
      outb(tmp | 3u, SPEAKER_PORT);
  }
#endif
  elks_speaker_on = 1;
  elks_last_divisor = divisor;
}


Sint4 timerrate = 0x4000;
Uint4 timercount = 0, t2val = 0;
Sint4 volume = 0;
Sint3 timerclock = 0;
bool soundflag = TRUE, musicflag = TRUE;
bool sndflag = FALSE, soundpausedflag = FALSE;
Sint5 randvs;

void soundint(void);


void soundlevdoneoff(void);
void soundlevdoneupdate(void);
void soundfallupdate(void);
void soundbreakoff(void);
void soundbreakupdate(void);
void soundwobbleupdate(void);
void soundfireupdate(void);
void soundexplodeoff(int n);
void soundexplodeupdate(void);
void soundbonusupdate(void);
void soundemoff(void);
void soundemupdate(void);
void soundemeraldoff(void);
void soundemeraldupdate(void);
void soundgoldoff(void);
void soundgoldupdate(void);
void soundeatmoff(void);
void soundeatmupdate(void);
void soundddieoff(void);
void soundddieupdate(void);
void sound1upoff(void);
void sound1upupdate(void);
void musicupdate(void);
static bool elks_sfx_active(void);
void elks_sound_off(void) { elks_pcspk_force_off(); }
void elks_sound_timer2(Uint4 t2v)
{
  elks_pcspk_set_divisor((unsigned short)t2v);
}

Sint4 randnos(Sint4 n)
{
  if (n <= 1)
    return 0;
  randvs = randvs * 0x15a4e35l + 1;
  return (Sint4)((randvs & 0x7fffffffl) % n);
}

void sett2val(Sint4 t2v)
{
  if (sndflag)
    timer2((Uint4)t2v);
}

void soundint(void)
{
  SOUND_PROF_TICK();
  timerclock++;
  if (soundflag && !sndflag)
    sndflag = musicflag = TRUE;
  if (!soundflag && sndflag) {
    sndflag = FALSE;
    soundoff();
  }
  if (sndflag && !soundpausedflag) {
    Uint4 music_t2val = 0;
    bool clean_music_tick = FALSE;

    t2val = 40;
    if (musicflag) {
      musicupdate();
      if (t2val != 40) {
        music_t2val = t2val;
        clean_music_tick = TRUE;
      }
    }
    soundemeraldupdate();
    soundwobbleupdate();
    soundddieupdate();
    soundbreakupdate();
    soundgoldupdate();
    soundemupdate();
    soundexplodeupdate();
    soundfireupdate();
    soundeatmupdate();
    soundfallupdate();
    sound1upupdate();
    soundbonusupdate();

    /* If an effect is active but silent on this tick, leave a gap instead
     * of allowing background music to immediately overwrite it.  This is the
     * audible DOS-like priority rule while still using a single cheap PIT2
     * square-wave backend.
     */
    if (clean_music_tick && elks_sfx_active() && t2val == music_t2val)
      t2val = 40;

    sett2val(t2val);
  }
}

void soundstop(void)
{
  int i;
  soundfalloff();
  soundwobbleoff();
  for (i = 0; i < FIREBALLS; i++)
    soundfireoff(i);
  musicoff();
  soundbonusoff();
  for (i = 0; i < FIREBALLS; i++)
    soundexplodeoff(i);
  soundbreakoff();
  soundemoff();
  soundemeraldoff();
  soundgoldoff();
  soundeatmoff();
  soundddieoff();
  sound1upoff();
}

bool soundlevdoneflag = FALSE;
Sint4 nljpointer = 0, nljnoteduration = 0;
static const Sint4 newlevjingle[11] = {0x8e8,0x712,0x5f2,0x7f0,0x6ac,0x54c,
                                       0x712,0x5f2,0x4b8,0x474,0x474};

void soundlevdone(void)
{
  soundstop();
  nljpointer = 0;
  nljnoteduration = 20;
  soundlevdoneflag = soundpausedflag = TRUE;
  while (soundlevdoneflag && !escape) {
    fillbuffer();
    checkkeyb();
    soundint();
    soundlevdoneupdate();
    olddelay(14);
  }
  soundlevdoneoff();
}

void soundlevdoneoff(void)
{
  soundlevdoneflag = soundpausedflag = FALSE;
  elks_pcspk_force_off();
}

void soundlevdoneupdate(void)
{
  if (sndflag) {
    if (nljpointer < 11)
      t2val = newlevjingle[nljpointer];

    sett2val(t2val);

    if (nljnoteduration > 0)
      nljnoteduration--;
    else {
      nljnoteduration = 20;
      nljpointer++;
      if (nljpointer > 10)
        soundlevdoneoff();
    }
  }
  else
    soundlevdoneflag = FALSE;
}

bool soundfallflag = FALSE, soundfallf = FALSE;
Sint4 soundfallvalue, soundfalln = 0;

void soundfall(void)
{
  soundfallvalue = 1000;
  soundfallflag = TRUE;
}

void soundfalloff(void)
{
  soundfallflag = FALSE;
  soundfalln = 0;
}

void soundfallupdate(void)
{
  if (soundfallflag) {
    if (soundfalln < 1) {
      soundfalln++;
      if (soundfallf)
        t2val = soundfallvalue;
    }
    else {
      soundfalln = 0;
      if (soundfallf) {
        soundfallvalue += 50;
        soundfallf = FALSE;
      }
      else
        soundfallf = TRUE;
    }
  }
}

bool soundbreakflag = FALSE;
Sint4 soundbreakduration = 0, soundbreakvalue = 0;

void soundbreak(void)
{
  soundbreakduration = 3;
  if (soundbreakvalue < 15000)
    soundbreakvalue = 15000;
  soundbreakflag = TRUE;
}

void soundbreakoff(void)
{
  soundbreakflag = FALSE;
}

void soundbreakupdate(void)
{
  if (soundbreakflag) {
    if (soundbreakduration != 0) {
      soundbreakduration--;
      t2val = soundbreakvalue;
    }
    else
      soundbreakflag = FALSE;
  }
}

bool soundwobbleflag = FALSE;
Sint4 soundwobblen = 0;

void soundwobble(void)
{
  soundwobbleflag = TRUE;
}

void soundwobbleoff(void)
{
  soundwobbleflag = FALSE;
  soundwobblen = 0;
}

void soundwobbleupdate(void)
{
  if (soundwobbleflag) {
    soundwobblen++;
    if (soundwobblen > 63)
      soundwobblen = 0;
    switch (soundwobblen) {
      case 0:
        t2val = 0x7d0;
        break;
      case 16:
      case 48:
        t2val = 0x9c4;
        break;
      case 32:
        t2val = 0xbb8;
        break;
    }
  }
}

bool soundfireflag[FIREBALLS] = {FALSE,FALSE}, sff[FIREBALLS];
Sint4 soundfirevalue[FIREBALLS], soundfiren[FIREBALLS] = {0,0};
int soundfirew = 0;

void soundfire(int n)
{
  soundfirevalue[n] = 500;
  soundfireflag[n] = TRUE;
}

void soundfireoff(int n)
{
  soundfireflag[n] = FALSE;
  soundfiren[n] = 0;
}

void soundfireupdate(void)
{
  int n;
  bool f = FALSE;
  for (n = 0; n < FIREBALLS; n++) {
    sff[n] = FALSE;
    if (soundfireflag[n]) {
      if (soundfiren[n] == 1) {
        soundfiren[n] = 0;
        soundfirevalue[n] += soundfirevalue[n] / 55;
        sff[n] = TRUE;
        f = TRUE;
        if (soundfirevalue[n] > 30000)
          soundfireoff(n);
      }
      else
        soundfiren[n]++;
    }
  }
  if (f) {
    do {
      n = soundfirew++;
      if (soundfirew == FIREBALLS)
        soundfirew = 0;
    } while (!sff[n]);
    t2val = soundfirevalue[n] + randnos(soundfirevalue[n] >> 3);
  }
}

bool soundexplodeflag[FIREBALLS] = {FALSE,FALSE}, sef[FIREBALLS];
Sint4 soundexplodevalue[FIREBALLS], soundexplodeduration[FIREBALLS];
int soundexplodew = 0;

void soundexplode(int n)
{
  soundexplodevalue[n] = 1500;
  soundexplodeduration[n] = 10;
  soundexplodeflag[n] = TRUE;
  soundfireoff(n);
}

void soundexplodeoff(int n)
{
  soundexplodeflag[n] = FALSE;
}

void soundexplodeupdate(void)
{
  int n;
  bool f = FALSE;
  for (n = 0; n < FIREBALLS; n++) {
    sef[n] = FALSE;
    if (soundexplodeflag[n]) {
      if (soundexplodeduration[n] != 0) {
        soundexplodevalue[n] = soundexplodevalue[n] - (soundexplodevalue[n] >> 3);
        soundexplodeduration[n]--;
        sef[n] = TRUE;
        f = TRUE;
      }
      else
        soundexplodeflag[n] = FALSE;
    }
  }
  if (f) {
    do {
      n = soundexplodew++;
      if (soundexplodew == FIREBALLS)
        soundexplodew = 0;
    } while (!sef[n]);
    t2val = soundexplodevalue[n];
  }
}

bool soundbonusflag = FALSE;
Sint4 soundbonusn = 0;

void soundbonus(void)
{
  soundbonusflag = TRUE;
}

void soundbonusoff(void)
{
  soundbonusflag = FALSE;
  soundbonusn = 0;
}

void soundbonusupdate(void)
{
  if (soundbonusflag) {
    soundbonusn++;
    if (soundbonusn > 15)
      soundbonusn = 0;
    if (soundbonusn >= 0 && soundbonusn < 6)
      t2val = 0x4ce;
    if (soundbonusn >= 8 && soundbonusn < 14)
      t2val = 0x5e9;
  }
}

bool soundemflag = FALSE;

void soundem(void)
{
  soundemflag = TRUE;
}

void soundemoff(void)
{
  soundemflag = FALSE;
}

void soundemupdate(void)
{
  if (soundemflag) {
    t2val = 1000;
    soundemoff();
  }
}

bool soundemeraldflag = FALSE;
Sint4 soundemeraldduration, emerfreq, soundemeraldn;
static const Sint4 emfreqs[8] = {0x8e8,0x7f0,0x712,0x6ac,0x5f2,0x54c,0x4b8,0x474};

void soundemerald(int n)
{
  emerfreq = emfreqs[n & 7];
  soundemeraldduration = 7;
  soundemeraldn = 0;
  soundemeraldflag = TRUE;
}

void soundemeraldoff(void)
{
  soundemeraldflag = FALSE;
}

void soundemeraldupdate(void)
{
  if (soundemeraldflag) {
    if (soundemeraldduration != 0) {
      if (soundemeraldn == 0 || soundemeraldn == 1)
        t2val = emerfreq;
      soundemeraldn++;
      if (soundemeraldn > 7) {
        soundemeraldn = 0;
        soundemeraldduration--;
      }
    }
    else
      soundemeraldoff();
  }
}

bool soundgoldflag = FALSE, soundgoldf = FALSE;
Sint4 soundgoldvalue1, soundgoldvalue2, soundgoldduration;

void soundgold(void)
{
  soundgoldvalue1 = 500;
  soundgoldvalue2 = 4000;
  soundgoldduration = 30;
  soundgoldf = FALSE;
  soundgoldflag = TRUE;
}

void soundgoldoff(void)
{
  soundgoldflag = FALSE;
}

void soundgoldupdate(void)
{
  if (soundgoldflag) {
    if (soundgoldduration != 0)
      soundgoldduration--;
    else
      soundgoldflag = FALSE;
    if (soundgoldf) {
      soundgoldf = FALSE;
      t2val = soundgoldvalue1;
    }
    else {
      soundgoldf = TRUE;
      t2val = soundgoldvalue2;
    }
    soundgoldvalue1 += (soundgoldvalue1 >> 4);
    soundgoldvalue2 -= (soundgoldvalue2 >> 4);
  }
}

bool soundeatmflag = FALSE;
Sint4 soundeatmvalue, soundeatmduration, soundeatmn;

void soundeatm(void)
{
  soundeatmduration = 20;
  soundeatmn = 3;
  soundeatmvalue = 2000;
  soundeatmflag = TRUE;
}

void soundeatmoff(void)
{
  soundeatmflag = FALSE;
}

void soundeatmupdate(void)
{
  if (soundeatmflag) {
    if (soundeatmn != 0) {
      if (soundeatmduration != 0) {
        if ((soundeatmduration & 3) == 1)
          t2val = soundeatmvalue;
        if ((soundeatmduration & 3) == 3)
          t2val = soundeatmvalue - (soundeatmvalue >> 4);
        soundeatmduration--;
        soundeatmvalue -= (soundeatmvalue >> 4);
      }
      else {
        soundeatmduration = 20;
        soundeatmn--;
        soundeatmvalue = 2000;
      }
    }
    else
      soundeatmflag = FALSE;
  }
}

bool soundddieflag = FALSE;
Sint4 soundddien, soundddievalue;

void soundddie(void)
{
  soundddien = 0;
  soundddievalue = 20000;
  soundddieflag = TRUE;
}

void soundddieoff(void)
{
  soundddieflag = FALSE;
}

void soundddieupdate(void)
{
  if (soundddieflag) {
    soundddien++;
    if (soundddien == 1)
      musicoff();
    if (soundddien >= 1 && soundddien <= 10)
      soundddievalue = 20000 - soundddien * 1000;
    if (soundddien > 10)
      soundddievalue += 500;
    if (soundddievalue > 30000)
      soundddieoff();
    t2val = soundddievalue;
  }
}

bool sound1upflag = FALSE;
Sint4 sound1upduration = 0;

void sound1up(void)
{
  sound1upduration = 96;
  sound1upflag = TRUE;
}

void sound1upoff(void)
{
  sound1upflag = FALSE;
}

void sound1upupdate(void)
{
  if (sound1upflag) {
    if ((sound1upduration / 3) & 1)
      t2val = (sound1upduration << 2) + 600;
    sound1upduration--;
    if (sound1upduration < 1)
      sound1upflag = FALSE;
  }
}

static bool elks_sfx_active(void)
{
  int n;

  if (soundemeraldflag || soundwobbleflag || soundddieflag ||
      soundbreakflag || soundgoldflag || soundemflag ||
      soundeatmflag || soundfallflag || sound1upflag ||
      soundbonusflag)
    return TRUE;

  for (n = 0; n < FIREBALLS; n++)
    if (soundfireflag[n] || soundexplodeflag[n])
      return TRUE;

  return FALSE;
}

bool musicplaying = FALSE;
Sint4 musicp = 0, tuneno = 0, noteduration = 0, notevalue = 0,
      musicnotewidth = 0, musicn = 0;

void music(Sint4 tune)
{
  tuneno = tune;
  musicp = 0;
  noteduration = 0;
  musicplaying = TRUE;
  if (tune == 2)
    soundddieoff();
}

void musicoff(void)
{
  musicplaying = FALSE;
  musicp = 0;
}
static const Uint4 bonusfreqs[10] = {
  0x11d1, 0xd59, 0xbe4, 0xa98, 0xe24, 0x8e8, 0xa00, 0x7f0,
  0xfdf, 0x970
};
static const Uint3 bonusdurs[3] = {
  2, 4, 10
};
static const Uint3 bonusjingle[161] = {
  0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x11, 0x21, 0x31, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x10,
  0x30, 0x21, 0x41, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x11, 0x21, 0x31, 0x10, 0x30, 0x52,
  0x60, 0x30, 0x20, 0x11, 0x31, 0x11, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x11, 0x21, 0x31, 0x00,
  0x00, 0x01, 0x00, 0x00, 0x01, 0x10, 0x30, 0x21, 0x41, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00,
  0x11, 0x21, 0x31, 0x10, 0x30, 0x52, 0x60, 0x30, 0x20, 0x11, 0x31, 0x11, 0x30, 0x30, 0x31, 0x30, 0x30, 0x31,
  0x30, 0x30, 0x31, 0x71, 0x31, 0x71, 0x31, 0x71, 0x31, 0x21, 0x11, 0x41, 0x81, 0x30, 0x30, 0x31, 0x30, 0x30,
  0x31, 0x30, 0x30, 0x31, 0x71, 0x31, 0x71, 0x31, 0x71, 0x51, 0x91, 0x51, 0x91, 0x51, 0x30, 0x30, 0x31, 0x30,
  0x30, 0x31, 0x30, 0x30, 0x31, 0x71, 0x31, 0x71, 0x31, 0x71, 0x31, 0x21, 0x11, 0x41, 0x81, 0x30, 0x30, 0x31,
  0x30, 0x30, 0x31, 0x30, 0x30, 0x31, 0x71, 0x31, 0x71, 0x31, 0x71, 0x51, 0x91, 0x51, 0x91, 0x51, 0xff
};
static const Uint4 backgfreqs[13] = {
  0xfdf, 0x11d1, 0x1530, 0x1ab2, 0x1fbf, 0xe24, 0xd59, 0x1400,
  0xa98, 0xbe4, 0x970, 0x8e8, 0x7f0
};
static const Uint3 backgdurs[2] = {
  2, 4
};
static const Uint3 backgjingle[146] = {
  0x00, 0x10, 0x00, 0x20, 0x30, 0x20, 0x41, 0x00, 0x10, 0x00, 0x20, 0x30, 0x20, 0x41, 0x00, 0x50, 0x60, 0x50,
  0x60, 0x00, 0x50, 0x00, 0x50, 0x10, 0x00, 0x10, 0x00, 0x70, 0x01, 0x00, 0x10, 0x00, 0x20, 0x30, 0x20, 0x41,
  0x00, 0x10, 0x00, 0x20, 0x30, 0x20, 0x41, 0x00, 0x50, 0x60, 0x50, 0x60, 0x00, 0x50, 0x00, 0x50, 0x10, 0x00,
  0x10, 0x00, 0x50, 0x61, 0x80, 0x90, 0x80, 0x60, 0x10, 0x60, 0x21, 0x80, 0x90, 0x80, 0x60, 0x10, 0x60, 0x21,
  0x80, 0xa0, 0xb0, 0xa0, 0xb0, 0x80, 0xa0, 0x80, 0xa0, 0x90, 0x80, 0x90, 0x80, 0x60, 0x81, 0x80, 0x90, 0x80,
  0x60, 0x10, 0x60, 0x21, 0x80, 0x90, 0x80, 0x60, 0x10, 0x60, 0x21, 0x80, 0xa0, 0xb0, 0xa0, 0xb0, 0x80, 0xa0,
  0x80, 0xa0, 0x90, 0x80, 0x90, 0x80, 0x60, 0x81, 0xc0, 0xb0, 0x80, 0x60, 0x10, 0x60, 0x21, 0x80, 0x90, 0x80,
  0x60, 0x10, 0x60, 0x21, 0x80, 0xa0, 0xb0, 0xa0, 0xb0, 0x80, 0xa0, 0x80, 0xa0, 0x90, 0x80, 0x90, 0x60, 0x90,
  0x81, 0xff
};
static const Uint4 dirgefreqs[5] = {
  0x7d00, 0x11d1, 0xefb, 0xfdf, 0x12e0
};
static const Uint3 dirgedurs[5] = {
  2, 6, 4, 12, 16
};
static const Uint3 dirge[25] = {
  0x00, 0x11, 0x12, 0x10, 0x11, 0x22, 0x30, 0x32, 0x10, 0x12, 0x40, 0x13, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
  0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0xff
};


static void musicread(const Uint3 *seq,const Uint4 *freqs,const Uint3 *durs,
                      Sint4 durmul,Sint4 widthsub,Sint4 fixedwidth)
{
  Uint3 e=seq[musicp++];
  if (e==0xff) {
    musicp=0;
    e=seq[musicp++];
  }
  if (seq[musicp]==0xff)
    musicp=0;
  noteduration=(Sint4)durs[e&15]*durmul;
  if (fixedwidth>=0)
    musicnotewidth=fixedwidth;
  else
    musicnotewidth=noteduration-widthsub;
  notevalue=freqs[e>>4];
}

void musicupdate(void)
{
  if (!musicplaying)
    return;
  if (noteduration!=0)
    noteduration--;
  else {
    musicn=0;
    switch (tuneno) {
      case 0:
        musicread(bonusjingle,bonusfreqs,bonusdurs,3,3,-1);
        break;
      case 1:
        musicread(backgjingle,backgfreqs,backgdurs,6,0,12);
        /* Timer-2 Popcorn sounds better with explicit note separation,
         * because PC speaker has no real volume control.  DOS used
         * pulse width for volume; ELKS uses rests/articulation instead.
         */
        if (noteduration <= 12)
          musicnotewidth = ELKS_POPCORN_SHORT_WIDTH;
        else
          musicnotewidth = ELKS_POPCORN_LONG_WIDTH;
        break;
      case 2:
        musicread(dirge,dirgefreqs,dirgedurs,10,10,-1);
        break;
    }
  }
  musicn++;

  /* All music() tunes now use the good-sounding ELKS renderer: clean
   * PIT channel-2 notes plus note-width rests.  SFX priority is handled
   * in soundint() after all effect updates.
   */
  if (musicn < musicnotewidth && notevalue != 0x7d00)
    t2val = notevalue;
  else
    t2val = 40;
}



void soundpause(void)
{
  soundpausedflag=TRUE;
  elks_pcspk_force_off();
}

void soundpauseoff(void)
{
  soundpausedflag=FALSE;
}

void setsoundt2(void)
{
}

void startint8(void)
{
  timerrate = 0x4000;
}

void stopint8(void)
{
  elks_pcspk_force_off();
}

void initsound(void)
{
  elks_pcspk_force_off();
  sndflag = TRUE;
  soundstop();
  setupsound();
  timerrate = 0x4000;
  randvs = getlrt();
}

void elks_sound_kill(void)
{
  stopint8();
}

void elks_sound_setup(void)
{
  inittimer();
  curtime = 0;
  startint8();
}
