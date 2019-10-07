/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/***********************************************************************
**
** Name: op_nofil.c
**
** Description: Test Program to verify the PR_FILE_NOT_FOUND_ERROR
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
#include "plgetopt.h"

/*
 * A file name that cannot exist
 */
#define NO_SUCH_FILE "/no/such/file.tmp"

static PRFileDesc *t1;

int main(int argc, char **argv)
{
    PR_STDIO_INIT();
    t1 = PR_Open(NO_SUCH_FILE,  PR_RDONLY, 0666);
    if (t1 == NULL) {
        if (PR_GetError() == PR_FILE_NOT_FOUND_ERROR) {
            printf ("error code is PR_FILE_NOT_FOUND_ERROR, as expected\n");
            printf ("PASS\n");
            return 0;
        } else {
            printf ("error code is %d \n", PR_GetError());
            printf ("FAIL\n");
            return 1;
        }
    }
    printf ("File %s exists on this machine!?\n", NO_SUCH_FILE);
    if (PR_Close(t1) == PR_FAILURE) {
        printf ("cannot close file\n");
        printf ("error code is %d \n", PR_GetError());
    }
    printf ("FAIL\n");
    return 1;
}
