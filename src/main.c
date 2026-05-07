/* Digger Remastered
   Copyright (c) Andrew Jenner 1998-2004 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "def.h"
#include "hardware.h"
#include "sound.h"
#include "sprite.h"
#include "input.h"
#include "scores.h"
#include "drawing.h"
#include "digger.h"
#include "monster.h"
#ifdef DIGGER_CGA_PROFILE
void elks_dump_profile(void);
unsigned int elks_game_start_count;
unsigned int elks_penalty_frame_count;
unsigned int elks_penalty_max;
unsigned int elks_incmont_count;
#endif
#include "bags.h"
#include "record.h"
#include "replay.h"
#include "main.h"
#ifndef DIGGER_ELKS
#include "newsnd.h"
#include "ini.h"
#endif

#ifdef _WINDOWS
#include "win_dig.h"
#include "win_snd.h"
#include "win_vid.h"
#endif

struct game
{
  Sint4 level;
  bool levdone;
} gamedat[2];

char pldispbuf[14];

Sint4 curplayer=0,nplayers=1,penalty=0,diggers=1,startlev=1;

bool levnotdrawn=FALSE,alldead=FALSE,unlimlives=FALSE,started;

char levfname[FILENAME_BUFFER_SIZE];
bool levfflag=FALSE;
bool biosflag=FALSE;
Sint5 delaytime=0;
int gtime=0;
bool gauntlet=FALSE,timeout=FALSE,synchvid=FALSE;

void shownplayers(void);
void switchnplayers(void);
void drawscreen(void);
void initchars(void);
void checklevdone(void);
Sint4 levno(void);
void testpause(void);
void calibrate(void);
void parsecmd(int argc,char *argv[]);
void patchcga(void);
void initlevel(void);
void finish(void);
void inir(void);
void redefkeyb(bool allf);
int getalllives(void);

Sint3 leveldat[8][MHEIGHT][MWIDTH]=
{{"S   B     HHHHS",
  "V  CC  C  V B  ",
  "VB CC  C  V    ",
  "V  CCB CB V CCC",
  "V  CC  C  V CCC",
  "HH CC  C  V CCC",
  " V    B B V    ",
  " HHHH     V    ",
  "C   V     V   C",
  "CC  HHHHHHH  CC"},
 {"SHHHHH  B B  HS",
  " CC  V       V ",
  " CC  V CCCCC V ",
  "BCCB V CCCCC V ",
  "CCCC V       V ",
  "CCCC V B  HHHH ",
  " CC  V CC V    ",
  " BB  VCCCCV CC ",
  "C    V CC V CC ",
  "CC   HHHHHH    "},
 {"SHHHHB B BHHHHS",
  "CC  V C C V BB ",
  "C   V C C V CC ",
  " BB V C C VCCCC",
  "CCCCV C C VCCCC",
  "CCCCHHHHHHH CC ",
  " CC  C V C  CC ",
  " CC  C V C     ",
  "C    C V C    C",
  "CC   C H C   CC"},
 {"SHBCCCCBCCCCBHS",
  "CV  CCCCCCC  VC",
  "CHHH CCCCC HHHC",
  "C  V  CCC  V  C",
  "   HHH C HHH   ",
  "  B  V B V  B  ",
  "  C  VCCCV  C  ",
  " CCC HHHHH CCC ",
  "CCCCC CVC CCCCC",
  "CCCCC CHC CCCCC"},
 {"SHHHHHHHHHHHHHS",
  "VBCCCCBVCCCCCCV",
  "VCCCCCCV CCBC V",
  "V CCCC VCCBCCCV",
  "VCCCCCCV CCCC V",
  "V CCCC VBCCCCCV",
  "VCCBCCCV CCCC V",
  "V CCBC VCCCCCCV",
  "VCCCCCCVCCCCCCV",
  "HHHHHHHHHHHHHHH"},
 {"SHHHHHHHHHHHHHS",
  "VCBCCV V VCCBCV",
  "VCCC VBVBV CCCV",
  "VCCCHH V HHCCCV",
  "VCC V CVC V CCV",
  "VCCHH CVC HHCCV",
  "VC V CCVCC V CV",
  "VCHHBCCVCCBHHCV",
  "VCVCCCCVCCCCVCV",
  "HHHHHHHHHHHHHHH"},
 {"SHCCCCCVCCCCCHS",
  " VCBCBCVCBCBCV ",
  "BVCCCCCVCCCCCVB",
  "CHHCCCCVCCCCHHC",
  "CCV CCCVCCC VCC",
  "CCHHHCCVCCHHHCC",
  "CCCCV CVC VCCCC",
  "CCCCHH V HHCCCC",
  "CCCCCV V VCCCCC",
  "CCCCCHHHHHCCCCC"},
 {"HHHHHHHHHHHHHHS",
  "V CCBCCCCCBCC V",
  "HHHCCCCBCCCCHHH",
  "VBV CCCCCCC VBV",
  "VCHHHCCCCCHHHCV",
  "VCCBV CCC VBCCV",
  "VCCCHHHCHHHCCCV",
  "VCCCC V V CCCCV",
  "VCCCCCV VCCCCCV",
  "HHHHHHHHHHHHHHH"}};

Sint4 getlevch(Sint4 x,Sint4 y,Sint4 l)
{
  if ((l==3 || l==4) && !levfflag && diggers==2 && y==9 && (x==6 || x==8))
    return 'H';
  return leveldat[l-1][y][x];
}

#ifdef INTDRF
extern FILE *info;
#endif

void game(void)
{
  Sint4 t,c,i;
  bool flashplayer=FALSE;
#ifdef DIGGER_ELKS
  bool startpause=TRUE;
#endif
#ifdef DIGGER_CGA_PROFILE
  elks_game_start_count++;
#endif
  penalty=0;
#ifdef _WINDOWS
  show_game_menu();
#endif
  if (gauntlet) {
    cgtime=gtime*1193181l;
    timeout=FALSE;
  }
  initlives();
  gamedat[0].level=startlev;
  if (nplayers==2)
    gamedat[1].level=startlev;
  alldead=FALSE;
  gclear();
  curplayer=0;
  initlevel();
  curplayer=1;
  initlevel();
  zeroscores();
  bonusvisible=TRUE;
  if (nplayers==2)
    flashplayer=TRUE;
  curplayer=0;
  while (getalllives()!=0 && !escape && !timeout) {
    while (!alldead && !escape && !timeout) {
      initmbspr();

#ifdef DIGGER_REPLAY
      if (replay_is_playing())
        randv=replay_play_seed();
      else
        randv=getlrt();
#else
      if (playing)
        randv=playgetrand();
      else
        randv=getlrt();
#endif
#ifdef INTDRF
      fprintf(info,"%lu\n",randv);
      frame=0;
#endif
#ifdef DIGGER_REPLAY
      if (replay_is_recording())
        replay_begin_block(randv);
#else
      recputrand(randv);
#endif
      if (levnotdrawn) {
        levnotdrawn=FALSE;
        drawscreen();
        if (flashplayer) {
          flashplayer=FALSE;
          strcpy(pldispbuf,"PLAYER ");
          if (curplayer==0)
            strcat(pldispbuf,"1");
          else
            strcat(pldispbuf,"2");
          cleartopline();
          for (t=0;t<15;t++)
            for (c=1;c<=3;c++) {
              outtext(pldispbuf,108,0,c);
              writecurscore(c);
              newframe();
              if (escape)
                return;
            }
          drawscores();
          for (i=0;i<diggers;i++)
            addscore(i,0);
        }
      }
      else
        initchars();
      erasetext(8,108,0,3);
      initscores();
      drawlives();
      music(1);

      flushkeybuf();
#ifdef DIGGER_ELKS
      if (startpause) {
        startpause=FALSE;
#ifdef DIGGER_REPLAY
        if (!replay_is_playing()) {
#endif
          pausef=TRUE;
          testpause();
          flushkeybuf();
#ifdef DIGGER_REPLAY
        }
#endif
      }
#endif
      for (i=0;i<diggers;i++)
        readdir(i);
      while (!alldead && !gamedat[curplayer].levdone && !escape && !timeout) {
        penalty=0;
        dodigger();
        domonsters();
        dobags();
#ifdef DIGGER_CGA_PROFILE
        if (penalty>0)
          elks_penalty_frame_count++;
#endif
        if (penalty>8) {
#ifdef DIGGER_CGA_PROFILE
          elks_incmont_count++;
#endif
          incmont(penalty-8);
        }
        testpause();
        checklevdone();
      }
      erasediggers();
      musicoff();
      t=20;
      while ((getnmovingbags()!=0 || t!=0) && !escape && !timeout) {
        if (t!=0)
          t--;
        penalty=0;
        dobags();
        dodigger();
        domonsters();
#ifdef DIGGER_CGA_PROFILE
        if (penalty>0)
          elks_penalty_frame_count++;
#endif
        if (penalty<8)
          t=0;
      }
      soundstop();
      for (i=0;i<diggers;i++)
        killfire(i);
      erasebonus();
      cleanupbags();
      savefield();
      erasemonsters();
#ifdef DIGGER_REPLAY
      if (replay_has_request())
        replay_end_block();
      else {
#endif
        recputeol();
        if (playing)
          playskipeol();
        if (escape)
          recputeog();
#ifdef DIGGER_REPLAY
      }
#endif
      if (gamedat[curplayer].levdone)
        soundlevdone();
      if (countem()==0 || gamedat[curplayer].levdone) {
#ifdef INTDRF
        fprintf(info,"%i\n",frame);
#endif
        for (i=curplayer;i<diggers+curplayer;i++)
          if (getlives(i)>0 && !digalive(i))
            declife(i);
        drawlives();
        gamedat[curplayer].level++;
        if (gamedat[curplayer].level>1000)
          gamedat[curplayer].level=1000;
        initlevel();
      }
      else
        if (alldead) {
#ifdef INTDRF
          fprintf(info,"%i\n",frame);
#endif
          for (i=curplayer;i<curplayer+diggers;i++)
            if (getlives(i)>0)
              declife(i);
          drawlives();
        }
      if ((alldead && getalllives()==0 && !gauntlet && !escape) || timeout)
        endofgame();
    }
    alldead=FALSE;
    if (nplayers==2 && getlives(1-curplayer)!=0) {
      curplayer=1-curplayer;
      flashplayer=levnotdrawn=TRUE;
    }
  }
#ifdef INTDRF
  fprintf(info,"-1\n%lu\n%i",getscore0(),gamedat[0].level);
#endif
}

void maininit(void)
{
  calibrate();
  inittimer();
  initkeyb();
  ginit();
  gpal(0);
  setretr(TRUE);
  detectjoy();
  inir();
  initsound();
  recstart();
}

#ifndef _WINDOWS
int main(int argc,char *argv[])
{
  maininit();
  parsecmd(argc,argv);
  return mainprog();
}
#endif

#ifdef _WINDOWS
  Sint4 frame;
#endif

int mainprog(void)
{
  Sint4 t,x=0;
#ifndef _WINDOWS
  Sint4 frame;
#endif
  loadscores();
  escape=FALSE;
#ifdef DIGGER_REPLAY
  if (replay_has_request() && replay_play_header()) {
    game();
    replay_finish();
    finish();
    return replay_exit_status();
  }
  if (replay_has_request() && replay_exit_status()) {
    finish();
    return replay_exit_status();
  }
#endif
  do {
#ifdef _WINDOWS
    show_main_menu();
#endif
    soundstop();
    creatembspr();
    detectjoy();
    gclear();
    gtitle();
    outtext("D I G G E R",100,0,3);
    shownplayers();
    showtable();
    started=FALSE;
#ifdef DIGGER_ELKS
    elks_input_flush_pending();
#endif
    frame=0;
    newframe();
    teststart();
#ifdef _WINDOWS
    reset_main_menu_screen=FALSE;
#endif
    while (!started) {
      started=teststart();
      if ((akeypressed==27 || akeypressed=='n' || akeypressed=='N') &&
          !gauntlet && diggers==1) {
        switchnplayers();
        shownplayers();
        akeypressed=0;
      }
      if (frame==0)
        for (t=54;t<174;t+=12)
          erasetext(12,164,t,0);
      if (frame==50) {
        movedrawspr(FIRSTMONSTER,292,63);
        x=292;
      }
      if (frame>50 && frame<=77) {
        x-=4;
        drawmon(0,1,DIR_LEFT,x,63);
      }
      if (frame>77)
        drawmon(0,1,DIR_RIGHT,184,63);
      if (frame==83)
        outtext("NOBBIN",216,64,2);
      if (frame==90) {
        movedrawspr(FIRSTMONSTER+1,292,82);
        drawmon(1,0,DIR_LEFT,292,82);
        x=292;
      }
      if (frame>90 && frame<=117) {
        x-=4;
        drawmon(1,0,DIR_LEFT,x,82);
      }
      if (frame>117)
        drawmon(1,0,DIR_RIGHT,184,82);
      if (frame==123)
        outtext("HOBBIN",216,83,2);
      if (frame==130) {
        movedrawspr(FIRSTDIGGER,292,101);
        drawdigger(0,DIR_LEFT,292,101,1);
        x=292;
      }
      if (frame>130 && frame<=157) {
        x-=4;
        drawdigger(0,DIR_LEFT,x,101,1);
      }
      if (frame>157)
        drawdigger(0,DIR_RIGHT,184,101,1);
      if (frame==163)
        outtext("DIGGER",216,102,2);
      if (frame==178) {
        movedrawspr(FIRSTBAG,184,120);
        drawgold(0,0,184,120);
      }
      if (frame==183)
        outtext("GOLD",216,121,2);
      if (frame==198)
        drawemerald(184,141);
      if (frame==203)
        outtext("EMERALD",216,140,2);
      if (frame==218)
        drawbonus(184,158);
      if (frame==223)
        outtext("BONUS",216,159,2);
      newframe();
      frame++;
      if (frame>250)
        frame=0;
#ifdef _WINDOWS
      if (reset_main_menu_screen) {
        escape=FALSE;
        break;
      }
#endif
    }
#ifndef DIGGER_RECORD_STUB
    if (savedrf) {
      if (gotgame)
        recsavedrf();
      savedrf=FALSE;
      continue;
    }
#endif
#ifdef _WINDOWS
    if (reset_main_menu_screen)
      continue;
#endif
    if (escape)
      break;
#ifdef DIGGER_REPLAY
    replay_record_header();
    if (escape)
      break;
#endif
    recinit();
    game();
#ifdef DIGGER_REPLAY
    replay_finish();
    if (replay_has_request())
      break;
#endif
#ifndef DIGGER_RECORD_STUB
    gotgame=TRUE;
    if (gotname)
      recsavedrf();
    savedrf=FALSE;
#endif
    escape=FALSE;
  } while (!escape);
  finish();
#ifdef DIGGER_REPLAY
  if (replay_has_request())
    return replay_exit_status();
#endif
  return 0;
}

void finish(void)
{
  killsound();
  soundoff();
  soundkillglob();
#ifdef DIGGER_CGA_PROFILE
  /* Leave the ELKS keyboard in raw mode until after the profile screen has
   * waited for a key.  Otherwise canonical terminal mode would require Enter,
   * and on some ELKS/emulator combinations the shell repaints immediately
   * after exit, making the counters visible only for a fraction of a second.
   */
  graphicsoff();
  elks_dump_profile();
  restorekeyb();
#else
  restorekeyb();
  graphicsoff();
#endif
#ifdef _WINDOWS
  windows_finish();
#endif
}

void shownplayers(void)
{
  if (diggers==2)
    if (gauntlet) {
      outtext("TWO PLAYER",180,25,3);
      outtext("GAUNTLET",192,39,3);
    }
    else {
      outtext("TWO PLAYER",180,25,3);
      outtext("SIMULTANEOUS",170,39,3);
    }
  else
    if (gauntlet) {
      outtext("GAUNTLET",192,25,3);
      outtext("MODE",216,39,3);
    }
    else
      if (nplayers==1) {
        outtext("ONE",220,25,3);
        outtext(" PLAYER ",192,39,3);
      }
      else {
        outtext("TWO",220,25,3);
        outtext(" PLAYERS",184,39,3);
      }
}

int getalllives(void)
{
  int t=0,i;
  for (i=curplayer;i<diggers+curplayer;i++)
    t+=getlives(i);
  return t;
}

void switchnplayers(void)
{
  nplayers=3-nplayers;
}

void initlevel(void)
{
  gamedat[curplayer].levdone=FALSE;
  makefield();
  makeemfield();
  initbags();
  levnotdrawn=TRUE;
}

void drawscreen(void)
{
  creatembspr();
  drawstatics();
  drawbags();
  drawemeralds();
  initdigger();
  initmonsters();
}


void initchars(void)
{
  initmbspr();
  initdigger();
  initmonsters();
}

void checklevdone(void)
{
  if ((countem()==0 || monleft()==0) && isalive())
    gamedat[curplayer].levdone=TRUE;
  else
    gamedat[curplayer].levdone=FALSE;
}

void incpenalty(void)
{
  penalty++;
#ifdef DIGGER_CGA_PROFILE
  if ((unsigned int)penalty > elks_penalty_max)
    elks_penalty_max = (unsigned int)penalty;
#endif
}

void cleartopline(void)
{
  erasetext(26,0,0,3);
  erasetext(1,308,0,3);
}

Sint4 levplan(void)
{
  Sint4 l=levno();
  if (l>8)
    l=(l&3)+5; /* Level plan: 12345678, 678, (5678) 247 times, 5 forever */
  return l;
}

Sint4 levof10(void)
{
  if (gamedat[curplayer].level>10)
    return 10;
  return gamedat[curplayer].level;
}

Sint4 levno(void)
{
  return gamedat[curplayer].level;
}

void setdead(bool df)
{
  alldead=df;
}

void testpause(void)
{
  int i;
  if (pausef) {
    pausef=FALSE;
    soundpause();
    sett2val(40);
    setsoundt2();
    cleartopline();
    outtext("PRESS ANY KEY",80,0,1);
    getkey();
    cleartopline();
    drawscores();
    for (i=0;i<diggers;i++)
      addscore(i,0);
    drawlives();
    if (!synchvid)
      curtime=gethrt();
  }
  else
    soundpauseoff();
}

void calibrate(void)
{
  volume=(Sint4)(getkips()/291);
  if (volume==0)
    volume=1;
}

#ifndef DIGGER_ELKS
Uint4 sound_device,sound_port,sound_irq,sound_dma,sound_rate,sound_length;
#endif

#ifdef DIGGER_ELKS
static Sint4 elks_argnum(char *p)
{
  Sint4 v=0;
  while (*p>='0' && *p<='9')
    v=(Sint4)(v*10+*p++-'0');
  return v;
}

static void elks_copy_levname(char *p)
{
  Sint4 j=0;
  while (*p!=0 && j<(FILENAME_BUFFER_SIZE-5))
    levfname[j++]=*p++;
  levfname[j]=0;
  levfflag=(j!=0) ? TRUE : FALSE;
}

static void elks_load_levfile(void)
{
  FILE *levf;
  Sint4 j;

  if (!levfflag)
    return;

  levf=fopen(levfname,"rb");
  if (levf==NULL) {
    j=0;
    while (levfname[j]!=0)
      j++;
    if (j < (FILENAME_BUFFER_SIZE-4)) {
      levfname[j++]='.';
      levfname[j++]='D';
      levfname[j++]='L';
      levfname[j++]='F';
      levfname[j]=0;
      levf=fopen(levfname,"rb");
    }
  }

  if (levf==NULL) {
    levfflag=FALSE;
    return;
  }
  fread(&bonusscore,2,1,levf);
  fread(leveldat,1200,1,levf);
  fclose(levf);
}

void parsecmd(int argc,char *argv[])
{
  Sint4 arg,i,speedmul;
  char *word;

  for (arg=1;arg<argc;arg++) {
    word=argv[arg];
    if (word[0]=='/' || word[0]=='-') {
      i=(word[2]==':') ? 3 : 2;
      switch (word[1]) {
        case 'L': case 'l':
          elks_copy_levname(word+i);
          break;
        case 'S': case 's':
          speedmul=elks_argnum(word+i);
          ftime=(Sint5)speedmul*2000l;
          break;
        case 'I': case 'i':
          startlev=elks_argnum(word+i);
          break;
        case 'U': case 'u':
          unlimlives=TRUE;
          break;
        case 'Q': case 'q':
          soundflag=FALSE;
          break;
        case 'M': case 'm':
          musicflag=FALSE;
          break;
        case '2':
          diggers=2;
          break;
        case 'K': case 'k':
          redefkeyb((word[2]=='A' || word[2]=='a') ? TRUE : FALSE);
          break;
        case 'G': case 'g':
          gtime=elks_argnum(word+i);
          if (gtime>3599)
            gtime=3599;
          if (gtime==0)
            gtime=120;
          gauntlet=TRUE;
          break;
#ifdef DIGGER_REPLAY
        case 'R': case 'r':
          replay_set_record_name(word+i);
          break;
        case 'P': case 'p':
          replay_set_play_name(word+i);
          break;
        case 'F': case 'f':
          replay_set_fast();
          break;
#endif
        case '?': case 'h': case 'H':
          finish();
          puts("DIGGER ELKS "DIGGER_VERSION);
#ifdef DIGGER_REPLAY
          puts("Options: /S:n /I:n /L:file /Q /M /G[:n] /2 /U /K[A] /R:file /P:file /F");
#else
          puts("Options: /S:n /I:n /L:file /Q /M /G[:n] /2 /U /K[A]");
#endif
          exit(1);
      }
    }
    else {
      speedmul=elks_argnum(word);
      if (speedmul!=0 && word[0]>='0' && word[0]<='9')
        ftime=(Sint5)speedmul*2000l;
      else
        elks_copy_levname(word);
    }
  }
  elks_load_levfile();
}
#else
void parsecmd(int argc,char *argv[])
{
  char *word;
  Sint4 arg,i,j,speedmul;
  bool sf,gs=FALSE,norepf=FALSE;
  FILE *levf;

  for (arg=1;arg<argc;arg++) {
    word=argv[arg];
    if (word[0]=='/' || word[0]=='-') {
      i=(word[2]==':') ? 3 : 2;
      if (word[1]=='L' || word[1]=='l') {
        j=0;
        while (word[i]!=0)
          levfname[j++]=word[i++];
        levfname[j]=word[i];
        levfflag=TRUE;
      }
      if (word[1]=='R' || word[1]=='r')
        recname(word+i);
      if (word[1]=='P' || word[1]=='p' || word[1]=='E' || word[1]=='e') {
        openplay(word+i);
        if (escape)
          norepf=TRUE;
      }
      if (word[1]=='E' || word[1]=='e') {
        finish();
        if (escape)
          exit(0);
        exit(1);
      }
      if ((word[1]=='O' || word[1]=='o') && !norepf) {
        arg=0;
        continue;
      }
      if (word[1]=='S' || word[1]=='s') {
        speedmul=0;
        while (word[i]!=0)
          speedmul=10*speedmul+word[i++]-'0';
        ftime=speedmul*2000l;
        gs=TRUE;
      }
      if (word[1]=='I' || word[1]=='i')
        startlev=(Sint4)atoi(word+i);
      if (word[1]=='U' || word[1]=='u')
        unlimlives=TRUE;
#ifndef _WINDOWS
      if (word[1]=='?' || word[1]=='h' || word[1]=='H') {
        finish();
#ifdef DIGGER_ELKS
        printf("DIGGER ELKS "DIGGER_VERSION"\n"
               "Options: /S:n /I:n /L:file /Q /M /G[:n] /2 /U\n");
#else
        printf("DIGGER - Copyright (c) 1983 Windmill software\n"
               "Restored 1998 by AJ Software\n"
#ifdef ARM
               "Acorn port by Julian Brown\n"
#endif
               "http://www.digger.org\n"
               "Version: "DIGGER_VERSION"\n\n"

               "Command line syntax:\n"
               "  DIGGER [[/S:]speed] [[/L:]level file] [/C] [/B] [/Q] [/M] "
                                                         "[/P:playback file]\n"
               "         [/E:playback file] [/R:record file] [/O] [/K[A]] "
                                                           "[/G[:time]] [/2]\n"
               "         [/A:device,port,irq,dma,rate,length] [/V] [/U] "
                                                               "[/I:level]\n\n"
               "/C = Use CGA graphics\n"
               "/B = Use BIOS palette functions for CGA (slow!)\n"
               "/Q = Quiet mode (no sound at all)       "
               "/M = No music\n"
               "/R = Record graphics to file\n"
               "/P = Playback and restart program       "
               "/E = Playback and exit program\n"
               "/O = Loop to beginning of command line\n"
               "/K = Redefine keyboard\n"
               "/G = Gauntlet mode\n"
               "/2 = Two player simultaneous mode\n"
               "/A = Use alternate sound device\n"
               "/V = Synchronize timing to vertical retrace\n"
               "/U = Allow unlimited lives\n"
               "/I = Start on a level other than 1\n");
#endif
        exit(1);
      }
#endif
      if (word[1]=='Q' || word[1]=='q')
        soundflag=FALSE;
      if (word[1]=='M' || word[1]=='m')
        musicflag=FALSE;
      if (word[1]=='2')
        diggers=2;
#ifndef _WINDOWS
      if (word[1]=='B' || word[1]=='b' || word[1]=='C' || word[1]=='c') {
        ginit=cgainit;
        gpal=cgapal;
        ginten=cgainten;
        gclear=cgaclear;
        ggetpix=cgagetpix;
        gputi=cgaputi;
        ggeti=cgageti;
        gputim=cgaputim;
        gwrite=cgawrite;
        gtitle=cgatitle;
        if (word[1]=='B' || word[1]=='b')
          biosflag=TRUE;
        ginit();
        gpal(0);
      }
      if (word[1]=='K' || word[1]=='k') {
        if (word[2]=='A' || word[2]=='a')
          redefkeyb(TRUE);
        else
          redefkeyb(FALSE);
      }
#ifndef DIGGER_ELKS
      if (word[1]=='A' || word[1]=='a') {
        sscanf(word+i,"%hu,%hx,%hu,%hu,%hu,%hu",&sound_device,&sound_port,
               &sound_irq,&sound_dma,&sound_rate,&sound_length);
        killsound();
        if (sound_device>0) {
          volume=1;
          setupsound=s1setupsound;
          killsound=s1killsound;
          fillbuffer=s1fillbuffer;
          initint8=s1initint8;
          restoreint8=s1restoreint8;
          if (sound_device==1) {
            soundoff=s1soundoff;
            setspkrt2=s1setspkrt2;
            settimer0=s1settimer0;
            timer0=s1timer0;
            settimer2=s1settimer2;
            timer2=s1timer2;
            getsample=getsample1;
          }
          else {
            soundoff=s2soundoff;
            setspkrt2=s2setspkrt2;
            settimer0=s2settimer0;
            timer0=s2timer0;
            settimer2=s2settimer2;
            timer2=s2timer2;
            getsample=getsample2;
          }
          soundinitglob(sound_port,sound_irq,sound_dma,sound_length,sound_rate);
        }
        initsound();
      }
#endif
      if (word[1]=='V' || word[1]=='v')
        synchvid=TRUE;
#endif
      if (word[1]=='G' || word[1]=='g') {
        gtime=0;
        while (word[i]!=0)
          gtime=10*gtime+word[i++]-'0';
        if (gtime>3599)
          gtime=3599;
        if (gtime==0)
          gtime=120;
        gauntlet=TRUE;
      }
    }
    else {
      i=strlen(word);
      if (i<1)
        continue;
      sf=TRUE;
      if (!gs)
        for (j=0;j<i;j++)
          if (word[j]<'0' || word[j]>'9') {
            sf=FALSE;
            break;
          }
      if (sf) {
        speedmul=0;
        j=0;
        while (word[j]!=0)
          speedmul=10*speedmul+word[j++]-'0';
        gs=TRUE;
        ftime=speedmul*2000l;
      }
      else {
        j=0;
        while (word[j]!=0) {
          levfname[j]=word[j];
          j++;
        }
        levfname[j]=word[j];
        levfflag=TRUE;
      }
    }
  }

  if (levfflag) {
    levf=fopen(levfname,"rb");
    if (levf==NULL) {
      strcat(levfname,".DLF");
      levf=fopen(levfname,"rb");
    }
    if (levf==NULL)
      levfflag=FALSE;
    else {
      fread(&bonusscore,2,1,levf);
      fread(leveldat,1200,1,levf);
      fclose(levf);
    }
  }
}
#endif

Sint5 randv;

Sint4 randno(Sint4 n)
{
  randv=randv*0x15a4e35l+1;
  return (Sint4)((randv&0x7fffffffl)%n);
}

char *keynames[17]={"Right","Up","Left","Down","Fire",
                    "Right","Up","Left","Down","Fire",
                    "Cheat","Accel","Brake","Music","Sound","Exit","Pause"};

#if !defined(_WINDOWS) && !defined(DIGGER_ELKS)
int dx_sound_volume;
bool g_bWindowed,use_640x480_fullscreen,use_async_screen_updates;
#endif

#ifdef DIGGER_ELKS
#ifndef DIGGER_ELKS_DEFAULT_FTIME
#define DIGGER_ELKS_DEFAULT_FTIME 80000l
#endif

void inir(void)
{
  int i;

  /* ELKS has no useful INI path in this port.  Keep the same built-in
   * defaults that the old INI-stub path produced, but avoid compiling the
   * DOS/Windows INI parser, string formatting, compatibility helpers, and
   * legacy sampled-sound device selection code.
   */
  for (i=0;i<17;i++)
    krdf[i]=TRUE;

  diggers=1;
  nplayers=1;
  gauntlet=FALSE;
  gtime=120;
  ftime=(Uint5)DIGGER_ELKS_DEFAULT_FTIME;
  soundflag=TRUE;
  musicflag=TRUE;
  synchvid=FALSE;
  biosflag=FALSE;
  unlimlives=FALSE;
  startlev=1;
  levfname[0]=0;
  levfflag=FALSE;
}
#else
void inir(void)
{
  char kbuf[80],vbuf[80];
  int i,j,p;
  bool cgaflag;

  for (i=0;i<17;i++) {
    sprintf(kbuf,"%s%c",keynames[i],(i>=5 && i<10) ? '2' : 0);
    sprintf(vbuf,"%i/%i/%i/%i/%i",keycodes[i][0],keycodes[i][1],
            keycodes[i][2],keycodes[i][3],keycodes[i][4]);
    GetINIString(INI_KEY_SETTINGS,kbuf,vbuf,vbuf,80,ININAME);
    krdf[i]=TRUE;
    p=0;
    for (j=0;j<5;j++) {
      keycodes[i][j]=atoi(vbuf+p);
      while (vbuf[p]!='/' && vbuf[p]!=0)
        p++;
      if (vbuf[p]==0)
        break;
      p++;
    }
  }
  gtime=(int)GetINIInt(INI_GAME_SETTINGS,"GauntletTime",120,ININAME);
  ftime=GetINIInt(INI_GAME_SETTINGS,"Speed",80000l,ININAME);
  gauntlet=GetINIBool(INI_GAME_SETTINGS,"GauntletMode",FALSE,ININAME);
  GetINIString(INI_GAME_SETTINGS,"Players","1",vbuf,80,ININAME);
  strupr(vbuf);
  if (vbuf[0]=='2' && vbuf[1]=='S') {
    diggers=2;
    nplayers=1;
  }
  else {
    diggers=1;
    nplayers=atoi(vbuf);
    if (nplayers<1 || nplayers>2)
      nplayers=1;
  }
  soundflag=GetINIBool(INI_SOUND_SETTINGS,"SoundOn",TRUE,ININAME);
  musicflag=GetINIBool(INI_SOUND_SETTINGS,"MusicOn",TRUE,ININAME);
  sound_device=(int)GetINIInt(INI_SOUND_SETTINGS,"Device",DEF_SND_DEV,ININAME);
  sound_port=(int)GetINIInt(INI_SOUND_SETTINGS,"Port",544,ININAME);
  sound_irq=(int)GetINIInt(INI_SOUND_SETTINGS,"Irq",5,ININAME);
  sound_dma=(int)GetINIInt(INI_SOUND_SETTINGS,"DMA",1,ININAME);
  sound_rate=(int)GetINIInt(INI_SOUND_SETTINGS,"Rate",22050,ININAME);
  sound_length=(int)GetINIInt(INI_SOUND_SETTINGS,"BufferSize",DEFAULT_BUFFER,
                              ININAME);
  if (sound_device>0) {
    volume=1;
    setupsound=s1setupsound;
    killsound=s1killsound;
    fillbuffer=s1fillbuffer;
    initint8=s1initint8;
    restoreint8=s1restoreint8;
    if (sound_device==1) {
      soundoff=s1soundoff;
      setspkrt2=s1setspkrt2;
      settimer0=s1settimer0;
      timer0=s1timer0;
      settimer2=s1settimer2;
      timer2=s1timer2;
      getsample=getsample1;
    }
    else {
      soundoff=s2soundoff;
      setspkrt2=s2setspkrt2;
      settimer0=s2settimer0;
      timer0=s2timer0;
      settimer2=s2settimer2;
      timer2=s2timer2;
      getsample=getsample2;
    }
    soundinitglob(sound_port,sound_irq,sound_dma,sound_length,sound_rate);
  }
#ifdef _WINDOWS
  dx_sound_volume=(int)GetINIInt(INI_SOUND_SETTINGS,"SoundVolume",100,ININAME);
  set_sound_volume(dx_sound_volume);
#else
  dx_sound_volume=(int)GetINIInt(INI_SOUND_SETTINGS,"SoundVolume",0,ININAME);
#endif
#ifndef DIRECTX
  g_bWindowed=TRUE;
#else
  g_bWindowed=!GetINIBool(INI_GRAPHICS_SETTINGS,"FullScreen",FALSE,ININAME);
#endif
  use_640x480_fullscreen=GetINIBool(INI_GRAPHICS_SETTINGS,"640x480",FALSE,
                                    ININAME);
#ifdef DIRECTX
  if (!g_bWindowed)
    ChangeCoopLevel();
#endif
  use_async_screen_updates=GetINIBool(INI_GRAPHICS_SETTINGS,"Async",TRUE,
                                      ININAME);
  synchvid=GetINIBool(INI_GRAPHICS_SETTINGS,"Synch",FALSE,ININAME);
  cgaflag=GetINIBool(INI_GRAPHICS_SETTINGS,"CGA",FALSE,ININAME);
  biosflag=GetINIBool(INI_GRAPHICS_SETTINGS,"BIOSPalette",FALSE,ININAME);
  if (cgaflag || biosflag) {
    ginit=cgainit;
    gpal=cgapal;
    ginten=cgainten;
    gclear=cgaclear;
    ggetpix=cgagetpix;
    gputi=cgaputi;
    ggeti=cgageti;
    gputim=cgaputim;
    gwrite=cgawrite;
    gtitle=cgatitle;
    ginit();
    gpal(0);
  }
  unlimlives=GetINIBool(INI_GAME_SETTINGS,"UnlimitedLives",FALSE,ININAME);
  startlev=(int)GetINIInt(INI_GAME_SETTINGS,"StartLevel",1,ININAME);
#ifdef _WINDOWS
  GetINIString(INI_GAME_SETTINGS,"LevelFile","",levfname,FILENAME_BUFFER_SIZE,ININAME);
  levfflag=(levfname[0]!='\0'); /* TRUE; */
#endif
}
#endif

void redefkeyb(bool allf)
{
  int i,j,k,l,z,y=0;
  bool f;
#ifndef DIGGER_ELKS
  char kbuf[80],vbuf[80];
#endif
  if (diggers==2) {
    outtext("PLAYER 1:",0,y,3);
    y+=12;
  }

  outtext("PRESS NEW KEY FOR",0,y,3);
  y+=12;

/* Step one: redefine keys that are always redefined. */

  for (i=0;i<5;i++) {
    outtext(keynames[i],0,y,2); /* Red first */
    findkey(i);
    outtext(keynames[i],0,y,1); /* Green once got */
    y+=12;
    for (j=0;j<i;j++) { /* Note: only check keys just pressed (I hate it when
                           this is done wrong, and it often is.) */
      if (keycodes[i][0]==keycodes[j][0] && keycodes[i][0]!=0) {
        i--;
        y-=12;
        break;
      }
      for (k=2;k<5;k++)
        for (l=2;l<5;l++)
          if (keycodes[i][k]==keycodes[j][l] && keycodes[i][k]!=-2) {
            j=i;
            k=5;
            i--;
            y-=12;
            break; /* Try again if this key already used */
          }
    }
  }

  if (diggers==2) {
    outtext("PLAYER 2:",0,y,3);
    y+=12;
    for (i=5;i<10;i++) {
      outtext(keynames[i],0,y,2); /* Red first */
      findkey(i);
      outtext(keynames[i],0,y,1); /* Green once got */
      y+=12;
      for (j=0;j<i;j++) { /* Note: only check keys just pressed (I hate it when
                             this is done wrong, and it often is.) */
        if (keycodes[i][0]==keycodes[j][0] && keycodes[i][0]!=0) {
          i--;
          y-=12;
          break;
        }
        for (k=2;k<5;k++)
          for (l=2;l<5;l++)
            if (keycodes[i][k]==keycodes[j][l] && keycodes[i][k]!=-2) {
              j=i;
              k=5;
              i--;
              y-=12;
              break; /* Try again if this key already used */
            }
      }
    }
  }

/* Step two: redefine other keys which step one has caused to conflict */

  z=0;
  y-=12;
  for (i=10;i<17;i++) {
    f=FALSE;
    for (j=0;j<10;j++)
      for (k=0;k<5;k++)
        for (l=2;l<5;l++)
          if (keycodes[i][k]==keycodes[j][l] && keycodes[i][k]!=-2)
            f=TRUE;
    for (j=10;j<i;j++)
      for (k=0;k<5;k++)
        for (l=0;l<5;l++)
          if (keycodes[i][k]==keycodes[j][l] && keycodes[i][k]!=-2)
            f=TRUE;
    if (f || (allf && i!=z)) {
      if (i!=z)
        y+=12;
      outtext(keynames[i],0,y,2); /* Red first */
      findkey(i);
      outtext(keynames[i],0,y,1); /* Green once got */
      z=i;
      i--;
    }
  }

/* Step three: save the INI file.  ELKS has no persistent INI backend, so
 * keep any redefined keys for this process only and avoid linking the no-op
 * WriteINIString compatibility path.
 */
#ifndef DIGGER_ELKS
  for (i=0;i<17;i++)
    if (krdf[i]) {
      sprintf(kbuf,"%s%c",keynames[i],(i>=5 && i<10) ? '2' : 0);
      sprintf(vbuf,"%i/%i/%i/%i/%i",keycodes[i][0],keycodes[i][1],
              keycodes[i][2],keycodes[i][3],keycodes[i][4]);
      WriteINIString(INI_KEY_SETTINGS,kbuf,vbuf,ININAME);
    }
#endif
}
