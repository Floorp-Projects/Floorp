/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * file: timemac.c
 * description: test time and date routines on the Mac
 */
#include <stdio.h>
#include "prinit.h"
#include "prtime.h"

#ifdef XP_MAC
#include "prlog.h"
#define printf PR_LogPrint
extern void SetupMacPrintfLog(char *logFile);
#endif


static char *dayOfWeek[] =
	{ "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "???" };
static char *month[] =
	{ "Jan", "Feb", "Mar", "Apr", "May", "Jun",
	  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", "???" };

static void printExplodedTime(const PRExplodedTime *et) {
    PRInt32 totalOffset;
    PRInt32 hourOffset, minOffset;
    const char *sign;

    /* Print day of the week, month, day, hour, minute, and second */
    printf( "%s %s %ld %02ld:%02ld:%02ld ",
	    dayOfWeek[et->tm_wday], month[et->tm_month], et->tm_mday,
	    et->tm_hour, et->tm_min, et->tm_sec);

    /* Print time zone */
    totalOffset = et->tm_params.tp_gmt_offset + et->tm_params.tp_dst_offset;
    if (totalOffset == 0) {
	printf("UTC ");
    } else {
        sign = "";
        if (totalOffset < 0) {
	    totalOffset = -totalOffset;
	    sign = "-";
        }
        hourOffset = totalOffset / 3600;
        minOffset = (totalOffset % 3600) / 60;
        printf("%s%02ld%02ld ", sign, hourOffset, minOffset);
    }

    /* Print year */
    printf("%d", et->tm_year);
}

int main(int argc, char** argv)
{
    PR_STDIO_INIT();
    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);

 
#ifdef XP_MAC
	SetupMacPrintfLog("timemac.log");
#endif

   /*
     *************************************************************
     **
     **  Testing PR_Now(), PR_ExplodeTime, and PR_ImplodeTime
     **  on the current time
     **
     *************************************************************
     */

    {
	PRTime t1, t2;
	PRExplodedTime et;

	printf("*********************************************\n");
	printf("**                                         **\n");
        printf("** Testing PR_Now(), PR_ExplodeTime, and   **\n");
	printf("** PR_ImplodeTime on the current time      **\n");
	printf("**                                         **\n");
	printf("*********************************************\n\n");
        t1 = PR_Now();

        /* First try converting to UTC */

        PR_ExplodeTime(t1, PR_GMTParameters, &et);
        if (et.tm_params.tp_gmt_offset || et.tm_params.tp_dst_offset) {
	    printf("ERROR: UTC has nonzero gmt or dst offset.\n");
	    return 1;
        }
        printf("Current UTC is ");
	printExplodedTime(&et);
	printf("\n");

        t2 = PR_ImplodeTime(&et);
        if (LL_NE(t1, t2)) {
	    printf("ERROR: Explode and implode are NOT inverse.\n");
	    return 1;
        }

        /* Next, try converting to local (US Pacific) time */

        PR_ExplodeTime(t1, PR_LocalTimeParameters, &et);
        printf("Current local time is ");
	printExplodedTime(&et);
	printf("\n");
	printf("GMT offset is %ld, DST offset is %ld\n",
		et.tm_params.tp_gmt_offset, et.tm_params.tp_dst_offset);
        t2 = PR_ImplodeTime(&et);
        if (LL_NE(t1, t2)) {
	    printf("ERROR: Explode and implode are NOT inverse.\n");
	    return 1;
	}
    }

    printf("Please examine the results\n");
    return 0;
}
