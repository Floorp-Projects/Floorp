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
 * file: timetest.c
 * description: test time and date routines
 */
/***********************************************************************
** Includes
***********************************************************************/
/* Used to get the command line option */
#include "plgetopt.h"

#include "prinit.h"
#include "prtime.h"
#include "prprf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef XP_MAC
#include "prlog.h"
#include "macstdlibextras.h"
extern void SetupMacPrintfLog(char *logFile);
#endif

int failed_already=0;
PRBool debug_mode = PR_FALSE;

static char *dayOfWeek[] =
	{ "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "???" };
static char *month[] =
	{ "Jan", "Feb", "Mar", "Apr", "May", "Jun",
	  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", "???" };

static void PrintExplodedTime(const PRExplodedTime *et) {
    PRInt32 totalOffset;
    PRInt32 hourOffset, minOffset;
    const char *sign;

    /* Print day of the week, month, day, hour, minute, and second */
    if (debug_mode) printf("%s %s %ld %02ld:%02ld:%02ld ",
	    dayOfWeek[et->tm_wday], month[et->tm_month], et->tm_mday,
	    et->tm_hour, et->tm_min, et->tm_sec);

    /* Print time zone */
    totalOffset = et->tm_params.tp_gmt_offset + et->tm_params.tp_dst_offset;
    if (totalOffset == 0) {
	if (debug_mode) printf("UTC ");
    } else {
        sign = "+";
        if (totalOffset < 0) {
	    totalOffset = -totalOffset;
	    sign = "-";
        }
        hourOffset = totalOffset / 3600;
        minOffset = (totalOffset % 3600) / 60;
        if (debug_mode) 
            printf("%s%02ld%02ld ", sign, hourOffset, minOffset);
    }

    /* Print year */
    if (debug_mode) printf("%hd", et->tm_year);
}

static int ExplodedTimeIsEqual(const PRExplodedTime *et1,
	const PRExplodedTime *et2)
{
    if (et1->tm_usec == et2->tm_usec &&
	    et1->tm_sec == et2->tm_sec &&
	    et1->tm_min == et2->tm_min &&
	    et1->tm_hour == et2->tm_hour &&
	    et1->tm_mday == et2->tm_mday &&
	    et1->tm_month == et2->tm_month &&
	    et1->tm_year == et2->tm_year &&
	    et1->tm_wday == et2->tm_wday &&
	    et1->tm_yday == et2->tm_yday &&
	    et1->tm_params.tp_gmt_offset == et2->tm_params.tp_gmt_offset &&
	    et1->tm_params.tp_dst_offset == et2->tm_params.tp_dst_offset) {
        return 1;
    } else {
	return 0;
    }
}

static void
testParseTimeString(PRTime t)
{
    PRExplodedTime et;
    PRTime t2;
    char timeString[128];
    char buf[128];
    PRInt32 totalOffset;
    PRInt32 hourOffset, minOffset;
    const char *sign;
    PRInt64 usec_per_sec;

    /* Truncate the microsecond part of PRTime */
    LL_I2L(usec_per_sec, PR_USEC_PER_SEC);
    LL_DIV(t, t, usec_per_sec);
    LL_MUL(t, t, usec_per_sec);

    PR_ExplodeTime(t, PR_LocalTimeParameters, &et);

    /* Print day of the week, month, day, hour, minute, and second */
    PR_snprintf(timeString, 128, "%s %s %ld %02ld:%02ld:%02ld ",
	    dayOfWeek[et.tm_wday], month[et.tm_month], et.tm_mday,
	    et.tm_hour, et.tm_min, et.tm_sec);
    /* Print time zone */
    totalOffset = et.tm_params.tp_gmt_offset + et.tm_params.tp_dst_offset;
    if (totalOffset == 0) {
	strcat(timeString, "GMT ");  /* I wanted to use "UTC" here, but
                                      * PR_ParseTimeString doesn't 
                                      * understand "UTC".  */
    } else {
        sign = "+";
        if (totalOffset < 0) {
	    totalOffset = -totalOffset;
	    sign = "-";
        }
        hourOffset = totalOffset / 3600;
        minOffset = (totalOffset % 3600) / 60;
        PR_snprintf(buf, 128, "%s%02ld%02ld ", sign, hourOffset, minOffset);
	strcat(timeString, buf);
    }
    /* Print year */
    PR_snprintf(buf, 128, "%hd", et.tm_year);
    strcat(timeString, buf);

    if (PR_ParseTimeString(timeString, PR_FALSE, &t2) == PR_FAILURE) {
	fprintf(stderr, "PR_ParseTimeString() failed\n");
	exit(1);
    }
    if (LL_NE(t, t2)) {
	fprintf(stderr, "PR_ParseTimeString() incorrect\n");
	PR_snprintf(buf, 128, "t is %lld, t2 is %lld, time string is %s\n",
                t, t2, timeString);
	fprintf(stderr, "%s\n", buf);
	exit(1);
    }
}

int main(int argc, char** argv)
{
	/* The command line argument: -d is used to determine if the test is being run
	in debug mode. The regress tool requires only one line output:PASS or FAIL.
	All of the printfs associated with this test has been handled with a if (debug_mode)
	test.
	Usage: test_name -d
	*/
	PLOptStatus os;
	PLOptState *opt;
    
    PR_STDIO_INIT();
	opt = PL_CreateOptState(argc, argv, "d");
	while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
		if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'd':  /* debug mode */
			debug_mode = PR_TRUE;
            break;
         default:
            break;
        }
    }
	PL_DestroyOptState(opt);

 /* main test */
	
    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);

#ifdef XP_MAC
	/* Set up the console */
	InitializeSIOUX(true);
	debug_mode = PR_TRUE;
#endif
    /* Testing zero PRTime (the epoch) */
    {
	PRTime t;
	PRExplodedTime et;

	LL_I2L(t, 0);
	if (debug_mode) printf("The NSPR epoch is:\n");
        PR_ExplodeTime(t, PR_LocalTimeParameters, &et);
	PrintExplodedTime(&et);
	if (debug_mode) printf("\n");
	PR_ExplodeTime(t, PR_GMTParameters, &et);
	PrintExplodedTime(&et);
	if (debug_mode) printf("\n\n");
	testParseTimeString(t);
    }

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

	if (debug_mode) {
	printf("*********************************************\n");
	printf("**                                         **\n");
    printf("** Testing PR_Now(), PR_ExplodeTime, and   **\n");
	printf("** PR_ImplodeTime on the current time      **\n");
	printf("**                                         **\n");
	printf("*********************************************\n\n");
	}
	t1 = PR_Now();

        /* First try converting to UTC */

        PR_ExplodeTime(t1, PR_GMTParameters, &et);
        if (et.tm_params.tp_gmt_offset || et.tm_params.tp_dst_offset) {
	    if (debug_mode) printf("ERROR: UTC has nonzero gmt or dst offset.\n");
		else failed_already=1;
	    return 1;
        }
        if (debug_mode) printf("Current UTC is ");
	PrintExplodedTime(&et);
	if (debug_mode) printf("\n");

        t2 = PR_ImplodeTime(&et);
        if (LL_NE(t1, t2)) {
	    if (debug_mode) printf("ERROR: Explode and implode are NOT inverse.\n");
		else printf("FAIL\n");
	    return 1;
        }

        /* Next, try converting to local (US Pacific) time */

        PR_ExplodeTime(t1, PR_LocalTimeParameters, &et);
        if (debug_mode) printf("Current local time is ");
	PrintExplodedTime(&et);
	if (debug_mode) printf("\n");
	if (debug_mode) printf("GMT offset is %ld, DST offset is %ld\n",
		et.tm_params.tp_gmt_offset, et.tm_params.tp_dst_offset);
        t2 = PR_ImplodeTime(&et);
        if (LL_NE(t1, t2)) {
	    if (debug_mode) printf("ERROR: Explode and implode are NOT inverse.\n");
	    return 1;
	}

	if (debug_mode) printf("Please examine the results\n");
	testParseTimeString(t1);
    }


    /*
     *******************************************
     **
     ** Testing PR_NormalizeTime()
     **
     *******************************************
     */

    /* July 4, 2001 is Wednesday */
    {
	PRExplodedTime et;

	if (debug_mode)  {
	printf("\n");
	printf("**********************************\n");
	printf("**                              **\n");
	printf("** Testing PR_NormalizeTime()   **\n");
	printf("**                              **\n");
	printf("**********************************\n\n");
	}
        et.tm_year    = 2001;
        et.tm_month   = 7 - 1;
        et.tm_mday    = 4;
        et.tm_hour    = 0;
        et.tm_min     = 0;
        et.tm_sec     = 0;
	et.tm_usec    = 0;
        et.tm_params  = PR_GMTParameters(&et);

	PR_NormalizeTime(&et, PR_GMTParameters);

	if (debug_mode) printf("July 4, 2001 is %s.\n", dayOfWeek[et.tm_wday]);
	if (et.tm_wday == 3) {
	    if (debug_mode) printf("PASS\n");
        } else {
            if (debug_mode) printf("ERROR: It should be Wednesday\n");
			else failed_already=1;
	    return 1;
	}
	testParseTimeString(PR_ImplodeTime(&et));

        /* June 12, 1997 23:00 PST == June 13, 1997 00:00 PDT */
        et.tm_year    = 1997;
        et.tm_month   = 6 - 1;
        et.tm_mday    = 12;
        et.tm_hour    = 23;
        et.tm_min     = 0;
        et.tm_sec     = 0;
	et.tm_usec    = 0;
        et.tm_params.tp_gmt_offset = -8 * 3600;
	et.tm_params.tp_dst_offset = 0;

	PR_NormalizeTime(&et, PR_USPacificTimeParameters);

	if (debug_mode) {
	    printf("Thu Jun 12, 1997 23:00:00 PST is ");
	}
	PrintExplodedTime(&et);
	if (debug_mode) printf(".\n");
	if (et.tm_wday == 5) {
	    if (debug_mode) printf("PASS\n");
        } else {
            if (debug_mode) printf("ERROR: It should be Friday\n");
			else failed_already=1;
	    return 1;
	}
	testParseTimeString(PR_ImplodeTime(&et));

        /* Feb 14, 1997 00:00:00 PDT == Feb 13, 1997 23:00:00 PST */
        et.tm_year    = 1997;
        et.tm_month   = 2 - 1;
        et.tm_mday    = 14;
        et.tm_hour    = 0;
        et.tm_min     = 0;
        et.tm_sec     = 0;
	et.tm_usec    = 0;
        et.tm_params.tp_gmt_offset = -8 * 3600;
	et.tm_params.tp_dst_offset = 3600;

	PR_NormalizeTime(&et, PR_USPacificTimeParameters);

	if (debug_mode) {
	    printf("Fri Feb 14, 1997 00:00:00 PDT is ");
	}
	PrintExplodedTime(&et);
	if (debug_mode) printf(".\n");
	if (et.tm_wday == 4) {
	    if (debug_mode) printf("PASS\n");
        } else {
            if (debug_mode) printf("ERROR: It should be Thursday\n");
			else failed_already=1;
	    return 1;
	}
	testParseTimeString(PR_ImplodeTime(&et));

        /* What time is Nov. 7, 1996, 18:29:23 PDT? */
        et.tm_year    = 1996;
        et.tm_month   = 11 - 1;
        et.tm_mday    = 7;
        et.tm_hour    = 18;
        et.tm_min     = 29;
        et.tm_sec     = 23;
	et.tm_usec    = 0;
        et.tm_params.tp_gmt_offset = -8 * 3600;  /* PDT */
	et.tm_params.tp_dst_offset = 3600; 

	PR_NormalizeTime(&et, PR_LocalTimeParameters);
        if (debug_mode) printf("Nov 7 18:29:23 PDT 1996 is ");
	PrintExplodedTime(&et);
	if (debug_mode) printf(".\n");
	testParseTimeString(PR_ImplodeTime(&et));

        /* What time is Oct. 7, 1995, 18:29:23 PST? */
        et.tm_year    = 1995;
        et.tm_month   = 10 - 1;
        et.tm_mday    = 7;
        et.tm_hour    = 18;
        et.tm_min     = 29;
        et.tm_sec     = 23;
        et.tm_params.tp_gmt_offset = -8 * 3600;  /* PST */
	et.tm_params.tp_dst_offset = 0;

	PR_NormalizeTime(&et, PR_LocalTimeParameters);
        if (debug_mode) printf("Oct 7 18:29:23 PST 1995 is ");
	PrintExplodedTime(&et);
	if (debug_mode) printf(".\n");
	testParseTimeString(PR_ImplodeTime(&et));

	if (debug_mode) printf("Please examine the results\n");
    }

    /*
     **************************************************************
     **
     ** Testing range of years
     **
     **************************************************************
     */

    {
	PRExplodedTime et1, et2;
    PRTime  ttt;
	PRTime secs;

	if (debug_mode) {
	printf("\n");
	printf("***************************************\n");
	printf("**                                   **\n");
	printf("**  Testing range of years           **\n");
	printf("**                                   **\n");
	printf("***************************************\n\n");
	}
	/* April 4, 1917 GMT */
	et1.tm_usec = 0;
	et1.tm_sec = 0;
	et1.tm_min = 0;
	et1.tm_hour = 0;
	et1.tm_mday = 4;
	et1.tm_month = 4 - 1;
	et1.tm_year = 1917;
	et1.tm_params = PR_GMTParameters(&et1);
	PR_NormalizeTime(&et1, PR_LocalTimeParameters);
	secs = PR_ImplodeTime(&et1);
	if (LL_GE_ZERO(secs)) {
	    if (debug_mode)
		printf("ERROR: April 4, 1917 GMT returns a nonnegative second count\n");
		failed_already = 1;
	    return 1;
        }
	PR_ExplodeTime(secs, PR_LocalTimeParameters, &et2);
	if (!ExplodedTimeIsEqual(&et1, &et2)) {
		if (debug_mode)
		printf("ERROR: PR_ImplodeTime and PR_ExplodeTime are not inverse for April 4, 1917 GMT\n");
		failed_already=1;
	    return 1;
        }
    ttt = PR_ImplodeTime(&et1);
	testParseTimeString( ttt );

	if (debug_mode) printf("Test passed for April 4, 1917\n");

	/* July 4, 2050 */
	et1.tm_usec = 0;
	et1.tm_sec = 0;
	et1.tm_min = 0;
	et1.tm_hour = 0;
	et1.tm_mday = 4;
	et1.tm_month = 7 - 1;
	et1.tm_year = 2050;
	et1.tm_params = PR_GMTParameters(&et1);
	PR_NormalizeTime(&et1, PR_LocalTimeParameters);
	secs = PR_ImplodeTime(&et1);
	if (!LL_GE_ZERO(secs)) {
	    if (debug_mode)
			printf("ERROR: July 4, 2050 GMT returns a negative second count\n");
		failed_already = 1;
	    return 1;
        }
	PR_ExplodeTime(secs, PR_LocalTimeParameters, &et2);
	if (!ExplodedTimeIsEqual(&et1, &et2)) {
	    if (debug_mode)
		printf("ERROR: PR_ImplodeTime and PR_ExplodeTime are not inverse for July 4, 2050 GMT\n");
		failed_already=1;
	    return 1;
        }
	testParseTimeString(PR_ImplodeTime(&et1));

	if (debug_mode) printf("Test passed for July 4, 2050\n");

    }

    /*
     **************************************************************
     **
     **  Stress test
     *
     **      Go through four years, starting from
     **      00:00:00 PST Jan. 1, 1993, incrementing
     **      every 10 minutes.
     **
     **************************************************************
     */

    {
	PRExplodedTime et, et1, et2;
	PRInt64 usecPer10Min;
	int day, hour, min;
	PRTime usecs;
	int dstInEffect = 0;

	if (debug_mode) {
	printf("\n");
	printf("*******************************************************\n");
	printf("**                                                   **\n");
	printf("**                 Stress test                       **\n");
	printf("**  Starting from midnight Jan. 1, 1993 PST,         **\n");
	printf("**  going through four years in 10-minute increment  **\n");
	printf("**                                                   **\n");
	printf("*******************************************************\n\n");
	}
	LL_I2L(usecPer10Min, 600000000L);

	/* 00:00:00 PST Jan. 1, 1993 */
	et.tm_usec = 0;
	et.tm_sec = 0;
	et.tm_min = 0;
	et.tm_hour = 0;
	et.tm_mday = 1;
	et.tm_month = 0;
	et.tm_year = 1993;
	et.tm_params.tp_gmt_offset = -8 * 3600;
	et.tm_params.tp_dst_offset = 0;
	usecs = PR_ImplodeTime(&et);

        for (day = 0; day < 4 * 365 + 1; day++) {
	    for (hour = 0; hour < 24; hour++) {
		for (min = 0; min < 60; min += 10) {
	            LL_ADD(usecs, usecs, usecPer10Min);
		    PR_ExplodeTime(usecs, PR_USPacificTimeParameters, &et1);

		    et2 = et;
		    et2.tm_usec += 600000000L;
		    PR_NormalizeTime(&et2, PR_USPacificTimeParameters);

		    if (!ExplodedTimeIsEqual(&et1, &et2)) {
		        if (debug_mode) printf("ERROR: componentwise comparison failed\n");
			PrintExplodedTime(&et1);
			if (debug_mode) printf("\n");
			PrintExplodedTime(&et2);
			if (debug_mode) printf("\n");
			failed_already=1;
		        return 1;
		    }

		    if (LL_NE(usecs, PR_ImplodeTime(&et1))) { 
                        if (debug_mode)
					printf("ERROR: PR_ExplodeTime and PR_ImplodeTime are not inverse\n");
			PrintExplodedTime(&et1);
			if (debug_mode) printf("\n");
			failed_already=1;
		        return 1;
		    }
		    testParseTimeString(usecs);

		    if (!dstInEffect && et1.tm_params.tp_dst_offset) {
		        dstInEffect = 1;
		        if (debug_mode) printf("DST changeover from ");
			PrintExplodedTime(&et);
			if (debug_mode) printf(" to ");
			PrintExplodedTime(&et1);
			if (debug_mode) printf(".\n");
                    } else if (dstInEffect && !et1.tm_params.tp_dst_offset) {
		        dstInEffect = 0;
			if (debug_mode) printf("DST changeover from ");
			PrintExplodedTime(&et);
			if (debug_mode) printf(" to ");
			PrintExplodedTime(&et1);
			if (debug_mode) printf(".\n");
                    }

		    et = et1;
		}
	    }
        }
	if (debug_mode) printf("Test passed\n");
    }


    /* Same stress test, but with PR_LocalTimeParameters */

    {
	PRExplodedTime et, et1, et2;
	PRInt64 usecPer10Min;
	int day, hour, min;
	PRTime usecs;
	int dstInEffect = 0;

	if (debug_mode) {
	printf("\n");
	printf("*******************************************************\n");
	printf("**                                                   **\n");
	printf("**                 Stress test                       **\n");
	printf("**  Starting from midnight Jan. 1, 1993 PST,         **\n");
	printf("**  going through four years in 10-minute increment  **\n");
	printf("**                                                   **\n");
	printf("*******************************************************\n\n");
	}
	
	LL_I2L(usecPer10Min, 600000000L);

	/* 00:00:00 PST Jan. 1, 1993 */
	et.tm_usec = 0;
	et.tm_sec = 0;
	et.tm_min = 0;
	et.tm_hour = 0;
	et.tm_mday = 1;
	et.tm_month = 0;
	et.tm_year = 1993;
	et.tm_params.tp_gmt_offset = -8 * 3600;
	et.tm_params.tp_dst_offset = 0;
	usecs = PR_ImplodeTime(&et);

        for (day = 0; day < 4 * 365 + 1; day++) {
	    for (hour = 0; hour < 24; hour++) {
		for (min = 0; min < 60; min += 10) {
	            LL_ADD(usecs, usecs, usecPer10Min);
		    PR_ExplodeTime(usecs, PR_LocalTimeParameters, &et1);

		    et2 = et;
		    et2.tm_usec += 600000000L;
		    PR_NormalizeTime(&et2, PR_LocalTimeParameters);

		    if (!ExplodedTimeIsEqual(&et1, &et2)) {
		        if (debug_mode) printf("ERROR: componentwise comparison failed\n");
			PrintExplodedTime(&et1);
			if (debug_mode) printf("\n");
			PrintExplodedTime(&et2);
			if (debug_mode) printf("\n");
		        return 1;
		    }

		    if (LL_NE(usecs, PR_ImplodeTime(&et1))) {
                        printf("ERROR: PR_ExplodeTime and PR_ImplodeTime are not inverse\n");
			PrintExplodedTime(&et1);
			if (debug_mode) printf("\n");
			failed_already=1;
		        return 1;
		    }
		    testParseTimeString(usecs);

		    if (!dstInEffect && et1.tm_params.tp_dst_offset) {
		        dstInEffect = 1;
		        if (debug_mode) printf("DST changeover from ");
			PrintExplodedTime(&et);
			if (debug_mode) printf(" to ");
			PrintExplodedTime(&et1);
			if (debug_mode) printf(".\n");
                    } else if (dstInEffect && !et1.tm_params.tp_dst_offset) {
		        dstInEffect = 0;
			if (debug_mode) printf("DST changeover from ");
			PrintExplodedTime(&et);
			if (debug_mode) printf(" to ");
			PrintExplodedTime(&et1);
			if (debug_mode) printf(".\n");
                    }

		    et = et1;
		}
	    }
        }
	if (debug_mode) printf("Test passed\n");
    }

    /* Same stress test, but with PR_LocalTimeParameters and going backward */

    {
	PRExplodedTime et, et1, et2;
	PRInt64 usecPer10Min;
	int day, hour, min;
	PRTime usecs;
	int dstInEffect = 0;

	if (debug_mode) {
	printf("\n");
	printf("*******************************************************\n");
	printf("**                                                   **\n");
	printf("**                 Stress test                       **\n");
	printf("**  Starting from midnight Jan. 1, 1997 PST,         **\n");
	printf("**  going back four years in 10-minute increment     **\n");
	printf("**                                                   **\n");
	printf("*******************************************************\n\n");
	}

	LL_I2L(usecPer10Min, 600000000L);

	/* 00:00:00 PST Jan. 1, 1997 */
	et.tm_usec = 0;
	et.tm_sec = 0;
	et.tm_min = 0;
	et.tm_hour = 0;
	et.tm_mday = 1;
	et.tm_month = 0;
	et.tm_year = 1997;
	et.tm_params.tp_gmt_offset = -8 * 3600;
	et.tm_params.tp_dst_offset = 0;
	usecs = PR_ImplodeTime(&et);

        for (day = 0; day < 4 * 365 + 1; day++) {
	    for (hour = 0; hour < 24; hour++) {
		for (min = 0; min < 60; min += 10) {
	            LL_SUB(usecs, usecs, usecPer10Min);
		    PR_ExplodeTime(usecs, PR_LocalTimeParameters, &et1);

		    et2 = et;
		    et2.tm_usec -= 600000000L;
		    PR_NormalizeTime(&et2, PR_LocalTimeParameters);

		    if (!ExplodedTimeIsEqual(&et1, &et2)) {
		        if (debug_mode) printf("ERROR: componentwise comparison failed\n");
			PrintExplodedTime(&et1);
			if (debug_mode) printf("\n");
			PrintExplodedTime(&et2);
			if (debug_mode) printf("\n");
		        return 1;
		    }

		    if (LL_NE(usecs, PR_ImplodeTime(&et1))) {
                   if (debug_mode)
					printf("ERROR: PR_ExplodeTime and PR_ImplodeTime are not inverse\n");
			PrintExplodedTime(&et1);
			if (debug_mode) printf("\n");
			failed_already=1;
		        return 1;
		    }
		    testParseTimeString(usecs);

		    if (!dstInEffect && et1.tm_params.tp_dst_offset) {
		        dstInEffect = 1;
		        if (debug_mode) printf("DST changeover from ");
			PrintExplodedTime(&et);
			if (debug_mode) printf(" to ");
			PrintExplodedTime(&et1);
			if (debug_mode) printf(".\n");
                    } else if (dstInEffect && !et1.tm_params.tp_dst_offset) {
		        dstInEffect = 0;
			if (debug_mode) printf("DST changeover from ");
			PrintExplodedTime(&et);
			if (debug_mode) printf(" to ");
			PrintExplodedTime(&et1);
			if (debug_mode) printf(".\n");
                    }

		    et = et1;
		}
	    }
        }
    }

#ifdef XP_MAC
	if (1)
	{
		char dummyChar;
		
		printf("Press return to exit\n\n");
		scanf("%c", &dummyChar);
	}
#endif

	if (failed_already) return 1;
	else return 0;

}
