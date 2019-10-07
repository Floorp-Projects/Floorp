/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/***********************************************************************
**
** Name: op_filok.c
**
** Description: Test Program to verify the PR_Open finding an existing file.
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

static PRFileDesc *t1;

int main(int argc, char **argv)
{
    PR_STDIO_INIT();

    t1 = PR_Open(argv[0], PR_RDONLY, 0666);

    if (t1 == NULL) {
        printf ("error code is %d \n", PR_GetError());
        printf ("File %s should be found\n", argv[0]);
        return 1;
    } else {
        if (PR_Close(t1) == PR_SUCCESS) {
            printf ("Test passed \n");
            return 0;
        } else {
            printf ("cannot close file\n");
            printf ("error code is %d\n", PR_GetError());
            return 1;
        }
    }
}
