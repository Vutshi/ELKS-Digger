/* Digger Remastered
   Copyright (c) Andrew Jenner 1998-2004 */

#ifdef DIGGER_RECORD_STUB

#define playing   ((bool)FALSE)
#define savedrf   ((bool)FALSE)
#define gotname   ((bool)FALSE)
#define gotgame   ((bool)FALSE)
#define drfvalid  ((bool)TRUE)
#define kludge    ((bool)FALSE)

#define openplay(name)          ((void)(name))
#define recstart()              ((void)0)
#define recname(name)           ((void)(name))
#define playgetdir(dir,fire)    ((void)0)
#define recinit()               ((void)0)
#define recputrand(randv)       ((void)(randv))
#define playgetrand()           ((Uint5)0)
#define recputinit(init)        ((void)(init))
#define recputeol()             ((void)0)
#define recputeog()             ((void)0)
#define playskipeol()           ((void)0)
#define recputdir(dir,fire)     ((void)0)
#define recsavedrf()            ((void)0)

#else

void openplay(char *name);
void recstart(void);
void recname(char *name);
void playgetdir(Sint4 *dir,bool *fire);
void recinit(void);
void recputrand(Uint5 randv);
Uint5 playgetrand(void);
void recputinit(char *init);
void recputeol(void);
void recputeog(void);
void playskipeol(void);
void recputdir(Sint4 dir,bool fire);
void recsavedrf(void);

extern bool playing,savedrf,gotname,gotgame,drfvalid,kludge;

#endif
