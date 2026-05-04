/* Small ELKS PC speaker smoke test, based on elkscmd/sys_utils/beep.c.
 * Stage 6E version: use precomputed PIT divisors and skip redundant writes.
 */
#include <autoconf.h>
#include "arch/io.h"
#include "arch/ports.h"
#include "unistd.h"

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

static unsigned short last_divisor;
static unsigned char speaker_on;

static void pcspk_set_divisor(unsigned short divisor)
{
#ifdef CONFIG_ARCH_IBMPC
  unsigned int tmp;

  if (divisor == 0) {
    if (speaker_on) {
      tmp = inb(SPEAKER_PORT) & 0xfcu;
      outb(tmp, SPEAKER_PORT);
    }
    speaker_on = 0;
    last_divisor = 0;
    return;
  }

  if (speaker_on && divisor == last_divisor)
    return;

  outb(0xb6, TIMER_CMDS_PORT);
  outb((unsigned int)divisor, TIMER2_PORT);
  outb((unsigned int)(divisor >> 8), TIMER2_PORT);

  tmp = inb(SPEAKER_PORT);
  if (tmp != (tmp | 3u))
    outb(tmp | 3u, SPEAKER_PORT);
  speaker_on = 1;
  last_divisor = divisor;
#else
  (void)divisor;
#endif
}

int main(void)
{
  static const unsigned short notes[4] = { 2712u, 1808u, 1356u, 1808u };
  unsigned int i;

  for (i = 0; i < 4u; i++) {
    pcspk_set_divisor(notes[i]);
    usleep(180000UL);
    pcspk_set_divisor(0);
    usleep(40000UL);
  }
  return 0;
}
