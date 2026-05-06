/*
 * ELKS Digger port support code.
 *
 * Copyright (C) 2026 Denis Vasilyev <Vutshi>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
/* Minimal ELKS hardware/timer shim for the Digger CGA-only port.
 *
 * This file deliberately excludes video code.  CGA drawing lives in
 * elks_cga.c so that the packed framebuffer backend can be reviewed and
 * replaced by 8086 assembly independently of keyboard/timer bring-up.
 */

#include <unistd.h>
#include <sys/time.h>
#include "def.h"
#include "hardware.h"

#define DIGGER_TICKS_PER_MS 1193ul

static struct timeval timer_base;
static bool timer_started;
static Uint5 fallback_counter;

void olddelay(Sint4 t)
{
  if (t <= 0)
    return;
  usleep((unsigned long)t * 1000ul);
}

Sint5 getkips(void)
{
  return 291;
}

void inittimer(void)
{
  if (gettimeofday(&timer_base, 0) == 0) {
    timer_started = TRUE;
    fallback_counter = 0;
  }
  else {
    timer_started = FALSE;
    fallback_counter = 0;
  }
}

Uint5 gethrt(void)
{
  struct timeval now;
  unsigned long sec;
  long usec;
  unsigned long ms;

  if (!timer_started)
    inittimer();

  if (timer_started && gettimeofday(&now, 0) == 0) {
    sec = (unsigned long)(now.tv_sec - timer_base.tv_sec);
    usec = now.tv_usec - timer_base.tv_usec;
    if (usec < 0) {
      usec += 1000000L;
      sec--;
    }
    /*
     * Digger pacing sleeps in whole milliseconds, so sub-ms PIT
     * conversion is wasted here and costs extra long division/modulo
     * on 8086-class targets.
     */
    ms = sec * 1000ul + (unsigned long)(usec / 1000L);
    return (Uint5)(ms * DIGGER_TICKS_PER_MS);
  }

  fallback_counter += 20000ul;
  return fallback_counter;
}

Sint5 getlrt(void)
{
  return (Sint5)gethrt();
}
