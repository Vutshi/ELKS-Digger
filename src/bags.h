/* Digger Remastered
   Copyright (c) Andrew Jenner 1998-2004 */

void dobags(void);
Sint4 getnmovingbags(void);
void cleanupbags(void);
void initbags(void);
void drawbags(void);
bool pushbags(Sint4 dir,Sint3 *clfirst,Sint3 *clcoll);
bool pushudbags(Sint3 *clfirst,Sint3 *clcoll);
Sint4 bagy(Sint4 bag);
Sint4 getbagdir(Sint4 bag);
void removebags(Sint3 *clfirst,Sint3 *clcoll);
bool bagexist(int bag);

