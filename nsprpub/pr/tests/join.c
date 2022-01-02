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
**          AGarcia- Converted the test to accomodate the debug_mode flag.
**          The debug mode will print all of the printfs associated with this test.
**          The regress mode will be the default mode. Since the regress tool limits
**          the output to a one line status:PASS or FAIL,all of the printf statements
**          have been handled with an if (debug_mode) statement.
***********************************************************************/

/***********************************************************************
** Includes
***********************************************************************/
/* Used to get the command line option */
#include "plgetopt.h"
#include "prttools.h"

#include "nspr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/***********************************************************************
** PRIVATE FUNCTION:    Test_Result
** DESCRIPTION: Used in conjunction with the regress tool, prints out the
**              status of the test case.
** INPUTS:      PASS/FAIL
** OUTPUTS:     None
** RETURN:      None
** SIDE EFFECTS:
**
** RESTRICTIONS:
**      None
** MEMORY:      NA
** ALGORITHM:   Determine what the status is and print accordingly.
**
***********************************************************************/


static void Test_Result (int result)
{
    if (result == PASS) {
        printf ("PASS\n");
    }
    else {
        printf ("FAIL\n");
    }
    exit (1);
}


/*
    Program to test joining of threads.  Two threads are created.  One
    to be waited upon until it has started.  The other to join after it has
    completed.
*/


static void PR_CALLBACK lowPriority(void *arg)
{
}

static void PR_CALLBACK highPriority(void *arg)
{
}

static void PR_CALLBACK unjoinable(void *arg)
{
    PR_Sleep(PR_INTERVAL_NO_TIMEOUT);
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
            Test_Result(FAIL);
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
            Test_Result(FAIL);
        }
        return;
    }

    /* Do the joining for both threads */
    if (PR_JoinThread(low) == PR_FAILURE) {
        if (debug_mode) {
            printf("\tcannot join low priority thread\n");
        }
        else {
            Test_Result (FAIL);
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
            Test_Result(FAIL);
        }
        return;
    } else {
        if (debug_mode) {
            printf("\tjoined high priority thread\n");
        }
    }
}

void joinWithUnjoinable(void)
{
    PRThread *thread;

    /* create the unjoinable thread */

    thread = PR_CreateThread(PR_USER_THREAD,
                             unjoinable, 0,
                             PR_PRIORITY_NORMAL,
                             PR_GLOBAL_THREAD,
                             PR_UNJOINABLE_THREAD,
                             0);
    if (!thread) {
        if (debug_mode) {
            printf("\tcannot create unjoinable thread\n");
        }
        else {
            Test_Result(FAIL);
        }
        return;
    }

    if (PR_JoinThread(thread) == PR_SUCCESS) {
        if (debug_mode) {
            printf("\tsuccessfully joined with unjoinable thread?!\n");
        }
        else {
            Test_Result(FAIL);
        }
        return;
    } else {
        if (debug_mode) {
            printf("\tcannot join with unjoinable thread, as expected\n");
        }
        if (PR_GetError() != PR_INVALID_ARGUMENT_ERROR) {
            if (debug_mode) {
                printf("\tWrong error code\n");
            }
            else {
                Test_Result(FAIL);
            }
            return;
        }
    }
    if (PR_Interrupt(thread) == PR_FAILURE) {
        if (debug_mode) {
            printf("\tcannot interrupt unjoinable thread\n");
        }
        else {
            Test_Result(FAIL);
        }
        return;
    } else {
        if (debug_mode) {
            printf("\tinterrupted unjoinable thread\n");
        }
    }
}

static PRIntn PR_CALLBACK RealMain(int argc, char **argv)
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
    printf("User-User test\n");
    runTest(PR_LOCAL_THREAD, PR_LOCAL_THREAD);
    printf("User-Kernel test\n");
    runTest(PR_LOCAL_THREAD, PR_GLOBAL_THREAD);
    printf("Kernel-User test\n");
    runTest(PR_GLOBAL_THREAD, PR_LOCAL_THREAD);
    printf("Kernel-Kernel test\n");
    runTest(PR_GLOBAL_THREAD, PR_GLOBAL_THREAD);
    printf("Join with unjoinable thread\n");
    joinWithUnjoinable();

    printf("PASSED\n");

    return 0;
}




int main(int argc, char **argv)
{
    PRIntn rv;

    PR_STDIO_INIT();
    rv = PR_Initialize(RealMain, argc, argv, 0);
    return rv;
}  /* main */
