/* Digger Remastered
   Copyright (c) Andrew Jenner 1998-2004 */

#include <stdlib.h>
#include "def.h"
#include "monster.h"
#include "main.h"
#include "sprite.h"
#include "digger.h"
#include "drawing.h"
#include "bags.h"
#include "sound.h"
#include "scores.h"
#include "record.h"

struct monster
{
  Sint4 x,y,h,v,xr,yr,dir,hdir,t,hnt,death,bag,dtime,stime,chase;
  bool flag,nob,alive;
} mondat[6];

Sint4 nextmonster=0,totalmonsters=0,maxmononscr=0,nextmontime=0,mongaptime=0;
Sint4 chase=0;

#ifdef DIGGER_CGA_PROFILE
unsigned int elks_monai_count;
unsigned int elks_prof_domons_calls;
unsigned int elks_prof_monster_slots_scanned;
unsigned int elks_prof_monster_updates;
unsigned int elks_prof_hobbin_dig_calls;
#define MONPROF_INC(v) do { if ((v) != 65535u) ++(v); } while (0)
#else
#define MONPROF_INC(v) ((void)0)
#endif

bool unbonusflag=FALSE;

void createmonster(void);
void monai(Sint4 mon);
void mondie(Sint4 mon);
bool fieldclear(Sint4 dir,Sint4 x,Sint4 y);
void squashmonster(Sint4 mon,Sint4 death,Sint4 bag);
Sint4 nmononscr(void);

void initmonsters(void)
{
  Sint4 i,lev10;
  lev10=levof10();
  for (i=0;i<MONSTERS;i++)
    mondat[i].flag=FALSE;
  nextmonster=0;
  mongaptime=45-(lev10<<1);
  totalmonsters=lev10+5;
  switch (lev10) {
    case 1:
      maxmononscr=3;
      break;
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
      maxmononscr=4;
      break;
    case 8:
    case 9:
    case 10:
      maxmononscr=5;
  }
  nextmontime=10;
  unbonusflag=TRUE;
}

void erasemonsters(void)
{
  Sint4 i;
  for (i=0;i<MONSTERS;i++)
    if (mondat[i].flag)
      erasespr(i+FIRSTMONSTER);
}

void domonsters(void)
{
  Sint4 i;
  MONPROF_INC(elks_prof_domons_calls);
  if (nextmontime>0)
    nextmontime--;
  else {
    if (nextmonster<totalmonsters && nmononscr()<maxmononscr && isalive() &&
        !bonusmode)
      createmonster();
    if (unbonusflag && nextmonster==totalmonsters && nextmontime==0)
      if (isalive()) {
        unbonusflag=FALSE;
        createbonus();
      }
  }
  for (i=0;i<MONSTERS;i++) {
    MONPROF_INC(elks_prof_monster_slots_scanned);
    if (mondat[i].flag) {
      MONPROF_INC(elks_prof_monster_updates);
      if (mondat[i].hnt>10-levof10()) {
        if (mondat[i].nob) {
          mondat[i].nob=FALSE;
          mondat[i].hnt=0;
        }
      }
      if (mondat[i].alive)
        if (mondat[i].t==0) {
          monai(i);
          if (randno(15-levof10())==0) /* Need to split for determinism */
            if (mondat[i].nob && mondat[i].alive)
              monai(i);
        }
        else
          mondat[i].t--;
      else
        mondie(i);
    }
  }
}

void createmonster(void)
{
  Sint4 i;
  for (i=0;i<MONSTERS;i++)
    if (!mondat[i].flag) {
      mondat[i].flag=TRUE;
      mondat[i].alive=TRUE;
      mondat[i].t=0;
      mondat[i].nob=TRUE;
      mondat[i].hnt=0;
      mondat[i].h=14;
      mondat[i].v=0;
      mondat[i].x=292;
      mondat[i].y=18;
      mondat[i].xr=0;
      mondat[i].yr=0;
      mondat[i].dir=DIR_LEFT;
      mondat[i].hdir=DIR_LEFT;
      mondat[i].chase=chase+curplayer;
      chase=(chase+1)%diggers;
      nextmonster++;
      nextmontime=mongaptime;
      mondat[i].stime=5;
      movedrawspr(i+FIRSTMONSTER,mondat[i].x,mondat[i].y);
      break;
    }
}

bool mongotgold=FALSE;

void mongold(void)
{
  mongotgold=TRUE;
}

void monai(Sint4 mon)
{
  struct monster *mp;
  Sint4 monox,monoy,dir,mdirp1,mdirp2,mdirp3,mdirp4,t;
  Sint4 mh,mv,mxr,myr,digx,digy,lev10;
  Sint3 clcoll[SPRITES],clfirst[TYPES];
  int i,m,dig;
  bool push;
#ifdef DIGGER_CGA_PROFILE
  elks_monai_count++;
#endif
  mp=&mondat[mon];
  monox=mp->x;
  monoy=mp->y;
  mh=mp->h;
  mv=mp->v;
  mxr=mp->xr;
  myr=mp->yr;
  if (mp->xr==0 && mp->yr==0) {
    lev10=levof10();

    /* If we are here the monster needs to know which way to turn next. */

    /* Turn hobbin back into nobbin if it's had its time */

    if (mp->hnt>30+(lev10<<1))
      if (!mp->nob) {
        mp->hnt=0;
        mp->nob=TRUE;
      }

    /* Set up monster direction properties to chase Digger */

    dig=mp->chase;
    if (!digalive(dig))
      dig=(diggers-1)-dig;
    digx=diggerx(dig);
    digy=diggery(dig);

    if (abs(digy-mp->y)>abs(digx-mp->x)) {
      if (digy<mp->y) { mdirp1=DIR_UP;    mdirp4=DIR_DOWN; }
                  else { mdirp1=DIR_DOWN;  mdirp4=DIR_UP; }
      if (digx<mp->x) { mdirp2=DIR_LEFT;  mdirp3=DIR_RIGHT; }
                  else { mdirp2=DIR_RIGHT; mdirp3=DIR_LEFT; }
    }
    else {
      if (digx<mp->x) { mdirp1=DIR_LEFT;  mdirp4=DIR_RIGHT; }
                  else { mdirp1=DIR_RIGHT; mdirp4=DIR_LEFT; }
      if (digy<mp->y) { mdirp2=DIR_UP;    mdirp3=DIR_DOWN; }
                  else { mdirp2=DIR_DOWN;  mdirp3=DIR_UP; }
    }

    /* In bonus mode, run away from Digger */

    if (bonusmode) {
      t=mdirp1; mdirp1=mdirp4; mdirp4=t;
      t=mdirp2; mdirp2=mdirp3; mdirp3=t;
    }

    /* Adjust priorities so that monsters don't reverse direction unless they
       really have to */

    dir=reversedir(mp->dir);
    if (dir==mdirp1) {
      mdirp1=mdirp2;
      mdirp2=mdirp3;
      mdirp3=mdirp4;
      mdirp4=dir;
    }
    if (dir==mdirp2) {
      mdirp2=mdirp3;
      mdirp3=mdirp4;
      mdirp4=dir;
    }
    if (dir==mdirp3) {
      mdirp3=mdirp4;
      mdirp4=dir;
    }

    /* Introduce a random element on levels <6 : occasionally swap p1 and p3 */

    if (randno(lev10+5)==1) /* Need to split for determinism */
      if (lev10<6) {
        t=mdirp1;
        mdirp1=mdirp3;
        mdirp3=t;
      }

    /* Check field and find direction */

    if (fieldclear(mdirp1,mp->h,mp->v))
      dir=mdirp1;
    else
      if (fieldclear(mdirp2,mp->h,mp->v))
        dir=mdirp2;
      else
        if (fieldclear(mdirp3,mp->h,mp->v))
          dir=mdirp3;
        else
          if (fieldclear(mdirp4,mp->h,mp->v))
            dir=mdirp4;

    /* Hobbins don't care about the field: they go where they want. */

    if (!mp->nob)
      dir=mdirp1;

    /* Monsters take a time penalty for changing direction */

    if (mp->dir!=dir)
      mp->t++;

    /* Save the new direction */

    mp->dir=dir;
  }

  /* If monster is about to go off edge of screen, stop it. */

  if ((mp->x==292 && mp->dir==DIR_RIGHT) ||
      (mp->x==12 && mp->dir==DIR_LEFT) ||
      (mp->y==180 && mp->dir==DIR_DOWN) ||
      (mp->y==18 && mp->dir==DIR_UP))
    mp->dir=DIR_NONE;

  /* Change hdir for hobbin */

  if (mp->dir==DIR_LEFT || mp->dir==DIR_RIGHT)
    mp->hdir=mp->dir;

  /* Hobbins dig */

  if (!mp->nob) {
    MONPROF_INC(elks_prof_hobbin_dig_calls);
#ifdef DIGGER_ELKS
    eatfieldgrid(mh,mxr,mv,myr,mp->dir);
#else
    eatfield(mp->x,mp->y,mp->dir);
#endif
  }

  /* (Draw new tunnels) and move monster */

  switch (mp->dir) {
    case DIR_RIGHT:
      if (!mp->nob)
        drawrightblob(mp->x,mp->y);
      mp->x+=4;
      mxr+=4;
      if (mxr==20) {
        mxr=0;
        mh++;
      }
      break;
    case DIR_UP:
      if (!mp->nob)
        drawtopblob(mp->x,mp->y);
      mp->y-=3;
      if (myr==0) {
        myr=15;
        mv--;
      }
      else
        myr-=3;
      break;
    case DIR_LEFT:
      if (!mp->nob)
        drawleftblob(mp->x,mp->y);
      mp->x-=4;
      if (mxr==0) {
        mxr=16;
        mh--;
      }
      else
        mxr-=4;
      break;
    case DIR_DOWN:
      if (!mp->nob)
        drawbottomblob(mp->x,mp->y);
      mp->y+=3;
      myr+=3;
      if (myr==18) {
        myr=0;
        mv++;
      }
      break;
  }

  /* Hobbins can eat emeralds */

  if (!mp->nob)
    hitemerald(mh,mv,mxr,myr,mp->dir);

  /* If Digger's gone, don't bother */

  if (!isalive()) {
    mp->x=monox;
    mp->y=monoy;
    mh=mp->h;
    mv=mp->v;
    mxr=mp->xr;
    myr=mp->yr;
  }

  /* If monster's just started, don't move yet */

  if (mp->stime!=0) {
    mp->stime--;
    mp->x=monox;
    mp->y=monoy;
    mh=mp->h;
    mv=mp->v;
    mxr=mp->xr;
    myr=mp->yr;
  }

  /* Increase time counter for hobbin */

  if (!mp->nob && mp->hnt<100)
    mp->hnt++;

  /* Draw monster */

  push=TRUE;
  drawmon(mon,mp->nob,mp->hdir,mp->x,mp->y);
  snapshotcollisions(clfirst,clcoll);
  incpenalty();

  /* Collision with another monster */

  if (clfirst[2]!=-1) {
    mp->t++; /* Time penalty */
    /* Ensure both aren't moving in the same dir. */
    i=clfirst[2];
    do {
      m=i-FIRSTMONSTER;
      if (mp->dir==mondat[m].dir && mondat[m].stime==0 &&
          mp->stime==0)
        mondat[m].dir=reversedir(mondat[m].dir);
      /* The kludge here is to preserve playback for a bug in previous
         versions. */
      if (!kludge)
        incpenalty();
      else
        if (!(m&1))
          incpenalty();
      i=clcoll[i];
    } while (i!=-1);
    if (kludge)
      if (clfirst[0]!=-1)
        incpenalty();
  }

  /* Check for collision with bag */

  if (clfirst[1]!=-1) {
    mp->t++; /* Time penalty */
    mongotgold=FALSE;
    if (mp->dir==DIR_RIGHT || mp->dir==DIR_LEFT) {
      push=pushbags(mp->dir,clfirst,clcoll);      /* Horizontal push */
      mp->t++; /* Time penalty */
    }
    else
      if (!pushudbags(clfirst,clcoll)) /* Vertical push */
        push=FALSE;
    if (mongotgold) /* No time penalty if monster eats gold */
      mp->t=0;
    if (!mp->nob && mp->hnt>1)
      removebags(clfirst,clcoll); /* Hobbins eat bags */
  }

  /* Increase hobbin cross counter */

  if (mp->nob && clfirst[2]!=-1 && isalive())
    mp->hnt++;

  /* See if bags push monster back */

  if (!push) {
    mp->x=monox;
    mp->y=monoy;
    mh=mp->h;
    mv=mp->v;
    mxr=mp->xr;
    myr=mp->yr;
    drawmon(mon,mp->nob,mp->hdir,mp->x,mp->y);
    incpenalty();
    if (mp->nob) /* The other way to create hobbin: stuck on h-bag */
      mp->hnt++;
    if ((mp->dir==DIR_UP || mp->dir==DIR_DOWN) &&
        mp->nob)
      mp->dir=reversedir(mp->dir); /* If vertical, give up */
  }

  /* Collision with Digger */

  if (clfirst[4]!=-1 && isalive()) {
    if (bonusmode) {
      killmon(mon);
      i=clfirst[4];
      while (i!=-1) {
        if (digalive(i-FIRSTDIGGER+curplayer))
          sceatm(i-FIRSTDIGGER+curplayer);
        i=clcoll[i];
      }
      soundeatm(); /* Collision in bonus mode */
    }
    else {
      i=clfirst[4];
      while (i!=-1) {
        if (digalive(i-FIRSTDIGGER+curplayer))
          killdigger(i-FIRSTDIGGER+curplayer,3,0); /* Kill Digger */
        i=clcoll[i];
      }
    }
  }

  /* Update co-ordinates */

  mp->h=mh;
  mp->v=mv;
  mp->xr=mxr;
  mp->yr=myr;
}

void mondie(Sint4 mon)
{
  switch (mondat[mon].death) {
    case 1:
      if (bagy(mondat[mon].bag)+6>mondat[mon].y)
        mondat[mon].y=bagy(mondat[mon].bag);
      drawmondie(mon,mondat[mon].nob,mondat[mon].hdir,mondat[mon].x,
                 mondat[mon].y);
      incpenalty();
      if (getbagdir(mondat[mon].bag)==-1) {
        mondat[mon].dtime=1;
        mondat[mon].death=4;
      }
      break;
    case 4:
      if (mondat[mon].dtime!=0)
        mondat[mon].dtime--;
      else {
        killmon(mon);
        if (diggers==2)
          scorekill2();
        else
          scorekill(curplayer);
      }
  }
}

#ifdef DIGGER_ELKS
#define MONFIELDIDX(h,v) (((v)<<4)-(v)+(h))

bool fieldclear(Sint4 dir,Sint4 x,Sint4 y)
{
  Sint4 base,fcur,fnext;
  base=MONFIELDIDX(x,y);
  fcur=field[base];
  switch (dir) {
    case DIR_RIGHT:
      if (x<14) {
        fnext=field[base+1];
        if ((fnext&0x2000)==0)
          if ((fnext&1)==0 || (fcur&0x10)==0)
            return TRUE;
      }
      break;
    case DIR_UP:
      if (y>0) {
        fnext=field[base-MWIDTH];
        if ((fnext&0x2000)==0)
          if ((fnext&0x800)==0 || (fcur&0x40)==0)
            return TRUE;
      }
      break;
    case DIR_LEFT:
      if (x>0) {
        fnext=field[base-1];
        if ((fnext&0x2000)==0)
          if ((fnext&0x10)==0 || (fcur&1)==0)
            return TRUE;
      }
      break;
    case DIR_DOWN:
      if (y<9) {
        fnext=field[base+MWIDTH];
        if ((fnext&0x2000)==0)
          if ((fnext&0x40)==0 || (fcur&0x800)==0)
            return TRUE;
      }
  }
  return FALSE;
}
#else
bool fieldclear(Sint4 dir,Sint4 x,Sint4 y)
{
  Sint4 fcur,fnext;
  switch (dir) {
    case DIR_RIGHT:
      if (x<14) {
        fnext=getfield(x+1,y);
        if ((fnext&0x2000)==0) {
          fcur=getfield(x,y);
          if ((fnext&1)==0 || (fcur&0x10)==0)
            return TRUE;
        }
      }
      break;
    case DIR_UP:
      if (y>0) {
        fnext=getfield(x,y-1);
        if ((fnext&0x2000)==0) {
          fcur=getfield(x,y);
          if ((fnext&0x800)==0 || (fcur&0x40)==0)
            return TRUE;
        }
      }
      break;
    case DIR_LEFT:
      if (x>0) {
        fnext=getfield(x-1,y);
        if ((fnext&0x2000)==0) {
          fcur=getfield(x,y);
          if ((fnext&0x10)==0 || (fcur&1)==0)
            return TRUE;
        }
      }
      break;
    case DIR_DOWN:
      if (y<9) {
        fnext=getfield(x,y+1);
        if ((fnext&0x2000)==0) {
          fcur=getfield(x,y);
          if ((fnext&0x40)==0 || (fcur&0x800)==0)
            return TRUE;
        }
      }
  }
  return FALSE;
}
#endif

void checkmonscared(Sint4 h)
{
  Sint4 m;
  for (m=0;m<MONSTERS;m++)
    if (h==mondat[m].h && mondat[m].dir==DIR_UP)
      mondat[m].dir=DIR_DOWN;
}

void killmon(Sint4 mon)
{
  if (mondat[mon].flag) {
    mondat[mon].flag=mondat[mon].alive=FALSE;
    erasespr(mon+FIRSTMONSTER);
    if (bonusmode)
      totalmonsters++;
  }
}

void squashmonsters(Sint4 bag,Sint3 *clfirst,Sint3 *clcoll)
{
  int next=clfirst[2],m;
  while (next!=-1) {
    m=next-FIRSTMONSTER;
    if (mondat[m].y>=bagy(bag))
      squashmonster(m,1,bag);
    next=clcoll[next];
  }
}

Sint4 killmonsters(Sint3 *clfirst,Sint3 *clcoll)
{
  int next=clfirst[2],m,n=0;
  while (next!=-1) {
    m=next-FIRSTMONSTER;
    killmon(m);
    n++;
    next=clcoll[next];
  }
  return n;
}

void squashmonster(Sint4 mon,Sint4 death,Sint4 bag)
{
  mondat[mon].alive=FALSE;
  mondat[mon].death=death;
  mondat[mon].bag=bag;
}

Sint4 monleft(void)
{
  return nmononscr()+totalmonsters-nextmonster;
}

Sint4 nmononscr(void)
{
  Sint4 i,n=0;
  for (i=0;i<MONSTERS;i++)
    if (mondat[i].flag)
      n++;
  return n;
}

void incmont(Sint4 n)
{
  Sint4 m;
  if (n>MONSTERS)
    n=MONSTERS;
  for (m=1;m<n;m++)
    mondat[m].t++;
}

#ifndef DIGGER_ELKS
Sint4 getfield(Sint4 x,Sint4 y)
{
  return field[y*15+x];
}
#endif
