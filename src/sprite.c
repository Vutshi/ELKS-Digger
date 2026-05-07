/* Digger Remastered
   Copyright (c) Andrew Jenner 1998-2004 */

#include <stdlib.h>
#include "def.h"
#include "sprite.h"
#include "hardware.h"

bool retrflag=TRUE;

bool sprrdrwf[SPRITES+1];
bool sprrecf[SPRITES+1];
bool sprenf[SPRITES];
Sint4 sprch[SPRITES+1];
Uint3 *sprmov[SPRITES];
Sint4 sprx[SPRITES+1];
Sint4 spry[SPRITES+1];
static Sint4 sprx2[SPRITES+1];
static Sint4 spry2[SPRITES+1];
Sint4 sprwid[SPRITES+1];
Sint4 sprhei[SPRITES+1];
Sint4 sprbwid[SPRITES];
Sint4 sprbhei[SPRITES];
Sint4 sprnch[SPRITES];
Sint4 sprnwid[SPRITES];
Sint4 sprnhei[SPRITES];
Sint4 sprnbwid[SPRITES];
Sint4 sprnbhei[SPRITES];

/*
 * ELKS/8086 live frames used to scan all SPRITES slots repeatedly even
 * though many slots are disabled.  Keep a small sorted list of enabled
 * sprites so restore/redraw/collision invalidation walks only live objects.
 * Sorted order preserves the original ascending sprite-number behaviour.
 */
static Sint4 spract[SPRITES];
static Sint4 spractidx[SPRITES];
static Sint4 spractcnt=0;
static bool spractinit=FALSE;

/*
 * Dirty sprites are a much smaller subset than active sprites on normal live
 * frames.  Keep a sorted dirty list so putis()/putims() can preserve the
 * original ascending sprite-number draw order without scanning every active
 * sprite just to find the few marked ones.
 */
static Sint4 sprdirty[SPRITES];
static Sint4 sprdirtyidx[SPRITES];
static Sint4 sprdirtycnt=0;

#ifdef DIGGER_CGA_PROFILE
unsigned int elks_prof_createspr_calls;
unsigned int elks_prof_movedrawspr_calls;
unsigned int elks_prof_erasespr_calls;
unsigned int elks_prof_drawspr_calls;
unsigned int elks_prof_getis_calls;
unsigned int elks_prof_getis_scan;
unsigned int elks_prof_getis_saved;
unsigned int elks_prof_setrdrw_calls;
unsigned int elks_prof_setrdrw_scan;
unsigned int elks_prof_setrdrw_recurse;
unsigned int elks_prof_collide_calls;
unsigned int elks_prof_bcollide_calls;
unsigned int elks_prof_putims_calls;
unsigned int elks_prof_putims_scan;
unsigned int elks_prof_putims_redrawn;
unsigned int elks_prof_putis_calls;
unsigned int elks_prof_putis_scan;
unsigned int elks_prof_putis_restored;
unsigned int elks_prof_bcollides_calls;
unsigned int elks_prof_bcollides_scan;
#define SPRPROF_INC(v) do { if ((v) != 65535u) ++(v); } while (0)
#define SPRPROF_ADD(v,n) ((void)0)
#else
#define SPRPROF_INC(v) ((void)0)
#define SPRPROF_ADD(v,n) ((void)0)
#endif

static void sprlist_init(void);
static void sprlist_enable(Sint4 n);
static void sprlist_disable(Sint4 n);
static void spr_mark_rdrw(Sint4 n);
static void spr_dirty_add(Sint4 n);
static void spr_dirty_remove(Sint4 n);
static void spr_update_extent(Sint4 n);

void clearrdrwf(void);
void clearrecf(void);
void setrdrwflgs(Sint4 n);
#ifndef DIGGER_ELKS
bool bcollide(Sint4 bx,Sint4 si);
#endif
void putims(void);
void putis(void);
void bcollides(int bx);

#ifndef DIGGER_ELKS
#ifdef DIGGER_CGA_ONLY
void (*ginit)(void)=cgainit;
void (*gclear)(void)=cgaclear;
void (*gpal)(Sint4 pal)=cgapal;
void (*ginten)(Sint4 inten)=cgainten;
void (*gputi)(Sint4 x,Sint4 y,Uint3 *p,Sint4 w,Sint4 h)=cgaputi;
void (*ggeti)(Sint4 x,Sint4 y,Uint3 *p,Sint4 w,Sint4 h)=cgageti;
void (*gputim)(Sint4 x,Sint4 y,Sint4 ch,Sint4 w,Sint4 h)=cgaputim;
Sint4 (*ggetpix)(Sint4 x,Sint4 y)=cgagetpix;
void (*gtitle)(void)=cgatitle;
void (*gwrite)(Sint4 x,Sint4 y,Sint4 ch,Sint4 c)=cgawrite;
#else
void (*ginit)(void)=vgainit;
void (*gclear)(void)=vgaclear;
void (*gpal)(Sint4 pal)=vgapal;
void (*ginten)(Sint4 inten)=vgainten;
void (*gputi)(Sint4 x,Sint4 y,Uint3 *p,Sint4 w,Sint4 h)=vgaputi;
void (*ggeti)(Sint4 x,Sint4 y,Uint3 *p,Sint4 w,Sint4 h)=vgageti;
void (*gputim)(Sint4 x,Sint4 y,Sint4 ch,Sint4 w,Sint4 h)=vgaputim;
Sint4 (*ggetpix)(Sint4 x,Sint4 y)=vgagetpix;
void (*gtitle)(void)=vgatitle;
void (*gwrite)(Sint4 x,Sint4 y,Sint4 ch,Sint4 c)=vgawrite;
#endif
#endif

static void sprlist_init(void)
{
  Sint4 i;
  if (spractinit)
    return;
  for (i=0;i<SPRITES;i++) {
    spractidx[i]=-1;
    sprdirtyidx[i]=-1;
  }
  spractcnt=0;
  sprdirtycnt=0;
  spractinit=TRUE;
}

static void spr_dirty_add(Sint4 n)
{
  Sint4 i,pos;

  if (n<0 || n>=SPRITES || !sprenf[n])
    return;
  if (sprdirtyidx[n]>=0)
    return;
  pos=0;
  while (pos<sprdirtycnt && sprdirty[pos]<n)
    pos++;
  for (i=sprdirtycnt;i>pos;i--) {
    sprdirty[i]=sprdirty[i-1];
    sprdirtyidx[sprdirty[i]]=i;
  }
  sprdirty[pos]=n;
  sprdirtyidx[n]=pos;
  sprdirtycnt++;
}

static void spr_dirty_remove(Sint4 n)
{
  Sint4 i,pos;

  if (n<0 || n>=SPRITES)
    return;
  pos=(Sint4)sprdirtyidx[n];
  if (pos<0)
    return;
  for (i=pos;i<sprdirtycnt-1;i++) {
    sprdirty[i]=sprdirty[i+1];
    sprdirtyidx[sprdirty[i]]=i;
  }
  sprdirtycnt--;
  sprdirtyidx[n]=-1;
}

static void spr_mark_rdrw(Sint4 n)
{
  sprlist_init();
  if (n<0 || n>SPRITES)
    return;
  sprrdrwf[n]=TRUE;
  spr_dirty_add(n);
}

static void sprlist_enable(Sint4 n)
{
  Sint4 i,pos;

  sprlist_init();
  if (sprenf[n])
    return;
  sprenf[n]=TRUE;
  if (spractidx[n]>=0)
    return;
  pos=0;
  while (pos<spractcnt && spract[pos]<n)
    pos++;
  for (i=spractcnt;i>pos;i--) {
    spract[i]=spract[i-1];
    spractidx[spract[i]]=i;
  }
  spract[pos]=n;
  spractidx[n]=pos;
  spractcnt++;
  if (sprrdrwf[n])
    spr_dirty_add(n);
}

static void sprlist_disable(Sint4 n)
{
  Sint4 i,pos;

  sprlist_init();
  spr_dirty_remove(n);
  sprenf[n]=FALSE;
  sprrdrwf[n]=FALSE;
  sprrecf[n]=FALSE;
  pos=(Sint4)spractidx[n];
  if (pos<0)
    return;
  for (i=pos;i<spractcnt-1;i++) {
    spract[i]=spract[i+1];
    spractidx[spract[i]]=i;
  }
  spractcnt--;
  spractidx[n]=-1;
}

static void spr_update_extent(Sint4 n)
{
  sprx2[n]=sprx[n]+(sprwid[n]<<2)-1;
  spry2[n]=spry[n]+sprhei[n]-1;
}

void setretr(bool f)
{
#ifdef INTDRF
  retrflag=FALSE;
#else
  retrflag=f;
#endif
}

void createspr(Sint4 n,Sint4 ch,Uint3 *mov,Sint4 wid,Sint4 hei,Sint4 bwid,
               Sint4 bhei)
{
  SPRPROF_INC(elks_prof_createspr_calls);
  sprlist_disable(n);
  sprnch[n]=sprch[n]=ch;
  sprmov[n]=mov;
  sprnwid[n]=sprwid[n]=wid;
  sprnhei[n]=sprhei[n]=hei;
  sprnbwid[n]=sprbwid[n]=bwid;
  sprnbhei[n]=sprbhei[n]=bhei;
}

void movedrawspr(Sint4 n,Sint4 x,Sint4 y)
{
  SPRPROF_INC(elks_prof_movedrawspr_calls);
  sprx[n]=x&-4;
  spry[n]=y;
  sprch[n]=sprnch[n];
  sprwid[n]=sprnwid[n];
  sprhei[n]=sprnhei[n];
  sprbwid[n]=sprnbwid[n];
  sprbhei[n]=sprnbhei[n];
  spr_update_extent(n);
  clearrdrwf();
  setrdrwflgs(n);
  putis();
  ggeti(sprx[n],spry[n],sprmov[n],sprwid[n],sprhei[n]);
  sprlist_enable(n);
  spr_mark_rdrw(n);
  putims();
}

void erasespr(Sint4 n)
{
  SPRPROF_INC(elks_prof_erasespr_calls);
  if (!sprenf[n])
    return;
  gputi(sprx[n],spry[n],sprmov[n],sprwid[n],sprhei[n]);
  sprlist_disable(n);
  clearrdrwf();
  setrdrwflgs(n);
  putims();
}

void drawspr(Sint4 n,Sint4 x,Sint4 y)
{
  Sint4 ai,i,ox,oy,ox2,oy2,nx2,ny2;
  SPRPROF_INC(elks_prof_drawspr_calls);
  x&=-4;

  /*
   * Original DOS Digger invalidates redraw dependencies twice here: once for
   * the sprite's old rectangle and once for its new rectangle.  Both passes
   * build the same kind of transitive overlap closure and OR the results into
   * sprrdrwf[].  Do one seed scan against both rectangles, then recurse from
   * matching live sprites exactly as setrdrwflgs() does.  This preserves the
   * old/new dirty-area union while saving one full active-sprite scan for the
   * common moving-sprite path.
   */
  ox=sprx[n];
  oy=spry[n];
  ox2=sprx2[n];
  oy2=spry2[n];
  nx2=x+(sprnwid[n]<<2)-1;
  ny2=y+sprnhei[n]-1;
  clearrdrwf();
  SPRPROF_INC(elks_prof_setrdrw_calls);
  sprrecf[n]=TRUE;
  for (ai=0;ai<spractcnt;ai++) {
    i=spract[ai];
    if (i==n || (sprrecf[i] && sprrdrwf[i]))
      continue;
    SPRPROF_INC(elks_prof_setrdrw_scan);
    if (!(sprx[i]>ox2 || ox>sprx2[i] ||
          spry[i]>oy2 || oy>spry2[i]) ||
        !(sprx[i]>nx2 || x>sprx2[i] ||
          spry[i]>ny2 || y>spry2[i])) {
      spr_mark_rdrw(i);
      SPRPROF_INC(elks_prof_setrdrw_recurse);
      setrdrwflgs(i);
    }
  }

  spr_mark_rdrw(n);
  putis();
  sprlist_enable(n);
  sprx[n]=x;
  spry[n]=y;
  sprch[n]=sprnch[n];
  sprwid[n]=sprnwid[n];
  sprhei[n]=sprnhei[n];
  sprbwid[n]=sprnbwid[n];
  sprbhei[n]=sprnbhei[n];
  spr_update_extent(n);
  ggeti(sprx[n],spry[n],sprmov[n],sprwid[n],sprhei[n]);
  putims();
  bcollides(n);
}

void initspr(Sint4 n,Sint4 ch,Sint4 wid,Sint4 hei,Sint4 bwid,Sint4 bhei)
{
  sprnch[n]=ch;
  sprnwid[n]=wid;
  sprnhei[n]=hei;
  sprnbwid[n]=bwid;
  sprnbhei[n]=bhei;
}

void initmiscspr(Sint4 x,Sint4 y,Sint4 wid,Sint4 hei)
{
  sprx[SPRITES]=x;
  spry[SPRITES]=y;
  sprwid[SPRITES]=wid;
  sprhei[SPRITES]=hei;
  spr_update_extent(SPRITES);
  clearrdrwf();
  setrdrwflgs(SPRITES);
  putis();
}

void getis(void)
{
  Sint4 di,i;
  SPRPROF_INC(elks_prof_getis_calls);
  for (di=0;di<sprdirtycnt;di++) {
    i=sprdirty[di];
    SPRPROF_INC(elks_prof_getis_scan);
    if (sprrdrwf[i]) {
      SPRPROF_INC(elks_prof_getis_saved);
      ggeti(sprx[i],spry[i],sprmov[i],sprwid[i],sprhei[i]);
    }
  }
  putims();
}

void drawmiscspr(Sint4 x,Sint4 y,Sint4 ch,Sint4 wid,Sint4 hei)
{
  sprx[SPRITES]=x&-4;
  spry[SPRITES]=y;
  sprch[SPRITES]=ch;
  sprwid[SPRITES]=wid;
  sprhei[SPRITES]=hei;
  spr_update_extent(SPRITES);
  gputim(sprx[SPRITES],spry[SPRITES],sprch[SPRITES],sprwid[SPRITES],
         sprhei[SPRITES]);
}

void clearrdrwf(void)
{
  Sint4 ai,i;

  /* Only dirty sprites can have sprrdrwf[] set. */
  for (ai=0;ai<sprdirtycnt;ai++) {
    i=sprdirty[ai];
    sprrdrwf[i]=FALSE;
    sprdirtyidx[i]=-1;
  }
  sprdirtycnt=0;

  /* sprrecf[] is a traversal marker, so clear it over the active set. */
  for (ai=0;ai<spractcnt;ai++)
    sprrecf[spract[ai]]=FALSE;
  sprrecf[SPRITES]=FALSE;
  sprrdrwf[SPRITES]=FALSE;
}

void clearrecf(void)
{
  Sint4 ai,i;
  for (ai=0;ai<spractcnt;ai++) {
    i=spract[ai];
    sprrecf[i]=FALSE;
  }
  sprrecf[SPRITES]=FALSE;
}

void setrdrwflgs(Sint4 n)
{
  Sint4 ai,i;

  if (sprrecf[n])
    return;

  SPRPROF_INC(elks_prof_setrdrw_calls);
  sprrecf[n]=TRUE;
  for (ai=0;ai<spractcnt;ai++) {
    i=spract[ai];
    if (i==n || (sprrecf[i] && sprrdrwf[i]))
      continue;
    SPRPROF_INC(elks_prof_setrdrw_scan);
    if (!(sprx[i]>sprx2[n] || sprx[n]>sprx2[i] ||
          spry[i]>spry2[n] || spry[n]>spry2[i])) {
      spr_mark_rdrw(i);
      SPRPROF_INC(elks_prof_setrdrw_recurse);
      setrdrwflgs(i);
    }
  }
}

#ifndef DIGGER_ELKS
bool bcollide(Sint4 bx,Sint4 si)
{
  SPRPROF_INC(elks_prof_bcollide_calls);

  /* Fast path for current Digger sprites: all border shrink values are zero. */
  if ((sprbwid[bx]|sprbwid[si]|sprbhei[bx]|sprbhei[si])==0) {
    if (sprx[bx]>sprx2[si])
      return FALSE;
    if (sprx[si]>sprx2[bx])
      return FALSE;
    if (spry[bx]>spry2[si])
      return FALSE;
    if (spry[si]>spry2[bx])
      return FALSE;
    return TRUE;
  }

  if (sprx[bx]>=sprx[si]) {
    if (sprx[bx]+sprbwid[bx]>sprx2[si]-sprbwid[si])
      return FALSE;
  }
  else
    if (sprx[si]+sprbwid[si]>sprx2[bx]-sprbwid[bx])
      return FALSE;
  if (spry[bx]>=spry[si]) {
    if (spry[bx]+sprbhei[bx]<=spry2[si]-sprbhei[si])
      return TRUE;
    return FALSE;
  }
  if (spry[si]+sprbhei[si]<=spry2[bx]-sprbhei[bx])
    return TRUE;
  return FALSE;
}
#endif

void putims(void)
{
  Sint4 di,i;
  SPRPROF_INC(elks_prof_putims_calls);
  for (di=0;di<sprdirtycnt;di++) {
    i=sprdirty[di];
    SPRPROF_INC(elks_prof_putims_scan);
    if (sprrdrwf[i]) {
      SPRPROF_INC(elks_prof_putims_redrawn);
      gputim(sprx[i],spry[i],sprch[i],sprwid[i],sprhei[i]);
    }
  }
}

void putis(void)
{
  Sint4 di,i;
  SPRPROF_INC(elks_prof_putis_calls);
  for (di=0;di<sprdirtycnt;di++) {
    i=sprdirty[di];
    SPRPROF_INC(elks_prof_putis_scan);
    if (sprrdrwf[i]) {
      SPRPROF_INC(elks_prof_putis_restored);
      gputi(sprx[i],spry[i],sprmov[i],sprwid[i],sprhei[i]);
    }
  }
}

Sint4 first[TYPES],coll[SPRITES];
int firstt[TYPES]={FIRSTBONUS,FIRSTBAG,FIRSTMONSTER,FIRSTFIREBALL,FIRSTDIGGER};
int lastt[TYPES]={LASTBONUS,LASTBAG,LASTMONSTER,LASTFIREBALL,LASTDIGGER};

void bcollides(int spr)
{
  int spc,next,i,ai;
  Sint4 tail[TYPES];

  SPRPROF_INC(elks_prof_bcollides_calls);
  for (next=0;next<TYPES;next++)
    first[next]=tail[next]=-1;
  for (ai=0;ai<spractcnt;ai++) {
    spc=spract[ai];
    SPRPROF_INC(elks_prof_bcollides_scan);
    if (spc==spr)
      continue;
#ifdef DIGGER_ELKS
    SPRPROF_INC(elks_prof_bcollide_calls);
    if (sprx[spr]>sprx2[spc] || sprx[spc]>sprx2[spr] ||
        spry[spr]>spry2[spc] || spry[spc]>spry2[spr])
      continue;
#else
    if (!bcollide(spr,spc))
      continue;
#endif
    if (spc<FIRSTBAG)
      i=0;
    else if (spc<FIRSTMONSTER)
      i=1;
    else if (spc<FIRSTFIREBALL)
      i=2;
    else if (spc<FIRSTDIGGER)
      i=3;
    else
      i=4;
    coll[spc]=-1;
    if (tail[i]==-1)
      first[i]=tail[i]=spc;
    else {
      coll[tail[i]]=spc;
      tail[i]=spc;
    }
  }
}


void snapshotcollisions(Sint4 *clfirst,Sint4 *clcoll)
{
  Sint4 t,next;

  for (t=0;t<TYPES;t++) {
    next=first[t];
    clfirst[t]=next;
    while (next!=-1) {
      clcoll[next]=coll[next];
      next=coll[next];
    }
  }
}
