/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/***********************************************************************
**
** Name: dbmalloc1.c
**
** Description: Tests PR_SetMallocCountdown PR_ClearMallocCountdown functions.
**
** Modification History:
**
** 19-May-97 AGarcia - separate the four join tests into different unit test modules.
**           AGarcia- Converted the test to accomodate the debug_mode flag.
**           The debug mode will print all of the printfs associated with this test.
**           The regress mode will be the default mode. Since the regress tool limits
**           the output to a one line status:PASS or FAIL,all of the printf statements
**           have been handled with an if (debug_mode) statement.
** 04-June-97 AGarcia removed the Test_Result function. Regress tool has been updated to
**          recognize the return code from tha main program.
***********************************************************************/

/***********************************************************************
** Includes
***********************************************************************/
/* Used to get the command line option */
#include "plgetopt.h"

#include "nspr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PRIntn failed_already=0;
PRIntn debug_mode;
/*
    Program to test joining of threads.  Two threads are created.  One
    to be waited upon until it has started.  The other to join after it has
    completed.
*/


static void lowPriority(void *arg)
{
}

static void highPriority(void *arg)
{
}

void runTest(PRThreadScope scope1, PRThreadScope scope2)
{
    PRThread *low,*high;

    /* create the low and high priority threads */

    low = PR_CreateThread(PR_USER_THREAD,
                          lowPriority, 0,
                          PR_PRIORITY_LOW,
                          scope1,
                          PR_JOINABLE_THREAD,
                          0);
    if (!low) {
        if (debug_mode) {
            printf("\tcannot create low priority thread\n");
        }
        else {
            failed_already=1;
        }
        return;
    }

    high = PR_CreateThread(PR_USER_THREAD,
                           highPriority, 0,
                           PR_PRIORITY_HIGH,
                           scope2,
                           PR_JOINABLE_THREAD,
                           0);
    if (!high) {
        if (debug_mode) {
            printf("\tcannot create high priority thread\n");
        }
        else {
            failed_already=1;
        }
        return;
    }

    /* Do the joining for both threads */
    if (PR_JoinThread(low) == PR_FAILURE) {
        if (debug_mode) {
            printf("\tcannot join low priority thread\n");
        }
        else {
            failed_already=1;
        }
        return;
    } else {
        if (debug_mode) {
            printf("\tjoined low priority thread\n");
        }
    }
    if (PR_JoinThread(high) == PR_FAILURE) {
        if (debug_mode) {
            printf("\tcannot join high priority thread\n");
        }
        else {
            failed_already=1;
        }
        return;
    } else {
        if (debug_mode) {
            printf("\tjoined high priority thread\n");
        }
    }
}

static PRIntn PR_CALLBACK RealMain( PRIntn argc, char **argv )
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
        if (PL_OPT_BAD == os) {
            continue;
        }
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

    /* main test */

    if (debug_mode) {
        printf("Kernel-Kernel test\n");
    }
    runTest(PR_GLOBAL_THREAD, PR_GLOBAL_THREAD);

    if(failed_already)
    {
        printf("FAIL\n");
        return 1;
    }
    else
    {
        printf("PASS\n");
        return 0;
    }

}

int main(int argc, char **argv)
{
    PRIntn rv;

    PR_STDIO_INIT();
    rv = PR_Initialize(RealMain, argc, argv, 0);
    return rv;
}  /* main */
