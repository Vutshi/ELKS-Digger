/*
 * ELKS Digger port support code.
 *
 * Copyright (C) 2026 Denis Vasilyev <Vutshi>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef DIGGER_CONFIG_ELKS_H
#define DIGGER_CONFIG_ELKS_H

/* Single ELKS/gcc-ia16 build profile. */
#define DIGGER_ELKS 1
#define DIGGER_CGA_ONLY 1
/* ELKS keeps DRF recording/playback compiled out through record.h macros. */
#define DIGGER_RECORD_STUB 1

/* The original Borland sources use memory model keywords in generated files. */
#ifndef near
#define near
#endif
#ifndef far
#ifdef __ia16__
#define far __far
#else
#define far
#endif
#endif
#ifndef huge
#define huge far
#endif


#endif /* DIGGER_CONFIG_ELKS_H */
