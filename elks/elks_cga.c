/* ELKS CGA backend for Digger.
 *
 * This preserves the original DOS packed CGA model used by dospc.asm:
 * two bits per pixel, four pixels per byte, 320x200 mode 4 at B800:0000,
 * with CGA's split odd/even scanline layout.
 *
 * Address of pixel byte containing (x,y):
 *   ((y & 1) << 13) + (((y >> 1) * 5) << 4) + (x >> 2)
 *
 * The row*80 part is written as shift/add to avoid 8086 runtime multiply
 * helpers and to avoid a 200-entry row-offset table in the data segment.
 *
 * The hot path blits complete packed bytes.  It intentionally does not draw
 * individual pixels, clip rectangles, allocate large stack objects, or call
 * curses/stdio in rendering functions.
 */

#include "def.h"
#include "hardware.h"
#if defined(DIGGER_CGA_PROFILE) || defined(DIGGER_ELKS_TITLE_BMP)
#include <stdio.h>
#endif

#define CGA_SEGMENT     0xB800u
#define CGA_BYTES       0x4000u
#define CGA_NEXT_FIELD  0x2000u
#define CGA_WRAP_DELTA  0x3FB0u  /* 0x4000 - 80 */
#define CGA_ROWBYTES    80u

#ifdef __ia16__
#define MK_FP(seg,off) \
  ((void __far *)((((unsigned long)(seg)) << 16) | ((unsigned int)(off))))
typedef volatile unsigned char __far *CGA_PTR;
#define CGA_MEM ((CGA_PTR)MK_FP(CGA_SEGMENT, 0))
#else
/* Host fallback for syntax/link checking only. */
typedef volatile unsigned char *CGA_PTR;
static unsigned char cga_shadow[CGA_BYTES];
#define CGA_MEM ((CGA_PTR)cga_shadow)
#endif

#ifdef __ia16__
void elks_cga_clear_asm(void);
void elks_cga_puti2_asm(unsigned int off, const Uint3 *p, unsigned int h);
void elks_cga_puti4_asm(unsigned int off, const Uint3 *p, unsigned int h);
void elks_cga_geti2_asm(unsigned int off, Uint3 *p, unsigned int h);
void elks_cga_geti4_asm(unsigned int off, Uint3 *p, unsigned int h);
void elks_cga_putim2_asm(unsigned int off, const Uint3 *dat, const Uint3 *msk,
                         unsigned int h);
void elks_cga_putim4_asm(unsigned int off, const Uint3 *dat, const Uint3 *msk,
                         unsigned int h);
void elks_cga_putim5_asm(unsigned int off, const Uint3 *dat, const Uint3 *msk,
                         unsigned int h);
void elks_cga_putim6_asm(unsigned int off, const Uint3 *dat, const Uint3 *msk,
                         unsigned int h);
#ifdef DIGGER_ELKS_TITLE_BMP
void elks_cga_title_scanline_asm(unsigned int off, const unsigned char *src);
#endif
#endif

extern Uint3 near *cgatable[];
extern Uint3 near *ascii2cga[];
extern bool biosflag;
extern bool retrflag;

static unsigned char cga_palette;
static bool cga_graphics_active;

#ifdef DIGGER_CGA_PROFILE
unsigned int elks_cga_puti2_calls;
unsigned int elks_cga_puti4_calls;
unsigned int elks_cga_puti_generic_calls;
unsigned int elks_cga_geti2_calls;
unsigned int elks_cga_geti4_calls;
unsigned int elks_cga_geti_generic_calls;
unsigned int elks_cga_putim2_calls;
unsigned int elks_cga_putim4_calls;
unsigned int elks_cga_putim5_calls;
unsigned int elks_cga_putim6_calls;
unsigned int elks_cga_putim_generic_calls;
extern unsigned int elks_newframe_count;
extern unsigned int elks_live_frame_count;
extern unsigned int elks_monai_count;
extern unsigned int elks_soundint_count;
extern unsigned int elks_input_read_count;
extern unsigned int elks_input_queued_count;
extern unsigned int elks_input_dup_count;
extern unsigned int elks_input_overflow_count;
extern unsigned int elks_input_maxq;
extern unsigned int elks_input_reset_count;
extern unsigned int elks_input_discard_count;
extern unsigned int elks_game_start_count;
extern unsigned int elks_penalty_frame_count;
extern unsigned int elks_penalty_max;
extern unsigned int elks_incmont_count;
extern unsigned int elks_pace_wait_count;
extern unsigned int elks_pace_sleep_count;
extern unsigned int elks_pace_wait0_count;
extern unsigned int elks_pace_late_count;
extern unsigned int elks_pace_late2_count;
extern unsigned int elks_pace_back_count;
extern unsigned int elks_pace_sleep_max;
extern unsigned int elks_pace_late_streak_max;
extern unsigned int elks_prof_getis_scan;
extern unsigned int elks_prof_getis_saved;
extern unsigned int elks_prof_putis_scan;
extern unsigned int elks_prof_putis_restored;
extern unsigned int elks_prof_putims_scan;
extern unsigned int elks_prof_putims_redrawn;
extern unsigned int elks_prof_setrdrw_scan;
extern unsigned int elks_prof_bcollides_scan;
extern unsigned int elks_prof_bcollide_calls;
extern unsigned int elks_prof_dobags_calls;
extern unsigned int elks_prof_bag_live_max;
extern unsigned int elks_prof_bag_updates;
extern unsigned int elks_prof_domons_calls;
extern unsigned int elks_prof_monster_slots_scanned;
extern unsigned int elks_prof_monster_updates;
#define CGA_PROF_INC(v) do { if ((v) != 65535u) ++(v); } while (0)
#else
#define CGA_PROF_INC(v) ((void)0)
#endif

static unsigned int cga_mul80(unsigned int row)
{
  /* row * 80 == row * 5 * 16 */
  return (unsigned int)(((row << 2) + row) << 4);
}

static unsigned int cga_offset(Sint4 x, Sint4 y)
{
  unsigned int uy = (unsigned int)y;
  unsigned int row = uy >> 1;

  return (unsigned int)(((uy & 1u) << 13) + cga_mul80(row) +
                        (unsigned int)(x >> 2));
}

static unsigned int cga_next_row(unsigned int off)
{
  off += CGA_NEXT_FIELD;
  if (off >= CGA_BYTES)
    off -= CGA_WRAP_DELTA;
  return off;
}

#ifdef __ia16__
static void bios_video_mode(unsigned char mode)
{
  asm volatile ("int $0x10" : : "a" ((unsigned short)mode));
}

static void bios_palette(unsigned char bh, unsigned char bl)
{
  asm volatile ("int $0x10" : :
                "a" ((unsigned short)0x0b00),
                "b" ((unsigned short)(((unsigned short)bh << 8) | bl)));
}

static unsigned char inb(unsigned short port)
{
  unsigned short v;
  asm volatile ("in %%dx,%%al" : "=a" (v) : "d" (port));
  return (unsigned char)v;
}

static void outb(unsigned short port, unsigned char val)
{
  asm volatile ("out %%al,%%dx" : : "a" ((unsigned short)val), "d" (port));
}

static void outw(unsigned short port, unsigned short val)
{
  asm volatile ("out %%ax,%%dx" : : "a" (val), "d" (port));
}
#else
static void bios_video_mode(unsigned char mode) { (void)mode; }
static void bios_palette(unsigned char bh, unsigned char bl)
{
  (void)bh;
  (void)bl;
}
static unsigned char inb(unsigned short port) { (void)port; return 0; }
static void outb(unsigned short port, unsigned char val)
{
  (void)port;
  (void)val;
}
static void outw(unsigned short port, unsigned short val)
{
  (void)port;
  (void)val;
}
#endif

static void cga_setpal_direct(unsigned char value)
{
  static const unsigned char cgacolours[12] = {
    2, 4, 6, 18, 20, 22, 3, 5, 7, 19, 21, 23
  };
  const unsigned char *p;

  cga_palette = (unsigned char)(value & 3);
  p = &cgacolours[cga_palette * 3];

  /* Original dospc.asm sequence: CGA colour-select register, then attribute
   * controller entries 1..3.  On plain CGA the 0x3c0 part is harmless; on
   * EGA/VGA-compatible adapters it preserves Digger's intended colours.
   */
  outw(0x03d9, (unsigned short)((unsigned short)cga_palette << 4));
  (void)inb(0x03ba);
  (void)inb(0x03da);
  outb(0x03c0, 1);
  outb(0x03c0, p[0]);
  outb(0x03c0, 2);
  outb(0x03c0, p[1]);
  outb(0x03c0, 3);
  outb(0x03c0, p[2]);
  outb(0x03c0, 0x20);
}

void graphicsoff(void)
{
  /* PROFILE builds print counters after returning to text mode.  The ELKS
   * keyboard backend also has an atexit safety cleanup which calls
   * graphicsoff(); do not let that second mode reset erase profile text.
   */
  if (!cga_graphics_active)
    return;

  cga_graphics_active = FALSE;
  bios_video_mode(0x03);
}

void gretrace(void)
{
  if (!retrflag)
    return;

#ifdef __ia16__
  while (inb(0x03da) & 8)
    ;
  while (!(inb(0x03da) & 8))
    ;
#endif
}

void cgainit(void)
{
  cga_palette = 0;
  bios_video_mode(0x04);
  cga_graphics_active = TRUE;
  bios_palette(0, 0);
  bios_palette(1, 0);
}

void cgaclear(void)
{
#ifdef __ia16__
  elks_cga_clear_asm();
#else
  CGA_PTR dst = CGA_MEM;
  unsigned int i;

  for (i = 0; i < CGA_BYTES; i++)
    dst[i] = 0;
#endif
}

void cgapal(Sint4 pal)
{
  cga_palette = (unsigned char)((cga_palette & 0xfd) |
                                ((pal & 1) << 1));
  gretrace();
  if (biosflag)
    bios_palette(1, (unsigned char)(pal & 1));
  else
    cga_setpal_direct(cga_palette);
}

void cgainten(Sint4 inten)
{
  cga_palette = (unsigned char)((cga_palette & 0xfe) | (inten & 1));
  gretrace();
  if (biosflag)
    bios_palette(0, (unsigned char)((inten & 1) << 4));
  else
    cga_setpal_direct(cga_palette);
}

void cgaputi(Sint4 x, Sint4 y, Uint3 *p, Sint4 w, Sint4 h)
{
  unsigned int off;
  Sint4 row;

  if (p == 0 || w <= 0 || h <= 0)
    return;

  off = cga_offset(x, y);
#ifdef __ia16__
  if (w == 2) {
    CGA_PROF_INC(elks_cga_puti2_calls);
    elks_cga_puti2_asm(off, p, (unsigned int)h);
    return;
  }
  if (w == 4) {
    CGA_PROF_INC(elks_cga_puti4_calls);
    elks_cga_puti4_asm(off, p, (unsigned int)h);
    return;
  }
#endif

  CGA_PROF_INC(elks_cga_puti_generic_calls);
  for (row = 0; row < h; row++) {
    CGA_PTR dst = CGA_MEM + off;
    Sint4 col;
    for (col = 0; col < w; col++)
      dst[col] = p[col];
    p += w;
    off = cga_next_row(off);
  }
}

void cgageti(Sint4 x, Sint4 y, Uint3 *p, Sint4 w, Sint4 h)
{
  unsigned int off;
  Sint4 row;

  if (p == 0 || w <= 0 || h <= 0)
    return;

  off = cga_offset(x, y);
#ifdef __ia16__
  if (w == 2) {
    CGA_PROF_INC(elks_cga_geti2_calls);
    elks_cga_geti2_asm(off, p, (unsigned int)h);
    return;
  }
  if (w == 4) {
    CGA_PROF_INC(elks_cga_geti4_calls);
    elks_cga_geti4_asm(off, p, (unsigned int)h);
    return;
  }
#endif

  CGA_PROF_INC(elks_cga_geti_generic_calls);
  for (row = 0; row < h; row++) {
    CGA_PTR src = CGA_MEM + off;
    Sint4 col;
    for (col = 0; col < w; col++)
      p[col] = src[col];
    p += w;
    off = cga_next_row(off);
  }
}

void cgaputim(Sint4 x, Sint4 y, Sint4 ch, Sint4 w, Sint4 h)
{
  Uint3 *dat;
  Uint3 *msk;
  unsigned int off;
  Sint4 row;

  if (w <= 0 || h <= 0 || ch < 0)
    return;

  dat = cgatable[ch * 2];
  msk = cgatable[ch * 2 + 1];
  if (dat == 0 || msk == 0)
    return;

  /* Equivalent to dospc.asm's mask/data compositing: dst=(dst&mask)|data. */
  off = cga_offset(x, y);
#ifdef __ia16__
  if (w == 2) {
    CGA_PROF_INC(elks_cga_putim2_calls);
    elks_cga_putim2_asm(off, dat, msk, (unsigned int)h);
    return;
  }
  if (w == 4) {
    CGA_PROF_INC(elks_cga_putim4_calls);
    elks_cga_putim4_asm(off, dat, msk, (unsigned int)h);
    return;
  }
  if (w == 5) {
    CGA_PROF_INC(elks_cga_putim5_calls);
    elks_cga_putim5_asm(off, dat, msk, (unsigned int)h);
    return;
  }
  if (w == 6) {
    CGA_PROF_INC(elks_cga_putim6_calls);
    elks_cga_putim6_asm(off, dat, msk, (unsigned int)h);
    return;
  }
#endif

  CGA_PROF_INC(elks_cga_putim_generic_calls);
  for (row = 0; row < h; row++) {
    CGA_PTR dst = CGA_MEM + off;
    Sint4 col;
    for (col = 0; col < w; col++)
      dst[col] = (unsigned char)((dst[col] & msk[col]) | dat[col]);
    dat += w;
    msk += w;
    off = cga_next_row(off);
  }
}

Sint4 cgagetpix(Sint4 x, Sint4 y)
{
  return (Sint4)CGA_MEM[cga_offset(x, y)];
}

void cgawrite(Sint4 x, Sint4 y, Sint4 ch, Sint4 c)
{
  Uint3 *glyph;
  unsigned char colour;
  unsigned int off;
  Sint4 row;

  ch -= 32;
  if (ch < 0 || ch >= 0x5f)
    return;

  glyph = ascii2cga[ch];
  if (glyph == 0)
    return;

  colour = (unsigned char)((c & 3) * 0x55);
  off = cga_offset(x, y);
  for (row = 0; row < 12; row++) {
    CGA_PTR dst = CGA_MEM + off;
    dst[0] = (unsigned char)(glyph[0] & colour);
    dst[1] = (unsigned char)(glyph[1] & colour);
    dst[2] = (unsigned char)(glyph[2] & colour);
    glyph += 3;
    off = cga_next_row(off);
  }
}


#ifdef DIGGER_ELKS_TITLE_BMP
static unsigned char cga_title_row[CGA_ROWBYTES];

static unsigned int bmp_get_u16(const unsigned char *p)
{
  return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static unsigned long bmp_get_u32(const unsigned char *p)
{
  return (unsigned long)p[0] | ((unsigned long)p[1] << 8) |
         ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

static unsigned char cga_title_colour(unsigned char c)
{
  c &= 3u;

  /* ctitle.bmp uses DOS/RGBI palette order:
   *   1 = red, 2 = green
   * CGA palette 0 pixel codes are:
   *   1 = green, 2 = red
   */
  if (c == 1u)
    return 2u;
  if (c == 2u)
    return 1u;
  return c;
}

static void cga_title_clear_row(void)
{
  unsigned int i;

  for (i = 0; i < CGA_ROWBYTES; i++)
    cga_title_row[i] = 0;
}

static void cga_title_put_mapped_pixel(unsigned int x, unsigned char colour)
{
  unsigned char shift;
  unsigned char mask;
  unsigned char *dst;

  if (colour == 0 || x >= 320u)
    return;

  shift = (unsigned char)((3u - (x & 3u)) << 1);
  mask = (unsigned char)(3u << shift);
  dst = &cga_title_row[x >> 2];
  dst[0] = (unsigned char)((dst[0] & ~mask) | (colour << shift));
}

static bool cga_title_put_encoded_run(unsigned int *x, unsigned int n,
                                      unsigned char pair)
{
  unsigned int px = *x;
  unsigned char hi = cga_title_colour((unsigned char)((pair >> 4) & 15u));
  unsigned char lo = cga_title_colour((unsigned char)(pair & 15u));
  bool dirty = FALSE;

  if (hi == 0 && lo == 0) {
    *x = (unsigned int)(px + n);
    return FALSE;
  }

  while (n != 0) {
    cga_title_put_mapped_pixel(px, hi);
    dirty |= (hi != 0);
    ++px;
    --n;
    if (n == 0)
      break;
    cga_title_put_mapped_pixel(px, lo);
    dirty |= (lo != 0);
    ++px;
    --n;
  }
  *x = px;
  return dirty;
}

static void cga_title_flush_row(unsigned int y)
{
  unsigned int off;

  if (y >= 200u)
    return;

  off = (unsigned int)(((y & 1u) << 13) + cga_mul80(y >> 1));
#ifdef __ia16__
  elks_cga_title_scanline_asm(off, cga_title_row);
#else
  {
    unsigned int i;
    CGA_PTR dst = CGA_MEM + off;

    for (i = 0; i < CGA_ROWBYTES; i++)
      dst[i] = cga_title_row[i];
  }
#endif
}

static bool cga_load_title_bmp(const char *name)
{
  unsigned char h[54];
  unsigned long offbits;
  unsigned int width, height, bpp, comp;
  unsigned int x, y;
  bool row_dirty;
  FILE *f;

  f = fopen(name, "rb");
  if (f == 0)
    return FALSE;

  if (fread(h, 1, sizeof(h), f) != sizeof(h)) {
    fclose(f);
    return FALSE;
  }

  if (h[0] != 'B' || h[1] != 'M' || bmp_get_u32(h + 14) < 40ul) {
    fclose(f);
    return FALSE;
  }

  offbits = bmp_get_u32(h + 10);
  width = (unsigned int)bmp_get_u32(h + 18);
  height = (unsigned int)bmp_get_u32(h + 22);
  bpp = bmp_get_u16(h + 28);
  comp = (unsigned int)bmp_get_u32(h + 30);

  if (width != 320u || height != 200u || bpp != 4u || comp != 2u) {
    fclose(f);
    return FALSE;
  }

  if (fseek(f, (long)offbits, SEEK_SET) != 0) {
    fclose(f);
    return FALSE;
  }

  x = 0;
  y = height - 1u;
  row_dirty = FALSE;
  cga_title_clear_row();

  for (;;) {
    int a = getc(f);
    int b = getc(f);

    if (a == EOF || b == EOF)
      break;

    if (a != 0) {
      row_dirty |= cga_title_put_encoded_run(&x, (unsigned int)a,
                                             (unsigned char)b);
    }
    else {
      if (b == 0) {
        if (row_dirty)
          cga_title_flush_row(y);
        x = 0;
        row_dirty = FALSE;
        if (y == 0)
          break;
        --y;
        cga_title_clear_row();
      }
      else if (b == 1) {
        if (row_dirty)
          cga_title_flush_row(y);
        fclose(f);
        return TRUE;
      }
      else if (b == 2) {
        int dx = getc(f);
        int dy = getc(f);
        if (dx == EOF || dy == EOF)
          break;
        if (dy != 0) {
          if (row_dirty)
            cga_title_flush_row(y);
          row_dirty = FALSE;
          cga_title_clear_row();
        }
        x += (unsigned int)dx;
        if ((unsigned int)dy > y)
          y = 0;
        else
          y -= (unsigned int)dy;
      }
      else {
        unsigned int n = (unsigned int)b;
        unsigned int bytes = (n + 1u) >> 1;
        unsigned int left = n;

        while (left != 0) {
          unsigned char hi, lo;
          int v = getc(f);
          if (v == EOF)
            goto fail;
          hi = cga_title_colour((unsigned char)((v >> 4) & 15));
          lo = cga_title_colour((unsigned char)(v & 15));
          cga_title_put_mapped_pixel(x, hi);
          row_dirty |= (hi != 0);
          ++x;
          --left;
          if (left == 0)
            break;
          cga_title_put_mapped_pixel(x, lo);
          row_dirty |= (lo != 0);
          ++x;
          --left;
        }
        if (bytes & 1u) {
          if (getc(f) == EOF)
            goto fail;
        }
      }
    }
  }

fail:
  fclose(f);
  return FALSE;
}
#endif

void cgatitle(void)
{
  cgaclear();
#ifdef DIGGER_ELKS_TITLE_BMP
  (void)cga_load_title_bmp("digtitle.bmp");
#endif
}

#ifdef DIGGER_CGA_PROFILE
void elks_dump_profile(void)
{
  printf("\n:\n");
  printf(" i:%u %u %u %u %u %u %u\n",
         elks_input_read_count, elks_input_queued_count,
         elks_input_dup_count, elks_input_overflow_count,
         elks_input_maxq, elks_input_reset_count,
         elks_input_discard_count);
  printf(" p:%u %u %u %u\n",
         elks_game_start_count, elks_penalty_frame_count,
         elks_penalty_max, elks_incmont_count);
  printf(" f:%u %u %u %u %u %u %u %u\n",
         elks_pace_wait_count, elks_pace_sleep_count,
         elks_pace_wait0_count, elks_pace_late_count,
         elks_pace_late2_count, elks_pace_back_count,
         elks_pace_sleep_max, elks_pace_late_streak_max);
  printf(" c:%u %u %u %u %u %u\n",
         elks_cga_puti4_calls, elks_cga_geti4_calls,
         elks_cga_putim4_calls, elks_cga_putim5_calls,
         elks_cga_putim6_calls, elks_cga_putim_generic_calls);
  printf(" s:%u %u %u %u %u\n",
         elks_prof_putis_restored, elks_prof_putims_redrawn,
         elks_prof_setrdrw_scan, elks_prof_bcollides_scan,
         elks_prof_bcollide_calls);
  printf(" r:%u %u %u %u %u %u\n",
         elks_prof_getis_scan, elks_prof_getis_saved,
         elks_prof_putis_scan, elks_prof_putis_restored,
         elks_prof_putims_scan, elks_prof_putims_redrawn);
  printf(" b:%u %u %u m:%u %u %u %u\n",
         elks_prof_dobags_calls, elks_prof_bag_live_max,
         elks_prof_bag_updates, elks_prof_domons_calls,
         elks_prof_monster_slots_scanned, elks_prof_monster_updates,
         elks_monai_count);

  printf("\n.\n");
  fflush(stdout);

#ifdef DIGGER_ELKS
  /* finish() calls us while the ELKS keyboard is still in raw mode, so this
   * really is one keypress, not Enter.  Clear any queued gameplay key first
   * so the exit key that ended the game does not immediately dismiss this.
   */
  elks_input_reset();
#endif
  (void)getkey();
}
#endif
