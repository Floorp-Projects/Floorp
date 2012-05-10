/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * file: timemac.c
 * description: test time and date routines on the Mac
 */
#include <stdio.h>
#include "prinit.h"
#include "prtime.h"


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
