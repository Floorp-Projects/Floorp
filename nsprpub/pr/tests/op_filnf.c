/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/***********************************************************************
**
** Name: op_filnf.c
**
** Description: Test Program to verify the PR_FILE_NOT_FOUND_ERROR
**				This test program also uses the TRUNCATE option
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

static PRFileDesc *t1;
PRIntn error_code;

int main(int argc, char **argv)
{
    PR_STDIO_INIT();
	t1 = PR_Open("/usr/tmp/ttools/err03.tmp", PR_TRUNCATE | PR_RDWR, 0666);
	if (t1 == NULL) {
		if (PR_GetError() == PR_FILE_NOT_FOUND_ERROR) {
				printf ("error code is %d \n", PR_GetError());
				printf ("PASS\n");
				return 0;
		}
		else {
				printf ("error code is %d \n", PR_GetError());
				printf ("FAIL\n");
			return 1;
		}
	}
	PR_Close(t1);
	printf ("opened a file that should not exist\n");
	printf ("FAIL\n");
	return 1;
}			
