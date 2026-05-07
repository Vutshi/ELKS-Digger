/* Digger Remastered
   Copyright (c) Andrew Jenner 1998-2004 */

void initsound(void);
void soundstop(void);
void music(Sint4 tune);
void musicoff(void);
void soundlevdone(void);
void sound1up(void);
void soundpause(void);
void soundpauseoff(void);
void setsoundt2(void);
void sett2val(Sint4 t2v);
void startint8(void);
void stopint8(void);
void soundbonus(void);
void soundbonusoff(void);
void soundfire(int n);
void soundexplode(int n);
void soundfireoff(int n);
void soundem(void);
void soundemerald(int emn);
void soundeatm(void);
void soundddie(void);
void soundwobble(void);
void soundwobbleoff(void);
void soundfall(void);
void soundfalloff(void);
void soundbreak(void);
void soundgold(void);

void soundint(void);

/*
void soundoff(void);
void timer2(Uint4 t2v);
*/

extern bool soundflag,musicflag;
extern Sint4 volume,timerrate;
extern Uint4 timercount;

#if defined(DIGGER_ELKS) && defined(DIGGER_ELKS_DOS_SOUND)
/* Full ELKS DOS-sound build uses the original sound state machine, but its
 * backend is fixed: direct cached PC-speaker divisors.  Make the backend calls
 * compile-time direct instead of carrying the original DOS sampled-device
 * function-pointer table.
 */
void elks_sound_setup(void);
void elks_sound_kill(void);
void elks_sound_off(void);
void elks_sound_timer2(Uint4 t2v);
#define setupsound()       elks_sound_setup()
#define killsound()        elks_sound_kill()
#define fillbuffer()       ((void)0)
#define initint8()         ((void)0)
#define restoreint8()      ((void)0)
#define soundoff()         elks_sound_off()
#define setspkrt2()        ((void)0)
#define settimer0(t0v)     ((void)(t0v))
#define timer0(t0v)        ((void)(t0v))
#define settimer2(t2v)     elks_sound_timer2((Uint4)(t2v))
#define timer2(t2v)        elks_sound_timer2((Uint4)(t2v))
#define soundkillglob()    elks_sound_kill()
#else
extern void (*setupsound)(void);
extern void (*killsound)(void);
extern void (*fillbuffer)(void);
extern void (*initint8)(void);
extern void (*restoreint8)(void);
extern void (*soundoff)(void);
extern void (*setspkrt2)(void);
extern void (*settimer0)(Uint4 t0v);
extern void (*timer0)(Uint4 t0v);
extern void (*settimer2)(Uint4 t2v);
extern void (*timer2)(Uint4 t2v);
extern void (*soundkillglob)(void);
#endif

