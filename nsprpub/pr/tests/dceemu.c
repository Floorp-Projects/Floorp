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

/*
** File:        dceemu.c
** Description: testing the DCE emulation api
**
** Modification History:
** 14-May-97 AGarcia- Converted the test to accomodate the debug_mode flag.
**	         The debug mode will print all of the printfs associated with this test.
**			 The regress mode will be the default mode. Since the regress tool limits
**           the output to a one line status:PASS or FAIL,all of the printf statements
**             have been handled with an if (debug_mode) statement.
** 04-June-97 AGarcia removed the Test_Result function. Regress tool has been updated to
**            recognize the return code from tha main program.
** 12-June-97 Revert to return code 0 and 1, remove debug option (obsolete).
**/

/***********************************************************************
** Includes
***********************************************************************/


#if defined(_PR_DCETHREADS)

#include "prlog.h"
#include "prinit.h"
#include "prpdce.h"

#include <stdio.h>
#include <stdlib.h>
PRIntn failed_already=0;
PRIntn debug_mode=0;

static PRIntn prmain(PRIntn argc, char **argv)
{
    PRStatus rv;
    PRLock *ml = PR_NewLock();
    PRCondVar *cv = PRP_NewNakedCondVar();
    PRIntervalTime tenmsecs = PR_MillisecondsToInterval(10);

    rv = PRP_TryLock(ml);
    PR_ASSERT(PR_SUCCESS == rv);
    if ((rv != PR_SUCCESS) & (!debug_mode)) failed_already=1; 
    
    rv = PRP_TryLock(ml);
    PR_ASSERT(PR_FAILURE == rv);
    if ((rv != PR_FAILURE) & (!debug_mode)) failed_already=1; 

    rv = PRP_NakedNotify(cv);
    PR_ASSERT(PR_SUCCESS == rv);
    if ((rv != PR_SUCCESS) & (!debug_mode)) failed_already=1; 

    rv = PRP_NakedBroadcast(cv);
    PR_ASSERT(PR_SUCCESS == rv);
    if ((rv != PR_SUCCESS) & (!debug_mode)) failed_already=1; 

    rv = PRP_NakedWait(cv, ml, tenmsecs);
    PR_ASSERT(PR_SUCCESS == rv);
    if ((rv != PR_SUCCESS) & (!debug_mode)) failed_already=1;     

    PR_Unlock(ml);    
        
    rv = PRP_NakedNotify(cv);
    PR_ASSERT(PR_SUCCESS == rv);
    if ((rv != PR_SUCCESS) & (!debug_mode)) failed_already=1;     

    rv = PRP_NakedBroadcast(cv);
    PR_ASSERT(PR_SUCCESS == rv);
    if ((rv != PR_SUCCESS) & (!debug_mode)) failed_already=1;     

    PRP_DestroyNakedCondVar(cv);
    PR_DestroyLock(ml);

    if (debug_mode) printf("Test succeeded\n");

    return 0;

}  /* prmain */

#endif /* #if defined(_PR_DCETHREADS) */

int main(int argc, char **argv)
{

#if defined(_PR_DCETHREADS)
    PR_Initialize(prmain, argc, argv, 0);
    if(failed_already)    
        return 1;
    else
        return 0;
#else
    return 0;
#endif
}  /* main */


/* decemu.c */
