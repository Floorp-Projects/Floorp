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
** Name: dbmalloc1.c (OBSOLETE)
**
** Description: Tests PR_SetMallocCountdown PR_ClearMallocCountdown functions.
**
** Modification History:
** 14-May-97 AGarcia- Converted the test to accomodate the debug_mode flag.
**	         The debug mode will print all of the printfs associated with this test.
**			 The regress mode will be the default mode. Since the regress tool limits
**           the output to a one line status:PASS or FAIL,all of the printf statements
**			 have been handled with an if (debug_mode) statement.
** 04-June-97 AGarcia removed the Test_Result function. Regress tool has been updated to
**			recognize the return code from tha main program.
**
** 12-June-97 AGarcia Revert to return code 0 and 1, remove debug option (obsolete).
***********************************************************************/


/***********************************************************************
** Includes
***********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include "nspr.h"

PRIntn failed_already=0;
PRIntn debug_mode;

/* variable used for both r1 and r2 tests */
int should_fail =0;
int actually_failed=0;


void
r1
(
    void
)
{
    int i;
	actually_failed=0;
    for(  i = 0; i < 5; i++ )
    {
        void *x = PR_MALLOC(128);
        if( (void *)0 == x ) {
			if (debug_mode) printf("\tMalloc %d failed.\n", i+1);
			actually_failed = 1;
		}
        PR_DELETE(x);
    }

	if (((should_fail != actually_failed) & (!debug_mode))) failed_already=1;


    return;
}

void
r2
(
    void
)
{
    int i;

    for( i = 0; i <= 5; i++ )
    {
		should_fail =0;
        if( 0 == i ) {
			if (debug_mode) printf("No malloc should fail:\n");
		}
        else {
			if (debug_mode) printf("Malloc %d should fail:\n", i);
			should_fail = 1;
		}
        PR_SetMallocCountdown(i);
        r1();
        PR_ClearMallocCountdown();
    }
}

int
main
(
    int     argc,
    char   *argv[]
)
{

 /* main test */
	
    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    PR_STDIO_INIT();
    r2();

    if(failed_already)    
        return 1;
    else
        return 0;

    
}

