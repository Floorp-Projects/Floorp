/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/***********************************************************************
**
** Name: op_noacc.c
**
** Description: Test Program to verify the PR_NO_ACCESS_RIGHTS_ERROR in PR_Open
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

static PRFileDesc *err01;
PRIntn error_code;

int main(int argc, char **argv)
{
#ifdef XP_PC
    printf("op_noacc: Test not valid on MS-Windows.\n\tNo concept of 'mode' on Open() call\n");
    return(0);
#endif


    PR_STDIO_INIT();
    err01 = PR_Open("err01.tmp", PR_CREATE_FILE | PR_RDWR, 0);
    if (err01 == NULL) {
        int error = PR_GetError();
        printf ("error code is %d\n", error);
        if (error == PR_NO_ACCESS_RIGHTS_ERROR) {
            printf ("PASS\n");
            return 0;
        }
    }
    printf ("FAIL\n");
    return 1;
}

