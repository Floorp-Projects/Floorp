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

#ifdef XP_MAC
#include "prlog.h"
#define printf PR_LogPrint
#else
#endif

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

#ifdef XP_MAC
	SetupMacPrintfLog("pr_open_re.log");
#endif
	
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
