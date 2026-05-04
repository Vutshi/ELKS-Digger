/* Digger Remastered
   Copyright (c) Andrew Jenner 1998-2004 */

void setretr(bool f);
void movedrawspr(Sint4 n,Sint4 x,Sint4 y);
void erasespr(Sint4 n);
void createspr(Sint4 n,Sint4 ch,Uint3 *mov,Sint4 wid,Sint4 hei,Sint4 bwid,
               Sint4 bhei);
void initspr(Sint4 n,Sint4 ch,Sint4 wid,Sint4 hei,Sint4 bwid,Sint4 bhei);
void drawspr(Sint4 n,Sint4 x,Sint4 y);
void initmiscspr(Sint4 x,Sint4 y,Sint4 wid,Sint4 hei);
void getis(void);
void drawmiscspr(Sint4 x,Sint4 y,Sint4 ch,Sint4 wid,Sint4 hei);

#ifdef DIGGER_ELKS
#include "hardware.h"
#define ginit()              cgainit()
#define gclear()             cgaclear()
#define gpal(pal)            cgapal((pal))
#define ginten(inten)        cgainten((inten))
#define gputi(x,y,p,w,h)     cgaputi((x),(y),(p),(w),(h))
#define ggeti(x,y,p,w,h)     cgageti((x),(y),(p),(w),(h))
#define gputim(x,y,ch,w,h)   cgaputim((x),(y),(ch),(w),(h))
#define ggetpix(x,y)         cgagetpix((x),(y))
#define gtitle()             cgatitle()
#define gwrite(x,y,ch,c)     cgawrite((x),(y),(ch),(c))
#else
extern void (*ginit)(void);
extern void (*gclear)(void);
extern void (*gpal)(Sint4 pal);
extern void (*ginten)(Sint4 inten);
extern void (*gputi)(Sint4 x,Sint4 y,Uint3 *p,Sint4 w,Sint4 h);
extern void (*ggeti)(Sint4 x,Sint4 y,Uint3 *p,Sint4 w,Sint4 h);
extern void (*gputim)(Sint4 x,Sint4 y,Sint4 ch,Sint4 w,Sint4 h);
extern Sint4 (*ggetpix)(Sint4 x,Sint4 y);
extern void (*gtitle)(void);
extern void (*gwrite)(Sint4 x,Sint4 y,Sint4 ch,Sint4 c);
#endif

extern Sint3 first[TYPES],coll[SPRITES];
void snapshotcollisions(Sint3 *clfirst,Sint3 *clcoll);
