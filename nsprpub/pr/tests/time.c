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
 * Program to test different ways to get the time; right now it is tuned
 * only for solaris.
 *   solaris results (100000 iterations):
 *          time to get time with time():   4.63 usec avg, 463 msec total
 *     time to get time with gethrtime():   2.17 usec avg, 217 msec total
 *  time to get time with gettimeofday():   1.25 usec avg, 125 msec total
 *
 *
 */
/***********************************************************************
** Includes
***********************************************************************/
/* Used to get the command line option */
#include "plgetopt.h"

#include "nspr.h"
#include "prpriv.h"
#include "prinrval.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_COUNT 100000
PRInt32 count;

time_t itime;
hrtime_t ihrtime;

void
ftime_init()
{
   itime = time(NULL);
   ihrtime = gethrtime();
}

time_t
ftime()
{
        hrtime_t now = gethrtime();

        return itime + ((now - ihrtime) / 1000000000ll);
}

static void timeTime(void)
{
    PRInt32 index = count;
    time_t rv;
 
    for (;index--;)
        rv = time(NULL);
}

static void timeGethrtime(void)
{
    PRInt32 index = count;
    time_t rv;
 
    for (;index--;)
        rv = ftime();
}

#include <sys/time.h>
static void timeGettimeofday(void)
{
    PRInt32 index = count;
    time_t rv;
    struct timeval tp;
 
    for (;index--;)
        rv = gettimeofday(&tp, NULL);
}

static void timePRTime32(void)
{
    PRInt32 index = count;
    PRInt32 rv32;
    PRTime q;
    PRTime rv;

    LL_I2L(q, 1000000);
 
    for (;index--;) {
        rv = PR_Now();
        LL_DIV(rv, rv, q);
        LL_L2I(rv32, rv);
    }
}

static void timePRTime64(void)
{
    PRInt32 index = count;
    PRTime rv;
 
    for (;index--;)
        rv = PR_Now();
}

/************************************************************************/

static void Measure(void (*func)(void), const char *msg)
{
    PRIntervalTime start, stop;
    double d;
    PRInt32 tot;

    start = PR_IntervalNow();
    (*func)();
    stop = PR_IntervalNow();

    d = (double)PR_IntervalToMicroseconds(stop - start);
    tot = PR_IntervalToMilliseconds(stop-start);

    if (debug_mode) printf("%40s: %6.2f usec avg, %d msec total\n", msg, d / count, tot);
}

void main(int argc, char **argv)
{
	/* The command line argument: -d is used to determine if the test is being run
	in debug mode. The regress tool requires only one line output:PASS or FAIL.
	All of the printfs associated with this test has been handled with a if (debug_mode)
	test.
	Usage: test_name -d
	*/
	PLOptStatus os;
	PLOptState *opt = PL_CreateOptState(argc, argv, "d:");
	while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
		if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'd':  /* debug mode */
			debug_mode = 1;
            break;
         default:
            break;
        }
    }
	PL_DestroyOptState(opt);

    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    PR_STDIO_INIT();

    if (argc > 1) {
	count = atoi(argv[1]);
    } else {
	count = DEFAULT_COUNT;
    }

    ftime_init();

    Measure(timeTime, "time to get time with time()");
    Measure(timeGethrtime, "time to get time with gethrtime()");
    Measure(timeGettimeofday, "time to get time with gettimeofday()");
    Measure(timePRTime32, "time to get time with PR_Time() (32bit)");
    Measure(timePRTime64, "time to get time with PR_Time() (64bit)");

	PR_Cleanup();
	return 0;
}



