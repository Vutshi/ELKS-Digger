/*
 * ELKS Digger port support code.
 *
 * Copyright (C) 2026 Denis Vasilyev <Vutshi>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
/* ELKS direct/stub sound implementation for the Digger CGA-only port.
 *
 * This keeps the original game-facing sound API but does not restore the
 * original DOS timer interrupt sound engine.  Effect entry points only set
 * tiny state.  The existing game loop advances sound by calling soundint().
 *
 * Stage 6E: the direct speaker backend consumes PIT channel-2 divisors
 * directly.  There is no Hz-to-divisor division in soundint(), and repeated
 * writes to the PIT are skipped when the divisor has not changed.
 *
 * Build modes:
 *   make          -> direct ELKS PC speaker backend
 *   make SOUND=0  -> silent stub backend
 */

#include "def.h"
#include "sound.h"
#include "hardware.h"
#include "digger.h"

#ifdef DIGGER_CGA_PROFILE
unsigned int elks_soundint_count;
#define SOUND_PROF_TICK() (++elks_soundint_count)
#else
#define SOUND_PROF_TICK() ((void)0)
#endif

#if defined(DIGGER_ELKS_PCSPK_DIRECT) && !defined(DIGGER_SOUND_STUB)
/* Match elkscmd/sys_utils/beep.c: use ELKS' configured I/O helpers and
 * port names, instead of hand-written inline assembly or KIOCSOUND ioctl.
 */
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

static void noop_void(void) {}
static void noop_u16(Uint4 v) { (void)v; }
static void backend_soundoff(void);

bool soundflag = TRUE;
bool musicflag = TRUE;
Sint4 volume = 1;
Sint4 timerrate = 0x7d0;
Uint4 timercount = 0;


void (*setupsound)(void) = noop_void;
void (*killsound)(void) = backend_soundoff;
void (*fillbuffer)(void) = noop_void;
void (*initint8)(void) = noop_void;
void (*restoreint8)(void) = noop_void;
void (*soundoff)(void) = backend_soundoff;
void (*setspkrt2)(void) = noop_void;
void (*settimer0)(Uint4 t0v) = noop_u16;
void (*timer0)(Uint4 t0v) = noop_u16;
void (*settimer2)(Uint4 t2v) = noop_u16;
void (*timer2)(Uint4 t2v) = noop_u16;
void (*soundkillglob)(void) = backend_soundoff;

#if defined(DIGGER_ELKS_PCSPK_DIRECT) && !defined(DIGGER_SOUND_STUB)

#define ELKS_TIMER2_MODE3        0xb6u
#define ELKS_PCSPK_MIN_DIV       64u
#define ELKS_PCSPK_MAX_DIV       30000u
#define ELKS_PCSPK_REST_DIV      0x7d00u
#define ELKS_MUSIC_TICKS         7u

#define ARRAYSIZE(a) ((unsigned char)(sizeof(a) / sizeof((a)[0])))

static unsigned char speaker_on;
static unsigned short last_divisor;
static unsigned char sound_paused;

static unsigned char level_on;
static unsigned char level_pos;
static unsigned char level_ticks;

static unsigned char music_on;
static unsigned char music_tune;
static unsigned char music_ticks;
static unsigned char music_pos;

static unsigned char bonus_on;
static unsigned char bonus_phase;

static unsigned char emerald_ticks;
static unsigned char emerald_phase;
static unsigned short emerald_div;

static unsigned char em_click_ticks;

static unsigned char wobble_on;
static unsigned char wobble_phase;

static unsigned char death_on;
static unsigned char death_ticks;
static unsigned char death_phase;
static unsigned short death_div;

static unsigned char break_ticks;
static unsigned short break_div;

static unsigned char gold_ticks;
static unsigned char gold_phase;
static unsigned short gold_div1;
static unsigned short gold_div2;

static unsigned char eatm_ticks;
static unsigned short eatm_div;

static unsigned char fall_on;
static unsigned char fall_phase;
static unsigned short fall_div;

static unsigned char oneup_ticks;
static unsigned char oneup_phase;
static unsigned char oneup_sub;

static unsigned char fire_on[FIREBALLS];
static unsigned char fire_wait[FIREBALLS];
static unsigned short fire_div[FIREBALLS];
static unsigned char fire_w;

static unsigned char explode_on[FIREBALLS];
static unsigned char explode_ticks[FIREBALLS];
static unsigned short explode_div[FIREBALLS];
static unsigned char explode_w;

static unsigned short noise = 0x1234u;

/* Original Digger divisor values, kept small and local. */
static const unsigned short emerald_divs[8] = {
  0x8e8u, 0x7f0u, 0x712u, 0x6acu, 0x5f2u, 0x54cu, 0x4b8u, 0x474u
};

static const unsigned short level_divs[11] = {
  0x8e8u, 0x712u, 0x5f2u, 0x7f0u, 0x6acu, 0x54cu,
  0x712u, 0x5f2u, 0x4b8u, 0x474u, 0x474u
};

static const unsigned short wobble_divs[4] = {
  0x7d0u, 0x9c4u, 0xbb8u, 0x9c4u
};

static const unsigned short music_divs[8] = {
  0xfdfu, 0x11d1u, 0xfdfu, 0x1530u, 0x1ab2u, 0x1530u, 0x1fbfu, 0xfdfu
};

static void pcspk_force_off(void)
{
#ifdef CONFIG_ARCH_IBMPC
  if (speaker_on) {
    unsigned int tmp = inb(SPEAKER_PORT) & 0xfcu;
    outb(tmp, SPEAKER_PORT);
  }
#endif
  speaker_on = 0;
  last_divisor = 0;
}

static void pcspk_set_divisor_cached(unsigned short divisor)
{
#ifdef CONFIG_ARCH_IBMPC
  unsigned int tmp;
#endif

  if (divisor == 0 || divisor < ELKS_PCSPK_MIN_DIV ||
      divisor > ELKS_PCSPK_MAX_DIV || divisor == ELKS_PCSPK_REST_DIV) {
    pcspk_force_off();
    return;
  }

  if (speaker_on && divisor == last_divisor)
    return;

#ifdef CONFIG_ARCH_IBMPC
  outb(ELKS_TIMER2_MODE3, TIMER_CMDS_PORT);
  outb((unsigned int)divisor, TIMER2_PORT);
  outb((unsigned int)(divisor >> 8), TIMER2_PORT);

  tmp = inb(SPEAKER_PORT);
  if (tmp != (tmp | 3u))
    outb(tmp | 3u, SPEAKER_PORT);
  speaker_on = 1;
  last_divisor = divisor;
#else
  (void)divisor;
  speaker_on = 0;
  last_divisor = 0;
#endif
}

static void backend_soundoff(void)
{
  pcspk_set_divisor_cached(0);
}

static void clear_effect_state(void)
{
  unsigned char i;

  level_on = level_pos = level_ticks = 0;
  bonus_on = bonus_phase = 0;
  emerald_ticks = emerald_phase = 0;
  emerald_div = 0;
  em_click_ticks = 0;
  wobble_on = wobble_phase = 0;
  death_on = death_ticks = death_phase = 0;
  death_div = 0;
  break_ticks = 0;
  break_div = 0;
  gold_ticks = gold_phase = 0;
  gold_div1 = gold_div2 = 0;
  eatm_ticks = 0;
  eatm_div = 0;
  fall_on = fall_phase = 0;
  fall_div = 0;
  oneup_ticks = oneup_phase = oneup_sub = 0;
  fire_w = explode_w = 0;

  for (i = 0; i < FIREBALLS; i++) {
    fire_on[i] = 0;
    fire_wait[i] = 0;
    fire_div[i] = 0;
    explode_on[i] = 0;
    explode_ticks[i] = 0;
    explode_div[i] = 0;
  }
}

static unsigned short tick_noise(void)
{
  noise ^= (unsigned short)(noise << 7);
  noise ^= (unsigned short)(noise >> 9);
  noise ^= (unsigned short)(noise << 8);
  return noise;
}

static unsigned short update_level(unsigned short div)
{
  if (!level_on)
    return div;

  div = level_divs[level_pos];
  if (level_ticks != 0)
    level_ticks--;

  if (level_ticks == 0) {
    level_pos++;
    if (level_pos < ARRAYSIZE(level_divs))
      level_ticks = 3;
    else
      level_on = 0;
  }

  return div;
}

static unsigned short update_music(unsigned short div)
{
  if (!music_on || !musicflag)
    return div;

  if (music_ticks != 0)
    music_ticks--;
  else {
    music_ticks = ELKS_MUSIC_TICKS;
    music_pos = (unsigned char)((music_pos + 1u) & 7u);
  }

  return music_divs[(music_pos + music_tune) & 7u];
}

static unsigned short update_emerald(unsigned short div)
{
  if (emerald_ticks == 0)
    return div;

  if ((emerald_phase & 3u) < 2u)
    div = emerald_div;
  emerald_phase++;
  emerald_ticks--;
  return div;
}

static unsigned short update_wobble(unsigned short div)
{
  if (!wobble_on)
    return div;

  wobble_phase = (unsigned char)((wobble_phase + 1u) & 63u);
  return wobble_divs[(wobble_phase >> 4) & 3u];
}

static unsigned short update_death(unsigned short div)
{
  if (!death_on)
    return div;

  if (death_ticks == 0) {
    death_on = 0;
    return div;
  }

  div = death_div;
  if (death_phase < 4u) {
    if (death_div > 4000u)
      death_div -= 2500u;
    death_phase++;
  }
  else
    death_div += 1200u;

  death_ticks--;
  if (death_div > ELKS_PCSPK_MAX_DIV)
    death_on = 0;

  return div;
}

static unsigned short update_break(unsigned short div)
{
  if (break_ticks == 0)
    return div;

  break_ticks--;
  return break_div;
}

static unsigned short update_gold(unsigned short div)
{
  if (gold_ticks == 0)
    return div;

  gold_ticks--;
  if (gold_phase) {
    div = gold_div1;
    gold_div1 += (unsigned short)(gold_div1 >> 4);
  }
  else {
    div = gold_div2;
    gold_div2 -= (unsigned short)(gold_div2 >> 4);
  }
  gold_phase ^= 1u;
  return div;
}

static unsigned short update_em_click(unsigned short div)
{
  if (em_click_ticks == 0)
    return div;

  em_click_ticks--;
  return 1000u;
}

static unsigned short update_explode(unsigned short div)
{
  unsigned char i;
  unsigned char n;

  for (i = 0; i < FIREBALLS; i++) {
    if (explode_on[i]) {
      if (explode_ticks[i] != 0) {
        explode_div[i] -= (unsigned short)(explode_div[i] >> 3);
        explode_ticks[i]--;
      }
      else
        explode_on[i] = 0;
    }
  }

  for (i = 0; i < FIREBALLS; i++) {
    n = explode_w++;
    if (explode_w == FIREBALLS)
      explode_w = 0;
    if (explode_on[n])
      return explode_div[n];
  }

  return div;
}

static unsigned short update_fire(unsigned short div)
{
  unsigned char i;
  unsigned char n;

  for (i = 0; i < FIREBALLS; i++) {
    if (fire_on[i]) {
      if (fire_wait[i]) {
        fire_wait[i] = 0;
        fire_div[i] += (unsigned short)((fire_div[i] >> 6) + 1u);
        if (fire_div[i] > ELKS_PCSPK_MAX_DIV)
          fire_on[i] = 0;
      }
      else
        fire_wait[i] = 1;
    }
  }

  for (i = 0; i < FIREBALLS; i++) {
    n = fire_w++;
    if (fire_w == FIREBALLS)
      fire_w = 0;
    if (fire_on[n])
      return (unsigned short)(fire_div[n] + (tick_noise() & 0x003fu));
  }

  return div;
}

static unsigned short update_eatm(unsigned short div)
{
  unsigned char phase;

  if (eatm_ticks == 0)
    return div;

  phase = (unsigned char)(eatm_ticks & 3u);
  if (phase == 1u)
    div = eatm_div;
  else if (phase == 3u)
    div = (unsigned short)(eatm_div - (eatm_div >> 4));

  if (phase == 0u && eatm_div > 200u)
    eatm_div -= (unsigned short)(eatm_div >> 4);

  eatm_ticks--;
  return div;
}

static unsigned short update_fall(unsigned short div)
{
  if (!fall_on)
    return div;

  fall_phase ^= 1u;
  if (fall_phase)
    return fall_div;

  if (fall_div < ELKS_PCSPK_MAX_DIV - 50u)
    fall_div += 50u;
  return div;
}

static unsigned short update_oneup(unsigned short div)
{
  if (oneup_ticks == 0)
    return div;

  oneup_sub++;
  if (oneup_sub >= 3u) {
    oneup_sub = 0;
    oneup_phase ^= 1u;
  }

  if (oneup_phase)
    div = (unsigned short)(600u + ((unsigned short)oneup_ticks << 2));

  oneup_ticks--;
  return div;
}

static unsigned short update_bonus(unsigned short div)
{
  if (!bonus_on)
    return div;

  bonus_phase = (unsigned char)((bonus_phase + 1u) & 15u);
  if (bonus_phase < 6u)
    return 0x4ceu;
  if (bonus_phase >= 8u && bonus_phase < 14u)
    return 0x5e9u;

  return div;
}

void initsound(void)
{
  inittimer();
  curtime = 0;
  soundflag = TRUE;
  musicflag = TRUE;
  sound_paused = 0;
  music_on = music_tune = music_ticks = music_pos = 0;
  clear_effect_state();
  pcspk_force_off();
}

void soundstop(void)
{
  clear_effect_state();
  music_on = music_ticks = music_pos = 0;
  backend_soundoff();
}

void music(Sint4 tune)
{
  music_tune = (unsigned char)tune & 7u;
  music_ticks = 0;
  music_on = 1;
  if (music_tune == 2u)
    death_on = 0;
}

void musicoff(void)
{
  music_on = 0;
}

void soundlevdone(void)
{
  clear_effect_state();
  level_on = 1;
  level_pos = 0;
  level_ticks = 3;
  sound_paused = 0;
}

void sound1up(void)
{
  oneup_ticks = 24;
  oneup_phase = 1;
  oneup_sub = 0;
}

void soundpause(void)
{
  sound_paused = 1;
  backend_soundoff();
}

void soundpauseoff(void)
{
  sound_paused = 0;
}

void setsoundt2(void) {}
void sett2val(Sint4 t2v) { (void)t2v; }
void startint8(void) {}
void stopint8(void) { backend_soundoff(); }
void soundbonus(void) { bonus_on = 1; }
void soundbonusoff(void) { bonus_on = 0; bonus_phase = 0; }

void soundfire(int n)
{
  if (n >= 0 && n < FIREBALLS) {
    fire_div[n] = 500u;
    fire_wait[n] = 0;
    fire_on[n] = 1;
  }
}

void soundexplode(int n)
{
  if (n >= 0 && n < FIREBALLS) {
    explode_div[n] = 1500u;
    explode_ticks[n] = 10;
    explode_on[n] = 1;
    fire_on[n] = 0;
  }
}

void soundfireoff(int n)
{
  if (n >= 0 && n < FIREBALLS) {
    fire_on[n] = 0;
    fire_wait[n] = 0;
  }
}

void soundem(void)
{
  em_click_ticks = 1;
}

void soundemerald(int emn)
{
  emerald_div = emerald_divs[(unsigned char)emn & 7u];
  emerald_ticks = 10;
  emerald_phase = 0;
}

void soundeatm(void)
{
  eatm_ticks = 20;
  eatm_div = 2000u;
}

void soundddie(void)
{
  musicoff();
  death_on = 1;
  death_ticks = 22;
  death_phase = 0;
  death_div = 20000u;
}

void soundwobble(void)
{
  wobble_on = 1;
}

void soundwobbleoff(void)
{
  wobble_on = 0;
  wobble_phase = 0;
}

void soundfall(void)
{
  fall_div = 1000u;
  fall_phase = 0;
  fall_on = 1;
}

void soundfalloff(void)
{
  fall_on = 0;
  fall_phase = 0;
}

void soundbreak(void)
{
  break_div = 15000u;
  break_ticks = 3;
}

void soundgold(void)
{
  gold_div1 = 500u;
  gold_div2 = 4000u;
  gold_ticks = 12;
  gold_phase = 0;
}

void soundint(void)
{
  unsigned short div = 0;

  SOUND_PROF_TICK();
  timercount++;
  if (!soundflag || sound_paused) {
    backend_soundoff();
    return;
  }

  if (level_on)
    div = update_level(div);
  else {
    div = update_music(div);
    div = update_emerald(div);
    div = update_wobble(div);
    div = update_death(div);
    div = update_break(div);
    div = update_gold(div);
    div = update_em_click(div);
    div = update_explode(div);
    div = update_fire(div);
    div = update_eatm(div);
    div = update_fall(div);
    div = update_oneup(div);
    div = update_bonus(div);
  }

  pcspk_set_divisor_cached(div);
}

#else /* DIGGER_SOUND_STUB or unsupported backend */

static void backend_soundoff(void) {}

void initsound(void)
{
  inittimer();
  curtime = 0;
  soundflag = musicflag = FALSE;
}
void soundstop(void) { backend_soundoff(); }
void music(Sint4 tune) { (void)tune; }
void musicoff(void) {}
void soundlevdone(void) {}
void sound1up(void) {}
void soundpause(void) {}
void soundpauseoff(void) {}
void setsoundt2(void) {}
void sett2val(Sint4 t2v) { (void)t2v; }
void startint8(void) {}
void stopint8(void) { backend_soundoff(); }
void soundbonus(void) {}
void soundbonusoff(void) {}
void soundfire(int n) { (void)n; }
void soundexplode(int n) { (void)n; }
void soundfireoff(int n) { (void)n; }
void soundem(void) {}
void soundemerald(int emn) { (void)emn; }
void soundeatm(void) {}
void soundddie(void) {}
void soundwobble(void) {}
void soundwobbleoff(void) {}
void soundfall(void) {}
void soundfalloff(void) {}
void soundbreak(void) {}
void soundgold(void) {}
void soundint(void) { SOUND_PROF_TICK(); }

#endif
