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

#ifdef XP_MAC
#include "prlog.h"
#define printf PR_LogPrint
#else
#endif

/*
 * The name of a file that is guaranteed to exist
 * on every machine of a particular OS.
 */
#ifdef XP_UNIX
#define EXISTING_FILENAME "/bin/sh"
#elif defined(WIN32)
#define EXISTING_FILENAME "c:/autoexec.bat"
#elif defined(OS2)
#define EXISTING_FILENAME "c:/config.sys"
#elif defined(BEOS)
#define EXISTING_FILENAME "/boot/beos/bin/sh"
#else
#error "Unknown OS"
#endif

static PRFileDesc *t1;

int main(int argc, char **argv)
{

#ifdef XP_MAC
	SetupMacPrintfLog("pr_open_re.log");
#endif
	
    PR_STDIO_INIT();

	t1 = PR_Open(EXISTING_FILENAME, PR_RDONLY, 0666);

	if (t1 == NULL) {
		printf ("error code is %d \n", PR_GetError());
		printf ("File %s should be found\n",
				EXISTING_FILENAME);
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
