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
#include <sys/time.h>

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



