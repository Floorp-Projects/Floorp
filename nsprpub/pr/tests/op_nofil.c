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

#ifdef XP_MAC
#include "prlog.h"
#define printf PR_LogPrint
#else
#endif

/*
 * A file name that cannot exist
 */
#define NO_SUCH_FILE "/no/such/file.tmp"

static PRFileDesc *t1;

int main(int argc, char **argv)
{

#ifdef XP_MAC
	SetupMacPrintfLog("pr_open_re.log");
#endif
	
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
