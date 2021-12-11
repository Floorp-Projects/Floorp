/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/***********************************************************************
**
** Name: op_2long.c
**
** Description: Test Program to verify the PR_NAME_TOO_LONG_ERROR
**
** Modification History:
** 03-June-97 AGarcia- Initial version
***********************************************************************/

/***********************************************************************
** Includes
***********************************************************************/
/* Used to get the command line option */
#include "prinit.h"
#include "prmem.h"
#include "prio.h"
#include "prerror.h"
#include <stdio.h>
#include "plerror.h"
#include "plgetopt.h"

static PRFileDesc *t1;
PRIntn error_code;

/*
 * should exceed any system's maximum file name length
 * Note: was set at 4096. This is legal on some unix (Linux 2.1+) platforms.
 *
 */
#define TOO_LONG 5000

int main(int argc, char **argv)
{
    char nameTooLong[TOO_LONG];
    int i;

    /* Generate a really long pathname */
    for (i = 0; i < TOO_LONG - 1; i++) {
        if (i % 10 == 0) {
            nameTooLong[i] = '/';
        } else {
            nameTooLong[i] = 'a';
        }
    }
    nameTooLong[TOO_LONG - 1] = 0;

    PR_STDIO_INIT();
    t1 = PR_Open(nameTooLong, PR_RDWR, 0666);
    if (t1 == NULL) {
        if (PR_GetError() == PR_NAME_TOO_LONG_ERROR) {
            PL_PrintError("error code is");
            printf ("PASS\n");
            return 0;
        }
        else {
            PL_PrintError("error code is");
            printf ("FAIL\n");
            return 1;
        }
    }

    else {
        printf ("Test passed\n");
        return 0;
    }



}
