/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
