/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include <stdio.h>
#include <stdlib.h>

/* We really do have 'long long' on the Mac, but NSPR doesn't yet know this.
   Need to define this before including NSPR to get the right defn of PRInt64. */
#if 0
#ifdef XP_MAC
#define HAVE_LONG_LONG
#endif
#endif

#include "prtime.h"
#include "prenv.h"
#include "prlong.h"

char * timebombVar = "NSM_TIMEBOMB";
#define SSM_SEC_PER_24HRS (60 * 60 * 24)

int main()
{ 
 FILE * headerFile, * timeFile;
 int daysToLive;
 char * tmp = PR_GetEnv(timebombVar);
 PRTime timeNow, lifeTime, expire, tmpVal, days, tmpMilli;
 PRInt32 timeHi, timeLow;

 daysToLive = atoi(tmp);
 timeNow = PR_Now();

 LL_I2L(tmpVal, SSM_SEC_PER_24HRS);
 LL_I2L(days, daysToLive);
 LL_MUL(tmpMilli, days, tmpVal);
 LL_MUL(lifeTime, tmpMilli, PR_USEC_PER_SEC);

 LL_ADD(expire, timeNow, lifeTime);
 LL_SHR(tmpVal, expire, 32);
 LL_L2UI(timeHi, tmpVal);
 LL_L2UI(timeLow, expire);


 timeFile = fopen("timestamp.h", "w");
 if (!timeFile) {
   printf("Can't create timestamp.h.\n");
   goto loser;
 }

 fprintf(timeFile, "/*\n * Created automatically, do not edit!\n */\n\n");
 fprintf(timeFile, "/* This build of Cartman will expire at this time. */ \n");
 fprintf(timeFile, "static PRTime expirationTime =  LL_INIT(0x%lx, 0x%lx);\n", timeHi, 
		   timeLow);
 
 fclose(timeFile);
 
 headerFile = fopen("timebomb.h", "w");
 if (!headerFile) {
   printf("Can't open timebomb.h for writing!\n");
   goto loser;
 }

 fprintf(headerFile,
         "/* This file is generated automatically by createBomb.c, do not edit! */\n\n\n");
 
 /*
  * Function declarations that are used for the timebomb.
  * Definitions are in timebomb.c, included in frontend.c.
  */
 fprintf(headerFile,
         "\n\n/* Functions used in Cartman for the timebomb. */\n");
 fprintf(headerFile,
         "/*\n * Set SSMTimeBombExpired to PR_TRUE if Cartman has expired. \n */\n");
 fprintf(headerFile, "void SSM_CheckTimeBomb();\n\n");
 fprintf(headerFile,
	 "/*\n * Run this function from frontend thread of control connection\n * if Cartman has expired. \n */\n");
 fprintf(headerFile, "void SSM_CartmanHasExpired(SSMControlConnection * control);\n\n");
 
 fclose(headerFile);

loser:
 return 0;
}
