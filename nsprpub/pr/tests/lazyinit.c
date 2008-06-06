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
** File:        lazyinit.c
** Description: Testing lazy initialization
**
**      Since you only get to initialize once, you have to rerun the test
**      for each test case. The test cases are numbered. If you want to
**      add more tests, take the next number and add it to the switch
**      statement.
**
**      This test is problematic on systems that don't support the notion
**      of console output. The workarounds to emulate that feature include
**      initializations themselves, which defeats the purpose here.
*/

#include "prcvar.h"
#include "prenv.h"
#include "prinit.h"
#include "prinrval.h"
#include "prio.h"
#include "prlock.h"
#include "prlog.h"
#include "prthread.h"
#include "prtypes.h"

#include <stdio.h>
#include <stdlib.h>

static void PR_CALLBACK lazyEntry(void *arg)
{
    PR_ASSERT(NULL == arg);
}  /* lazyEntry */


PRIntn main(PRIntn argc, char *argv[])
{
    PRUintn pdkey;
    PRStatus status;
    char *path = NULL;
    PRDir *dir = NULL;
    PRLock *ml = NULL;
    PRCondVar *cv = NULL;
    PRThread *thread = NULL;
    PRIntervalTime interval = 0;
    PRFileDesc *file, *udp, *tcp, *pair[2];
    PRIntn test;

    if ( argc < 2)
    {
        test = 0;
    }
    else
        test = atoi(argv[1]);
        
    switch (test)
    {
        case 0: ml = PR_NewLock(); 
            break;
            
        case 1: interval = PR_SecondsToInterval(1);
            break;
            
        case 2: thread = PR_CreateThread(
            PR_USER_THREAD, lazyEntry, NULL, PR_PRIORITY_NORMAL,
            PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0); 
            break;
            
        case 3: file = PR_Open("/usr/tmp/", PR_RDONLY, 0); 
            break;
            
        case 4: udp = PR_NewUDPSocket(); 
            break;
            
        case 5: tcp = PR_NewTCPSocket(); 
            break;
            
        case 6: dir = PR_OpenDir("/usr/tmp/"); 
            break;
            
        case 7: (void)PR_NewThreadPrivateIndex(&pdkey, NULL);
            break;
        
        case 8: path = PR_GetEnv("PATH");
            break;
            
        case 9: status = PR_NewTCPSocketPair(pair);
            break;
            
        case 10: PR_SetConcurrency(2);
            break;
            
        default: 
            printf(
                "lazyinit: unrecognized command line argument: %s\n", 
                argv[1] );
            printf( "FAIL\n" );
            exit( 1 );
            break;
    } /* switch() */
    return 0;
}  /* Lazy */

/* lazyinit.c */
