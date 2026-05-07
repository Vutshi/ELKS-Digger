/* Digger Remastered
   Copyright (c) Andrew Jenner 1998-2004 */

#include <string.h>
#include "def.h"
#include "bags.h"
#include "main.h"
#include "sprite.h"
#include "sound.h"
#include "drawing.h"
#include "monster.h"
#include "digger.h"
#include "scores.h"

#ifdef DIGGER_CGA_PROFILE
unsigned int elks_prof_dobags_calls;
unsigned int elks_prof_bag_live_max;
unsigned int elks_prof_bag_updates;
#define BAGPROF_INC(v) do { if ((v) != 65535u) ++(v); } while (0)
#else
#define BAGPROF_INC(v) ((void)0)
#endif

struct bag {
  Sint4 x,y,h,v,xr,yr,dir,wt,gt,fallh;
  bool wobbling,unfallen,exist;
} bagdat1[BAGS],bagdat2[BAGS],bagdat[BAGS];

Sint4 pushcount=0,goldtime=0;
#ifdef DIGGER_ELKS
#define BAGFIELD(h,v) field[(((v)<<4)-(v)+(h))]
#else
#define BAGFIELD(h,v) getfield((h),(v))
#endif
#ifdef DIGGER_ELKS
static Sint4 bagcnt=0,bagcnt1=0,bagcnt2=0;
#else
#define bagcnt BAGS
#endif

void updatebag(Sint4 bag);
void baghitground(Sint4 bag);
bool pushbag(Sint4 bag,Sint4 dir);
void removebag(Sint4 bn);
void getgold(Sint4 bag);

void initbags(void)
{
  Sint4 bag,x,y;
  pushcount=0;
  goldtime=150-levof10()*10;
  for (bag=0;bag<BAGS;bag++)
    bagdat[bag].exist=FALSE;
  bag=0;
  for (x=0;x<MWIDTH;x++)
    for (y=0;y<MHEIGHT;y++)
      if (getlevch(x,y,levplan())=='B')
        if (bag<BAGS) {
          bagdat[bag].exist=TRUE;
          bagdat[bag].gt=0;
          bagdat[bag].fallh=0;
          bagdat[bag].dir=DIR_NONE;
          bagdat[bag].wobbling=FALSE;
          bagdat[bag].wt=15;
          bagdat[bag].unfallen=TRUE;
          bagdat[bag].x=x*20+12;
          bagdat[bag].y=y*18+18;
          bagdat[bag].h=x;
          bagdat[bag].v=y;
          bagdat[bag].xr=0;
          bagdat[bag++].yr=0;
        }
#ifdef DIGGER_ELKS
  bagcnt=bag;
  if (curplayer==0)
    bagcnt1=bagcnt;
  else
    bagcnt2=bagcnt;
#endif
  if (curplayer==0)
    memcpy(bagdat1,bagdat,BAGS*sizeof(struct bag));
  else
    memcpy(bagdat2,bagdat,BAGS*sizeof(struct bag));
}

void drawbags(void)
{
  Sint4 bag;
#ifdef DIGGER_ELKS
  bagcnt=(curplayer==0) ? bagcnt1 : bagcnt2;
#endif
  for (bag=0;bag<bagcnt;bag++) {
    if (curplayer==0)
      memcpy(&bagdat[bag],&bagdat1[bag],sizeof(struct bag));
    else
      memcpy(&bagdat[bag],&bagdat2[bag],sizeof(struct bag));
    if (bagdat[bag].exist)
      movedrawspr(bag+FIRSTBAG,bagdat[bag].x,bagdat[bag].y);
  }
}

void cleanupbags(void)
{
  Sint4 bag;
  soundfalloff();
  for (bag=0;bag<bagcnt;bag++) {
    if (bagdat[bag].exist && ((bagdat[bag].h==7 && bagdat[bag].v==9) ||
        bagdat[bag].xr!=0 || bagdat[bag].yr!=0 || bagdat[bag].gt!=0 ||
        bagdat[bag].fallh!=0 || bagdat[bag].wobbling)) {
      bagdat[bag].exist=FALSE;
      erasespr(bag+FIRSTBAG);
    }
    if (curplayer==0)
      memcpy(&bagdat1[bag],&bagdat[bag],sizeof(struct bag));
    else
      memcpy(&bagdat2[bag],&bagdat[bag],sizeof(struct bag));
  }
#ifdef DIGGER_ELKS
  while (bagcnt>0 && !bagdat[bagcnt-1].exist)
    bagcnt--;
  if (curplayer==0)
    bagcnt1=bagcnt;
  else
    bagcnt2=bagcnt;
#endif
}

void dobags(void)
{
  Sint4 bag;
#ifdef DIGGER_CGA_PROFILE
  unsigned int livebags=0;
#endif
  bool soundfalloffflag=TRUE,soundwobbleoffflag=TRUE;
  BAGPROF_INC(elks_prof_dobags_calls);
  for (bag=0;bag<bagcnt;bag++)
    if (bagdat[bag].exist) {
#ifdef DIGGER_CGA_PROFILE
      if (livebags != 65535u)
        livebags++;
#endif
      if (bagdat[bag].gt!=0) {
        if (bagdat[bag].gt==1) {
          soundbreak();
          drawgold(bag,4,bagdat[bag].x,bagdat[bag].y);
          incpenalty();
        }
        if (bagdat[bag].gt==3) {
          drawgold(bag,5,bagdat[bag].x,bagdat[bag].y);
          incpenalty();
        }
        if (bagdat[bag].gt==5) {
          drawgold(bag,6,bagdat[bag].x,bagdat[bag].y);
          incpenalty();
        }
        bagdat[bag].gt++;
        if (bagdat[bag].gt==goldtime)
          removebag(bag);
        else
          if (bagdat[bag].v<MHEIGHT-1 && bagdat[bag].gt<goldtime-10)
            if ((BAGFIELD(bagdat[bag].h,bagdat[bag].v+1)&0x2000)==0)
              bagdat[bag].gt=goldtime-10;
      }
      else {
        BAGPROF_INC(elks_prof_bag_updates);
        updatebag(bag);
      }

      if (bagdat[bag].exist) {
        if (bagdat[bag].dir==DIR_DOWN)
          soundfalloffflag=FALSE;
        if (bagdat[bag].dir!=DIR_DOWN && bagdat[bag].wobbling)
          soundwobbleoffflag=FALSE;
      }
    }
#ifdef DIGGER_CGA_PROFILE
  if (livebags > elks_prof_bag_live_max)
    elks_prof_bag_live_max = livebags;
#endif
  if (soundfalloffflag)
    soundfalloff();
  if (soundwobbleoffflag)
    soundwobbleoff();
}

Sint4 wblanim[4]={2,0,1,0};

void updatebag(Sint4 bag)
{
  Sint4 x,h,xr,y,v,yr,wbl;
  x=bagdat[bag].x;
  h=bagdat[bag].h;
  xr=bagdat[bag].xr;
  y=bagdat[bag].y;
  v=bagdat[bag].v;
  yr=bagdat[bag].yr;
  switch (bagdat[bag].dir) {
    case DIR_NONE:
      if (y<180 && xr==0) {
        if (bagdat[bag].wobbling) {
          if (bagdat[bag].wt==0) {
            bagdat[bag].dir=DIR_DOWN;
            soundfall();
            break;
          }
          bagdat[bag].wt--;
          wbl=bagdat[bag].wt&7;
          if (!(wbl&1)) {
            drawgold(bag,wblanim[wbl>>1],x,y);
            incpenalty();
            soundwobble();
          }
        }
        else
          if ((BAGFIELD(h,v+1)&0xfdf)!=0xfdf)
            if (!checkdiggerunderbag(h,v+1))
              bagdat[bag].wobbling=TRUE;
      }
      else {
        bagdat[bag].wt=15;
        bagdat[bag].wobbling=FALSE;
      }
      break;
    case DIR_RIGHT:
    case DIR_LEFT:
      if (xr==0) {
        if (y<180 && (BAGFIELD(h,v+1)&0xfdf)!=0xfdf) {
          bagdat[bag].dir=DIR_DOWN;
          bagdat[bag].wt=0;
          soundfall();
        }
        else
          baghitground(bag);
      }
      break;
    case DIR_DOWN:
      if (yr==0)
        bagdat[bag].fallh++;
      if (y>=180)
        baghitground(bag);
      else
        if ((BAGFIELD(h,v+1)&0xfdf)==0xfdf)
          if (yr==0)
            baghitground(bag);
      checkmonscared(bagdat[bag].h);
  }
  if (bagdat[bag].dir!=DIR_NONE) {
    if (bagdat[bag].dir!=DIR_DOWN && pushcount!=0)
      pushcount--;
    else
      pushbag(bag,bagdat[bag].dir);
  }
}

void baghitground(Sint4 bag)
{
  int i,next;
  if (bagdat[bag].dir==DIR_DOWN && bagdat[bag].fallh>1)
    bagdat[bag].gt=1;
  else
    bagdat[bag].fallh=0;
  bagdat[bag].dir=DIR_NONE;
  bagdat[bag].wt=15;
  bagdat[bag].wobbling=FALSE;
  drawgold(bag,0,bagdat[bag].x,bagdat[bag].y);
  incpenalty();
  i=first[1];
  while (i!=-1) {
    next=coll[i];
    removebag(i-FIRSTBAG);
    i=next;
  }
}

bool pushbag(Sint4 bag,Sint4 dir)
{
  Sint4 x,y,h,v,xr,yr,ox,oy;
  Sint4 clfirst[TYPES],clcoll[SPRITES];
  int i;
  bool push=TRUE,digf;
  ox=x=bagdat[bag].x;
  oy=y=bagdat[bag].y;
  h=bagdat[bag].h;
  v=bagdat[bag].v;
  xr=bagdat[bag].xr;
  yr=bagdat[bag].yr;
  if (bagdat[bag].gt!=0) {
    getgold(bag);
    return TRUE;
  }
  if (bagdat[bag].dir==DIR_DOWN && (dir==DIR_RIGHT || dir==DIR_LEFT)) {
    drawgold(bag,3,x,y);
    incpenalty();
    i=first[4];
    while (i!=-1) {
      if (diggery(i-FIRSTDIGGER+curplayer)>=y)
        killdigger(i-FIRSTDIGGER+curplayer,1,bag);
      i=coll[i];
    }
    if (first[2]!=-1)
      squashmonsters(bag,first,coll);
    return 1;
  }
  if ((x==292 && dir==DIR_RIGHT) || (x==12 && dir==DIR_LEFT) ||
      (y==180 && dir==DIR_DOWN) || (y==18 && dir==DIR_UP))
    push=FALSE;
  if (push) {
    switch (dir) {
      case DIR_RIGHT:
        x+=4;
        xr+=4;
        if (xr==20) {
          xr=0;
          h++;
        }
        break;
      case DIR_LEFT:
        x-=4;
        if (xr==0) {
          xr=16;
          h--;
        }
        else
          xr-=4;
        break;
      case DIR_DOWN:
        if (bagdat[bag].unfallen) {
          bagdat[bag].unfallen=FALSE;
          drawsquareblob(x,y);
          drawtopblob(x,y+21);
        }
        else
          drawfurryblob(x,y);
#ifdef DIGGER_ELKS
        eatfieldgrid(h,xr,v,yr,dir);
#else
        eatfield(x,y,dir);
#endif
        killemerald(h,v);
        y+=6;
        yr+=6;
        if (yr==18) {
          yr=0;
          v++;
        }
    }
    switch(dir) {
      case DIR_DOWN:
        drawgold(bag,3,x,y);
        incpenalty();
        i=first[4];
        while (i!=-1) {
          if (diggery(i-FIRSTDIGGER+curplayer)>=y)
            killdigger(i-FIRSTDIGGER+curplayer,1,bag);
          i=coll[i];
        }
        if (first[2]!=-1)
          squashmonsters(bag,first,coll);
        break;
      case DIR_RIGHT:
      case DIR_LEFT:
        bagdat[bag].wt=15;
        bagdat[bag].wobbling=FALSE;
        drawgold(bag,0,x,y);
        snapshotcollisions(clfirst,clcoll);
        incpenalty();
        pushcount=1;
        if (clfirst[1]!=-1)
          if (!pushbags(dir,clfirst,clcoll)) {
            x=ox;
            y=oy;
            h=bagdat[bag].h;
            xr=bagdat[bag].xr;
            v=bagdat[bag].v;
            yr=bagdat[bag].yr;
            drawgold(bag,0,ox,oy);
            incpenalty();
            push=FALSE;
          }
        i=clfirst[4];
        digf=FALSE;
        while (i!=-1) {
          if (digalive(i-FIRSTDIGGER+curplayer))
            digf=TRUE;
          i=clcoll[i];
        }
        if (digf || clfirst[2]!=-1) {
          x=ox;
          y=oy;
          h=bagdat[bag].h;
          xr=bagdat[bag].xr;
          v=bagdat[bag].v;
          yr=bagdat[bag].yr;
          drawgold(bag,0,ox,oy);
          incpenalty();
          push=FALSE;
        }
    }
    if (push)
      bagdat[bag].dir=dir;
    else
      bagdat[bag].dir=reversedir(dir);
    bagdat[bag].x=x;
    bagdat[bag].y=y;
    bagdat[bag].h=h;
    bagdat[bag].v=v;
    bagdat[bag].xr=xr;
    bagdat[bag].yr=yr;
  }
  return push;
}

bool pushbags(Sint4 dir,Sint4 *clfirst,Sint4 *clcoll)
{
  bool push=TRUE;
  int next=clfirst[1];
  while (next!=-1) {
    if (!pushbag(next-FIRSTBAG,dir))
      push=FALSE;
    next=clcoll[next];
  }
  return push;
}

bool pushudbags(Sint4 *clfirst,Sint4 *clcoll)
{
  bool push=TRUE;
  int next=clfirst[1];
  while (next!=-1) {
    if (bagdat[next-FIRSTBAG].gt!=0)
      getgold(next-FIRSTBAG);
    else
      push=FALSE;
    next=clcoll[next];
  }
  return push;
}

void removebag(Sint4 bag)
{
  if (bagdat[bag].exist) {
    bagdat[bag].exist=FALSE;
    erasespr(bag+FIRSTBAG);
#ifdef DIGGER_ELKS
    while (bagcnt>0 && !bagdat[bagcnt-1].exist)
      bagcnt--;
#endif
  }
}

bool bagexist(int bag)
{
  return bagdat[bag].exist;
}

Sint4 bagy(Sint4 bag)
{
  return bagdat[bag].y;
}

Sint4 getbagdir(Sint4 bag)
{
  if (bagdat[bag].exist)
    return bagdat[bag].dir;
  return -1;
}

void removebags(Sint4 *clfirst,Sint4 *clcoll)
{
  int next=clfirst[1];
  while (next!=-1) {
    removebag(next-FIRSTBAG);
    next=clcoll[next];
  }
}

Sint4 getnmovingbags(void)
{
  Sint4 bag,n=0;
  for (bag=0;bag<bagcnt;bag++)
    if (bagdat[bag].exist && bagdat[bag].gt<10 &&
        (bagdat[bag].gt!=0 || bagdat[bag].wobbling))
      n++;
  return n;
}

void getgold(Sint4 bag)
{
  bool f=TRUE;
  int i;
  drawgold(bag,6,bagdat[bag].x,bagdat[bag].y);
  incpenalty();
  i=first[4];
  while (i!=-1) {
    if (digalive(i-FIRSTDIGGER+curplayer)) {
      scoregold(i-FIRSTDIGGER+curplayer);
      soundgold();
      digresettime(i-FIRSTDIGGER+curplayer);
      f=FALSE;
    }
    i=coll[i];
  }
  if (f)
    mongold();
  removebag(bag);
}
