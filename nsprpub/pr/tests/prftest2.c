/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** File:    prftest2.c
** Description:
**     This is a simple test of the PR_snprintf() function defined
**     in prprf.c.
**
** Modification History:
** 14-May-97 AGarcia- Converted the test to accomodate the debug_mode flag.
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

#include "prlong.h"
#include "prinit.h"
#include "prprf.h"

#include <string.h>

#define BUF_SIZE 128

PRIntn failed_already=0;
PRIntn debug_mode;

int main(int argc, char **argv)
{
    PRInt16 i16;
    PRIntn n;
    PRInt32 i32;
    PRInt64 i64;
    char buf[BUF_SIZE];

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


    PR_STDIO_INIT();
    i16 = -32;
    n = 30;
    i32 = 64;
    LL_I2L(i64, 333);
    PR_snprintf(buf, BUF_SIZE, "%d %hd %lld %ld", n, i16, i64, i32);
    if (!strcmp(buf, "30 -32 333 64")) {
        if (debug_mode) {
            printf("PR_snprintf test 2 passed\n");
        }
    } else {
        if (debug_mode) {
            printf("PR_snprintf test 2 failed\n");
            printf("Converted string is %s\n", buf);
            printf("Should be 30 -32 333 64\n");
        }
        else {
            failed_already=1;
        }
    }
    if(failed_already)
    {
        printf("FAILED\n");
        return 1;
    }
    else
    {
        printf("PASSED\n");
        return 0;
    }
}
