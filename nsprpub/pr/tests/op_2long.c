/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
