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

/*
** File: ilock.c
** Description:  ilock.c is a unit test for nssilock. ilock.c
** tests the basic operation of nssilock. It should not be
** considered a complete test suite.
**
** To check that logging works, before running this test,
** define the following environment variables:
** 
**
**
**
**
*/

#include <stdio.h>
#include <stdlib.h>
#include <plgetopt.h> 
#include <nspr.h>
#include <nssilock.h>


/*
** Test harness infrastructure
*/
PRLogModuleInfo *lm;
PRLogModuleLevel msgLevel = PR_LOG_NONE;
PRIntn  debug = 0;
PRUint32  failed_already = 0;
/* end Test harness infrastructure */

PRIntn optIterations = 1; /* default iterations  */

PRIntn main(PRIntn argc, char *argv[])
{
    PRIntn i;
    {
        /*
        ** Get command line options
        */
        PLOptStatus os;
        PLOptState *opt = PL_CreateOptState(argc, argv, "hdvi:");

	    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
        {
		    if (PL_OPT_BAD == os) continue;
            switch (opt->option)
            {
            case 'd':  /* debug */
                debug = 1;
			    msgLevel = PR_LOG_ERROR;
                break;
            case 'v':  /* verbose mode */
			    msgLevel = PR_LOG_DEBUG;
                break;
            case 'i':  /* number of iterations */
			    optIterations = atol( opt->value );
                if ( 0 == optIterations ) optIterations = 1; /* coerce default on zero */
                break;
             default:
                break;
            }
        }
	    PL_DestroyOptState(opt);
    }

    for ( i = 0 ; i < optIterations ; i++ ) {
        /* First, test Lock */ 
        {
            PZLock *pl;
            PZMonitor *pm;
            PZCondVar *cv;
            PRStatus rc;

            pl = PZ_NewLock( nssILockOther );
            if ( NULL == pl )  {
                failed_already = PR_TRUE;
                goto Finished;
            }
            PZ_Lock( pl );
        
            rc = PZ_Unlock( pl );
            if ( PR_FAILURE == rc )  {
                failed_already = PR_TRUE;
                goto Finished;
            }
            PZ_DestroyLock( pl );

        /* now, test CVar */
            /* re-create the lock we just destroyed */
            pl = PZ_NewLock( nssILockOther );
            if ( NULL == pl )  {
                failed_already = PR_TRUE;
                goto Finished;
            }

            cv = PZ_NewCondVar( pl );
            if ( NULL == cv ) {
                failed_already = PR_TRUE;
                goto Finished;
            }

            PZ_Lock( pl );
            rc = PZ_NotifyCondVar( cv );
            if ( PR_FAILURE == rc ) {
                failed_already = PR_TRUE;
                goto Finished;
            }

            rc = PZ_NotifyAllCondVar( cv );
            if ( PR_FAILURE == rc ) {
                failed_already = PR_TRUE;
                goto Finished;
            }

            rc = PZ_WaitCondVar( cv, PR_SecondsToInterval(1));
            if ( PR_FAILURE == rc ) {
                if ( PR_UNKNOWN_ERROR != PR_GetError())  {
                    failed_already = PR_TRUE;
                    goto Finished;
                }
            }
            PZ_Unlock( pl );
            PZ_DestroyCondVar( cv );

        /* Now, test Monitor */
            pm = PZ_NewMonitor( nssILockOther );
            if ( NULL == pm )  {
                failed_already = PR_TRUE;
                goto Finished;
            }
        
            PZ_EnterMonitor( pm );

            rc = PZ_Notify( pm );
            if ( PR_FAILURE == rc )  {
                failed_already = PR_TRUE;
                goto Finished;
            }
            rc = PZ_NotifyAll( pm );
            if ( PR_FAILURE == rc )  {
                failed_already = PR_TRUE;
                goto Finished;
            }
            rc = PZ_Wait( pm, PR_INTERVAL_NO_WAIT );
            if ( PR_FAILURE == rc )  {
                failed_already = PR_TRUE;
                goto Finished;
            }
            rc = PZ_ExitMonitor( pm );
            if ( PR_FAILURE == rc )  {
                failed_already = PR_TRUE;
                goto Finished;
            }
            PZ_DestroyMonitor( pm );
        }
    } /* --- end for() --- */


Finished:
    if (debug) printf("%s\n", (failed_already)? "FAIL" : "PASS");
    return( (failed_already == PR_TRUE )? 1 : 0 );
}  /* main() */
/* end ilock.c */

