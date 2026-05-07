/* Digger Remastered
   Copyright (c) Andrew Jenner 1998-2004 */

void dobags(void);
Sint4 getnmovingbags(void);
void cleanupbags(void);
void initbags(void);
void drawbags(void);
bool pushbags(Sint4 dir,Sint4 *clfirst,Sint4 *clcoll);
bool pushudbags(Sint4 *clfirst,Sint4 *clcoll);
Sint4 bagy(Sint4 bag);
Sint4 getbagdir(Sint4 bag);
void removebags(Sint4 *clfirst,Sint4 *clcoll);
bool bagexist(int bag);
