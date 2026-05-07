/* ELKS Digger deterministic replay support.
 *
 * Compact streaming binary format, independent of host keyboard timing.
 */
#include <stdio.h>
#include <string.h>
#include "def.h"
#include "main.h"
#include "digger.h"
#include "input.h"
#include "scores.h"
#include "replay.h"

#define REPLAY_MAGIC0 'E'
#define REPLAY_MAGIC1 'D'
#define REPLAY_MAGIC2 'R'
#define REPLAY_MAGIC3 '1'
#define REPLAY_VERSION 1u
#define REPLAY_HEADER_SIZE (24u + (8u * MHEIGHT * MWIDTH))
#define REPLAY_EOB 0xffffu

#define REPLAY_MODE_NONE   0
#define REPLAY_MODE_RECORD 1
#define REPLAY_MODE_PLAY   2

#define REPLAY_FLAG_GAUNTLET 0x0001u
#define REPLAY_FLAG_UNLIM    0x0002u

static FILE *replay_file;
static char replay_name[FILENAME_BUFFER_SIZE];
static int replay_mode;
static int replay_header_done;
static int replay_block_open;
static int replay_error;
static int replay_fast_flag;
static unsigned int replay_pending_events;
static unsigned int replay_run_mask;
static unsigned int replay_run_count;
static int replay_have_run;

static void replay_copy_name(char *name)
{
  int i;

  i = 0;
  while (name[i] != 0 && i < (FILENAME_BUFFER_SIZE - 1)) {
    replay_name[i] = name[i];
    i++;
  }
  replay_name[i] = 0;
}

static void replay_fail(void)
{
  replay_error = TRUE;
  escape = TRUE;
}

static void replay_put_byte(unsigned int v)
{
  if (replay_error)
    return;
  if (fputc((int)(v & 0xffu), replay_file) == EOF)
    replay_fail();
}

static int replay_get_byte(void)
{
  int c;

  if (replay_error)
    return -1;
  c = fgetc(replay_file);
  if (c == EOF) {
    replay_fail();
    return -1;
  }
  return c & 0xff;
}

static void replay_put_u16(unsigned int v)
{
  replay_put_byte(v);
  replay_put_byte(v >> 8);
}

static unsigned int replay_get_u16(void)
{
  unsigned int lo, hi;

  lo = (unsigned int)replay_get_byte();
  hi = (unsigned int)replay_get_byte();
  return lo | (hi << 8);
}

static void replay_put_u32(Uint5 v)
{
  replay_put_byte((unsigned int)v);
  replay_put_byte((unsigned int)(v >> 8));
  replay_put_byte((unsigned int)(v >> 16));
  replay_put_byte((unsigned int)(v >> 24));
}

static Uint5 replay_get_u32(void)
{
  Uint5 b0, b1, b2, b3;

  b0 = (Uint5)replay_get_byte();
  b1 = (Uint5)replay_get_byte();
  b2 = (Uint5)replay_get_byte();
  b3 = (Uint5)replay_get_byte();
  return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

static void replay_write_levels(void)
{
  int l, y, x;

  for (l = 0; l < 8; l++)
    for (y = 0; y < MHEIGHT; y++)
      for (x = 0; x < MWIDTH; x++)
        replay_put_byte((unsigned int)(unsigned char)leveldat[l][y][x]);
}

static void replay_read_levels(void)
{
  int l, y, x;

  for (l = 0; l < 8; l++)
    for (y = 0; y < MHEIGHT; y++)
      for (x = 0; x < MWIDTH; x++)
        leveldat[l][y][x] = (Sint3)replay_get_byte();
}

static unsigned int replay_flags_from_game(void)
{
  unsigned int flags;

  flags = 0;
  if (gauntlet)
    flags |= REPLAY_FLAG_GAUNTLET;
  if (unlimlives)
    flags |= REPLAY_FLAG_UNLIM;
  return flags;
}

static void replay_apply_flags(unsigned int flags)
{
  gauntlet = (flags & REPLAY_FLAG_GAUNTLET) ? TRUE : FALSE;
  unlimlives = (flags & REPLAY_FLAG_UNLIM) ? TRUE : FALSE;
}

void replay_set_record_name(char *name)
{
  replay_copy_name(name);
  replay_mode = REPLAY_MODE_RECORD;
}

void replay_set_play_name(char *name)
{
  replay_copy_name(name);
  replay_mode = REPLAY_MODE_PLAY;
}

void replay_set_fast(void)
{
  replay_fast_flag = TRUE;
}

int replay_is_recording(void)
{
  return replay_mode == REPLAY_MODE_RECORD && replay_header_done && !replay_error;
}

int replay_is_playing(void)
{
  return replay_mode == REPLAY_MODE_PLAY && replay_header_done && !replay_error;
}

int replay_fast(void)
{
  return replay_is_playing() && replay_fast_flag;
}

int replay_has_request(void)
{
  return replay_mode != REPLAY_MODE_NONE;
}

int replay_exit_status(void)
{
  return replay_error ? 1 : 0;
}

void replay_record_header(void)
{
  unsigned int flags;

  if (replay_mode != REPLAY_MODE_RECORD || replay_header_done)
    return;

  replay_file = fopen(replay_name, "wb");
  if (replay_file == NULL) {
    replay_fail();
    return;
  }

  flags = replay_flags_from_game();

  replay_put_byte(REPLAY_MAGIC0);
  replay_put_byte(REPLAY_MAGIC1);
  replay_put_byte(REPLAY_MAGIC2);
  replay_put_byte(REPLAY_MAGIC3);
  replay_put_u16(REPLAY_VERSION);
  replay_put_u16(REPLAY_HEADER_SIZE);
  replay_put_u16(flags);
  replay_put_u16((unsigned int)nplayers);
  replay_put_u16((unsigned int)diggers);
  replay_put_u16((unsigned int)startlev);
  replay_put_u16((unsigned int)gtime);
  replay_put_u32(ftime);
  replay_put_u16((unsigned int)bonusscore);
  replay_write_levels();

  if (!replay_error)
    replay_header_done = TRUE;
}

int replay_play_header(void)
{
  unsigned int version, header_size, flags;

  if (replay_mode != REPLAY_MODE_PLAY)
    return FALSE;
  if (replay_header_done)
    return TRUE;

  replay_file = fopen(replay_name, "rb");
  if (replay_file == NULL) {
    replay_fail();
    return FALSE;
  }

  if (replay_get_byte() != REPLAY_MAGIC0 ||
      replay_get_byte() != REPLAY_MAGIC1 ||
      replay_get_byte() != REPLAY_MAGIC2 ||
      replay_get_byte() != REPLAY_MAGIC3) {
    replay_fail();
    return FALSE;
  }

  version = replay_get_u16();
  header_size = replay_get_u16();
  if (version != REPLAY_VERSION || header_size != REPLAY_HEADER_SIZE) {
    replay_fail();
    return FALSE;
  }

  flags = replay_get_u16();
  nplayers = (Sint4)replay_get_u16();
  diggers = (Sint4)replay_get_u16();
  startlev = (Sint4)replay_get_u16();
  gtime = (int)replay_get_u16();
  ftime = replay_get_u32();
  bonusscore = (Uint4)replay_get_u16();
  replay_read_levels();
  replay_apply_flags(flags);

  if (nplayers < 1 || nplayers > 2 || diggers < 1 || diggers > 2 ||
      startlev < 1 || startlev > 1000 || ftime == 0) {
    replay_fail();
    return FALSE;
  }

  replay_header_done = TRUE;
  return TRUE;
}

static void replay_flush_run(void)
{
  if (!replay_have_run)
    return;
  replay_put_u16(replay_run_mask);
  replay_put_byte(replay_run_count);
  replay_have_run = FALSE;
  replay_run_count = 0;
}

void replay_begin_block(Uint5 seed)
{
  unsigned int cp, level;

  if (replay_mode != REPLAY_MODE_RECORD || !replay_header_done || replay_error)
    return;

  cp = (unsigned int)curplayer;
  level = (unsigned int)levno();
  replay_put_byte('B');
  replay_put_u32(seed);
  replay_put_u16(cp);
  replay_put_u16(level);
  replay_block_open = TRUE;
  replay_have_run = FALSE;
  replay_run_count = 0;
  replay_pending_events = 0;
}

Uint5 replay_play_seed(void)
{
  Uint5 seed;
  unsigned int cp, level;

  if (replay_mode != REPLAY_MODE_PLAY || !replay_header_done || replay_error)
    return 0;

  if (replay_get_byte() != 'B') {
    replay_fail();
    return 0;
  }

  seed = replay_get_u32();
  cp = replay_get_u16();
  level = replay_get_u16();
  if (cp != (unsigned int)curplayer || level != (unsigned int)levno()) {
    replay_fail();
    return 0;
  }

  replay_block_open = TRUE;
  replay_run_count = 0;
  replay_pending_events = 0;
  return seed;
}

void replay_end_block(void)
{
  unsigned int marker;

  if (replay_mode == REPLAY_MODE_RECORD && replay_header_done && replay_block_open) {
    replay_flush_run();
    replay_put_u16(REPLAY_EOB);
  }
  else if (replay_mode == REPLAY_MODE_PLAY && replay_header_done && replay_block_open) {
    if (replay_run_count != 0) {
      replay_fail();
      return;
    }
    marker = replay_get_u16();
    if (marker != REPLAY_EOB)
      replay_fail();
  }

  replay_block_open = FALSE;
  replay_have_run = FALSE;
  replay_run_count = 0;
  replay_pending_events = 0;
}

void replay_finish(void)
{
  int c;

  if (replay_mode == REPLAY_MODE_RECORD && replay_header_done && replay_file != NULL) {
    if (replay_block_open)
      replay_end_block();
    replay_put_byte('G');
  }
  else if (replay_mode == REPLAY_MODE_PLAY && replay_header_done && replay_file != NULL && !replay_error) {
    c = replay_get_byte();
    if (c != 'G')
      replay_fail();
  }

  if (replay_file != NULL) {
    fclose(replay_file);
    replay_file = NULL;
  }
  replay_header_done = FALSE;
  replay_block_open = FALSE;
}

void replay_note_event(unsigned int mask)
{
  if (replay_mode == REPLAY_MODE_RECORD && replay_header_done && replay_block_open)
    replay_pending_events |= mask;
}

static void replay_record_frame(void)
{
  unsigned int mask;

  if (!replay_block_open)
    return;

  mask = replay_capture_input() | replay_pending_events;
  replay_pending_events = 0;

  if (replay_have_run && replay_run_mask == mask && replay_run_count < 255u) {
    replay_run_count++;
    return;
  }

  replay_flush_run();
  replay_run_mask = mask;
  replay_run_count = 1;
  replay_have_run = TRUE;
}

static void replay_apply_frame_events(unsigned int mask)
{
  if (mask & REPLAY_MASK_SPEEDUP) {
    if (ftime > 10000l)
      ftime -= 10000l;
  }
  if (mask & REPLAY_MASK_SPEEDDN)
    ftime += 10000l;
  if (mask & REPLAY_MASK_ESCAPE)
    escape = TRUE;
}

static void replay_play_frame(void)
{
  unsigned int mask;

  if (!replay_block_open || replay_error)
    return;

  if (replay_run_count == 0) {
    mask = replay_get_u16();
    if (mask == REPLAY_EOB) {
      replay_fail();
      return;
    }
    replay_run_mask = mask;
    replay_run_count = (unsigned int)replay_get_byte();
    if (replay_run_count == 0) {
      replay_fail();
      return;
    }
  }

  mask = replay_run_mask;
  replay_run_count--;
  replay_inject_input(mask);
  replay_apply_frame_events(mask);
}

void replay_frame_boundary(void)
{
  if (replay_mode == REPLAY_MODE_RECORD && replay_header_done && !replay_error)
    replay_record_frame();
  else if (replay_mode == REPLAY_MODE_PLAY && replay_header_done && !replay_error)
    replay_play_frame();
}
