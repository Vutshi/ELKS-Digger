/* Digger Remastered
   Copyright (c) Andrew Jenner 1998-2004 */

void domonsters(void);
void incmont(Sint4 n);
void erasemonsters(void);
void initmonsters(void);
Sint4 monleft(void);
void killmon(Sint4 mon);
Sint4 killmonsters(Sint3 *clfirst,Sint3 *clcoll);
void checkmonscared(Sint4 h);
void squashmonsters(Sint4 bag,Sint3 *clfirst,Sint3 *clcoll);
void mongold(void);

#ifndef DIGGER_ELKS
Sint4 getfield(Sint4 x,Sint4 y);
#endif
