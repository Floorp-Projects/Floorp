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
** File: ntioto.c
** Description: 
** This test, ntioto.c, was designed to reproduce a bug reported by NES
** on WindowsNT (fibers implementation). NSPR was asserting in ntio.c
** after PR_AcceptRead() had timed out. I/O performed subsequent to the
** call to PR_AcceptRead() could complete on a CPU other than the one
** on which it was started. The assert in ntio.c detected this, then
** asserted.
**
** Design:
** This test will fail with an assert in ntio.c if the problem it was
** designed to catch occurs. It returns 0 otherwise.
** 
** The main() thread initializes and tears things down. A file is
** opened for writing; this file will be written to by AcceptThread()
** and JitterThread().  Main() creates a socket for reading, listens
** and binds the socket.
** 
** ConnectThread() connects to the socket created by main, then polls
** the "state" variable. When state is AllDone, ConnectThread() exits.
**
** AcceptThread() calls PR_AcceptRead() on the socket. He fully expects
** it to time out. After the timeout, AccpetThread() interacts with
** JitterThread() via a common condition variable and the state
** variable. The two threads ping-pong back and forth, each thread
** writes the the file opened by main. This should provoke the
** condition reported by NES (if we didn't fix it).
**
** The failure is not solid. It may fail within a few ping-pongs between
** AcceptThread() and JitterThread() or may take a while. The default
** iteration count, jitter, is set by DEFAULT_JITTER. This may be
** modified at the command line with the -j option.
** 
*/

#include <plgetopt.h> 
#include <nspr.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
** Test harness infrastructure
*/
PRLogModuleInfo *lm;
PRLogModuleLevel msgLevel = PR_LOG_NONE;
PRIntn  debug = 0;
PRIntn  verbose = 0;
PRUint32  failed_already = 0;
/* end Test harness infrastructure */

/* JITTER_DEFAULT: the number of times AcceptThread() and JitterThread() ping-pong */
#define JITTER_DEFAULT  100000
#define BASE_PORT 9867

PRIntervalTime timeout;
PRNetAddr   listenAddr;
PRFileDesc  *listenSock;
PRLock      *ml;
PRCondVar   *cv;
volatile enum  {
    RunJitter,
    RunAcceptRead,
    AllDone
} state = RunAcceptRead;
PRFileDesc  *file1;
PRIntn  iCounter = 0;
PRIntn  jitter = JITTER_DEFAULT;
PRBool  resume = PR_FALSE;

/*
** Emit help text for this test
*/
static void Help( void )
{
    printf("Template: Help(): display your help message(s) here");
    exit(1);
} /* end Help() */


/*
** static computation of PR_AcceptRead() buffer size.
*/
#define ACCEPT_READ_DATASIZE 10
#define ACCEPT_READ_BUFSIZE (PR_ACCEPT_READ_BUF_OVERHEAD + ACCEPT_READ_DATASIZE)

static void AcceptThread(void *arg)
{
    PRIntn bytesRead;
    char dataBuf[ACCEPT_READ_BUFSIZE];
    PRFileDesc  *arSock;
    PRNetAddr   *arAddr;

    bytesRead = PR_AcceptRead( listenSock, 
        &arSock,
        &arAddr,
        dataBuf,
        ACCEPT_READ_DATASIZE,
        PR_SecondsToInterval(1));

    if ( bytesRead == -1 && PR_GetError() == PR_IO_TIMEOUT_ERROR )
        if ( debug ) printf("AcceptRead timed out\n");
    else
        if ( debug ) printf("Oops! read: %d, error: %d\n", bytesRead, PR_GetError());

    while( state != AllDone )  {
        PR_Lock( ml );
        while( state != RunAcceptRead )
            PR_WaitCondVar( cv, PR_INTERVAL_NO_TIMEOUT );
        if ( ++iCounter >= jitter )
            state = AllDone;
        else
            state = RunJitter;
        if ( verbose ) printf(".");
        PR_NotifyCondVar( cv );
        PR_Unlock( ml );
        PR_Write( file1, ".", 1 );
    }

    return;
} /* end AcceptThread() */

static void JitterThread(void *arg)
{
    while( state != AllDone )  {
        PR_Lock( ml );
        while( state != RunJitter && state != AllDone )
            PR_WaitCondVar( cv, PR_INTERVAL_NO_TIMEOUT );
        if ( state != AllDone)
            state = RunAcceptRead;
        if ( verbose ) printf("+");
        PR_NotifyCondVar( cv );
        PR_Unlock( ml );
        PR_Write( file1, "+", 1 );
    }
    return;
} /* end Goofy() */

static void ConnectThread( void *arg )
{
    PRStatus    rv;
    PRFileDesc  *clientSock;
    PRNetAddr   serverAddress;
    clientSock = PR_NewTCPSocket();

    PR_ASSERT(clientSock);

    if ( resume ) {
        if ( debug ) printf("pausing 3 seconds before connect\n");
        PR_Sleep( PR_SecondsToInterval(3));
    }

    memset(&serverAddress, 0, sizeof(serverAddress));
    rv = PR_InitializeNetAddr(PR_IpAddrLoopback, BASE_PORT, &serverAddress);
    PR_ASSERT( PR_SUCCESS == rv );
    rv = PR_Connect( clientSock, 
        &serverAddress, 
        PR_SecondsToInterval(1));
    PR_ASSERT( PR_SUCCESS == rv );

    /* that's all we do. ... Wait for the acceptread() to timeout */
    while( state != AllDone )
        PR_Sleep( PR_SecondsToInterval(1));
    return;
} /* end ConnectThread() */


PRIntn main(PRIntn argc, char *argv[])
{
    PRThread *tJitter;
    PRThread *tAccept;
    PRThread *tConnect;
    PRStatus rv;
    /* This test if valid for WinNT only! */

#if !defined(WINNT)
    return 0;
#endif

    {
        /*
        ** Get command line options
        */
        PLOptStatus os;
        PLOptState *opt = PL_CreateOptState(argc, argv, "hdrvj:");

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
                verbose = 1;
			    msgLevel = PR_LOG_DEBUG;
                break;
            case 'j':
                jitter = atoi(opt->value);
                if ( jitter == 0)
                    jitter = JITTER_DEFAULT;
                break;
            case 'r':
                resume = PR_TRUE;
                break;
            case 'h':  /* help message */
			    Help();
                break;
             default:
                break;
            }
        }
	    PL_DestroyOptState(opt);
    }

    lm = PR_NewLogModule("Test");       /* Initialize logging */

    /* set concurrency */
    PR_SetConcurrency( 4 );

    /* setup thread synchronization mechanics */
    ml = PR_NewLock();
    cv = PR_NewCondVar( ml );

    /* setup a tcp socket */
    memset(&listenAddr, 0, sizeof(listenAddr));
    rv = PR_InitializeNetAddr(PR_IpAddrAny, BASE_PORT, &listenAddr);
    PR_ASSERT( PR_SUCCESS == rv );

    listenSock = PR_NewTCPSocket();
    PR_ASSERT( listenSock );

    rv = PR_Bind( listenSock, &listenAddr);
    PR_ASSERT( PR_SUCCESS == rv );

    rv = PR_Listen( listenSock, 5 );
    PR_ASSERT( PR_SUCCESS == rv );

    /* open a file for writing, provoke bug */
    file1 = PR_Open("xxxTestFile", PR_CREATE_FILE | PR_RDWR, 666);

    /* create Connect thread */
    tConnect = PR_CreateThread(
        PR_USER_THREAD, ConnectThread, NULL,
        PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,
        PR_JOINABLE_THREAD, 0 );
    PR_ASSERT( tConnect );

    /* create jitter off thread */
    tJitter = PR_CreateThread(
        PR_USER_THREAD, JitterThread, NULL,
        PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,
        PR_JOINABLE_THREAD, 0 );
    PR_ASSERT( tJitter );

    /* create acceptread thread */
    tAccept = PR_CreateThread(
        PR_USER_THREAD, AcceptThread, NULL,
        PR_PRIORITY_NORMAL, PR_LOCAL_THREAD,
        PR_JOINABLE_THREAD, 0 );
    PR_ASSERT( tAccept );

    /* wait for all threads to quit, then terminate gracefully */
    PR_JoinThread( tConnect );
    PR_JoinThread( tAccept );
    PR_JoinThread( tJitter );
    PR_Close( listenSock );
    PR_DestroyCondVar(cv);
    PR_DestroyLock(ml);
    PR_Close( file1 );
    PR_Delete( "xxxTestFile");

    /* test return and exit */
    if (debug) printf("%s\n", (failed_already)? "FAIL" : "PASS");
    return( (failed_already == PR_TRUE )? 1 : 0 );
}  /* main() */
/* end ntioto.c */
