/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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


int main(int argc, char **argv)
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
