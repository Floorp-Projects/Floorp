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

/*******************************************************************
** udpsrc.c -- Test basic function of UDP server
**
** udpsrv operates on the same machine with program udpclt.
** udpsrv is the server side of a udp sockets application.
** udpclt is the client side of a udp sockets application.
**
** The test is designed to assist developers in porting/debugging
** the UDP socket functions of NSPR.
**
** This test is not a stress test.
**
** main() starts two threads: UDP_Server() and UDP_Client();
** main() uses PR_JoinThread() to wait for the threads to complete.
**
** UDP_Server() does repeated recvfrom()s from a socket.
** He detects an EOF condition set by UDP_Client(). For each
** packet received by UDP_Server(), he checks its content for
** expected content, then sends the packet back to UDP_Client().
** 
** UDP_Client() sends packets to UDP_Server() using sendto()
** he recieves packets back from the server via recvfrom().
** After he sends enough packets containing UDP_AMOUNT_TO_WRITE
** bytes of data, he sends an EOF message.
** 
** The test issues a pass/fail message at end.
** 
** Notes:
** The variable "_debug_on" can be set to 1 to cause diagnostic
** messages related to client/server synchronization. Useful when
** the test hangs.
** 
** Error messages are written to stdout.
** 
********************************************************************
*/
/* --- include files --- */
#include "nspr.h"
#include "prpriv.h"

#include "plgetopt.h"
#include "prttools.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef XP_PC
#define mode_t int
#endif

#define UDP_BUF_SIZE            4096
#define UDP_DGRAM_SIZE          128
#define UDP_AMOUNT_TO_WRITE     (PRInt32)((UDP_DGRAM_SIZE * 1000l) +1)
#define NUM_UDP_CLIENTS         1
#define NUM_UDP_DATAGRAMS_PER_CLIENT    5
#define UDP_SERVER_PORT         9050
#define UDP_CLIENT_PORT         9053
#define MY_INADDR               PR_INADDR_ANY
#define PEER_INADDR             PR_INADDR_LOOPBACK

#define UDP_TIMEOUT             400000
/* #define UDP_TIMEOUT             PR_INTERVAL_NO_TIMEOUT */

/* --- static data --- */
static PRIntn _debug_on      = 0;
static PRBool passed         = PR_TRUE;
static PRUint32 cltBytesRead = 0;
static PRUint32 srvBytesRead = 0;
static PRFileDesc *output    = NULL;

/* --- static function declarations --- */
#define DPRINTF(arg) if (_debug_on) PR_fprintf(output, arg)



/*******************************************************************
** ListNetAddr() -- Display the Net Address on stdout
**
** Description: displays the component parts of a PRNetAddr struct
**
** Arguments:   address of PRNetAddr structure to display
**
** Returns: void
**
** Notes:
**
********************************************************************
*/
void ListNetAddr( char *msg, PRNetAddr *na )
{
    char    mbuf[256];
    
    sprintf( mbuf, "ListNetAddr: %s family: %d, port: %d, ip: %8.8X\n",
            msg, na->inet.family, PR_ntohs( na->inet.port), PR_ntohl(na->inet.ip) );
#if 0            
    DPRINTF( mbuf );            
#endif
} /* --- end ListNetAddr() --- */

/********************************************************************
** UDP_Server() -- Test a UDP server application
**
** Description: The Server side of a UDP Client/Server application.
**
** Arguments: none
**
** Returns: void
**
** Notes:
**
**
********************************************************************
*/
static void PR_CALLBACK UDP_Server( void *arg )
{
    static char     svrBuf[UDP_BUF_SIZE];
    PRFileDesc      *svrSock;
    PRInt32         rv;
    PRNetAddr       netaddr;
    PRBool          bound = PR_FALSE;
    PRBool          endOfInput = PR_FALSE;
    PRInt32         numBytes = UDP_DGRAM_SIZE;
    
    DPRINTF("udpsrv: UDP_Server(): starting\n" );

    /* --- Create the socket --- */
    DPRINTF("udpsrv: UDP_Server(): Creating UDP Socket\n" );
    svrSock = PR_NewUDPSocket();
    if ( svrSock == NULL )
    {
        passed = PR_FALSE;
        if (debug_mode)
            PR_fprintf(output,
                "udpsrv: UDP_Server(): PR_NewUDPSocket() returned NULL\n" );
        return;
    }
    
    /* --- Initialize the sockaddr_in structure --- */
    memset( &netaddr, 0, sizeof( netaddr )); 
    netaddr.inet.family = PR_AF_INET;
    netaddr.inet.port   = PR_htons( UDP_SERVER_PORT );
    netaddr.inet.ip     = PR_htonl( MY_INADDR );
    
    /* --- Bind the socket --- */
    while ( !bound )
    {
        DPRINTF("udpsrv: UDP_Server(): Binding socket\n" );
        rv = PR_Bind( svrSock, &netaddr );
        if ( rv < 0 )
        {
            if ( PR_GetError() == PR_ADDRESS_IN_USE_ERROR )
            {
                if (debug_mode) PR_fprintf(output, "udpsrv: UDP_Server(): \
						PR_Bind(): reports: PR_ADDRESS_IN_USE_ERROR\n");
                PR_Sleep( PR_MillisecondsToInterval( 2000 ));
                continue;
            }
            else
            {
                passed = PR_FALSE;
                if (debug_mode) PR_fprintf(output, "udpsrv: UDP_Server(): \
						PR_Bind(): failed: %ld with error: %ld\n",
                        rv, PR_GetError() );
                PR_Close( svrSock );
                return;
            }
        }
        else
            bound = PR_TRUE;
    }
    ListNetAddr( "UDP_Server: after bind", &netaddr );
    
    /* --- Recv the socket --- */
    while( !endOfInput )
    {
        DPRINTF("udpsrv: UDP_Server(): RecvFrom() socket\n" );
        rv = PR_RecvFrom( svrSock, svrBuf, numBytes, 0, &netaddr, UDP_TIMEOUT );
        if ( rv == -1 )
        {
            passed = PR_FALSE;
            if (debug_mode)
                PR_fprintf(output,
                    "udpsrv: UDP_Server(): PR_RecvFrom(): failed with error: %ld\n",
                    PR_GetError() );
            PR_Close( svrSock );
            return;
        }
        ListNetAddr( "UDP_Server after RecvFrom", &netaddr );
        
        srvBytesRead += rv;
        
        if ( svrBuf[0] == 'E' )
        {
            DPRINTF("udpsrv: UDP_Server(): EOF on input detected\n" );
            endOfInput = PR_TRUE;
        }
            
        /* --- Send the socket --- */
        DPRINTF("udpsrv: UDP_Server(): SendTo(): socket\n" );
        rv = PR_SendTo( svrSock, svrBuf, rv, 0, &netaddr, PR_INTERVAL_NO_TIMEOUT );
        if ( rv == -1 )
        {
            passed = PR_FALSE;
            if (debug_mode)
                PR_fprintf(output,
                    "udpsrv: UDP_Server(): PR_SendTo(): failed with error: %ld\n",
                    PR_GetError() );
            PR_Close( svrSock );
            return;
        }
        ListNetAddr( "UDP_Server after SendTo", &netaddr );
    }
    
    /* --- Close the socket --- */
    DPRINTF("udpsrv: UDP_Server(): Closing socket\n" );
    rv = PR_Close( svrSock );
    if ( rv != PR_SUCCESS )
    {
        passed = PR_FALSE;
        if (debug_mode)
            PR_fprintf(output,
                "udpsrv: UDP_Server(): PR_Close(): failed to close socket\n" );
        return;
    }
    
    DPRINTF("udpsrv: UDP_Server(): Normal end\n" );
} /* --- end UDP_Server() --- */


static char         cltBuf[UDP_BUF_SIZE];
static char         cltBufin[UDP_BUF_SIZE];
/********************************************************************
** UDP_Client() -- Test a UDP client application
**
** Description:
**
** Arguments:
**
**
** Returns:
** 0 -- Successful execution
** 1 -- Test failed.
**
** Notes:
**
**
********************************************************************
*/
static void PR_CALLBACK UDP_Client( void *arg )
{
    PRFileDesc   *cltSock;
    PRInt32      rv;
    PRBool       bound = PR_FALSE;
    PRNetAddr    netaddr;
    PRNetAddr    netaddrx;
    PRBool       endOfInput = PR_FALSE;
    PRInt32      numBytes = UDP_DGRAM_SIZE;
    PRInt32      writeThisMany = UDP_AMOUNT_TO_WRITE;
    int          i;
    
    
    DPRINTF("udpsrv: UDP_Client(): starting\n" );
    
    /* --- Create the socket --- */
    cltSock = PR_NewUDPSocket();
    if ( cltSock == NULL )
    {
        passed = PR_FALSE;
        if (debug_mode)
            PR_fprintf(output,
                "udpsrv: UDP_Client(): PR_NewUDPSocket() returned NULL\n" );
        return;
    }
    
    /* --- Initialize the sockaddr_in structure --- */
    memset( &netaddr, 0, sizeof( netaddr )); 
    netaddr.inet.family = PR_AF_INET;
    netaddr.inet.ip     = PR_htonl( MY_INADDR );
    netaddr.inet.port   = PR_htons( UDP_CLIENT_PORT );
    
    /* --- Initialize the write buffer --- */    
    for ( i = 0; i < UDP_BUF_SIZE ; i++ )
        cltBuf[i] = i;
    
    /* --- Bind the socket --- */
    while ( !bound )
    {
        DPRINTF("udpsrv: UDP_Client(): Binding socket\n" );
        rv = PR_Bind( cltSock, &netaddr );
        if ( rv < 0 )
        {
            if ( PR_GetError() == PR_ADDRESS_IN_USE_ERROR )
            {
                if (debug_mode)
                    PR_fprintf(output,
                        "udpsrv: UDP_Client(): PR_Bind(): reports: PR_ADDRESS_IN_USE_ERROR\n");
                PR_Sleep( PR_MillisecondsToInterval( 2000 ));
                continue;
            }
            else
            {
                passed = PR_FALSE;
                if (debug_mode)
                    PR_fprintf(output,
                        "udpsrv: UDP_Client(): PR_Bind(): failed: %ld with error: %ld\n",
                        rv, PR_GetError() );
                PR_Close( cltSock );
                return;
            }
        }
        else
            bound = PR_TRUE;
    }
    ListNetAddr( "UDP_Client after Bind", &netaddr );
    
    /* --- Initialize the sockaddr_in structure --- */
    memset( &netaddr, 0, sizeof( netaddr )); 
    netaddr.inet.family = PR_AF_INET;
    netaddr.inet.ip     = PR_htonl( PEER_INADDR );
    netaddr.inet.port   = PR_htons( UDP_SERVER_PORT );
    
    /* --- send and receive packets until no more data left */    
    while( !endOfInput )
    {
        /*
        ** Signal EOF in the data stream on the last packet
        */        
        if ( writeThisMany <= UDP_DGRAM_SIZE )
        {
            DPRINTF("udpsrv: UDP_Client(): Send EOF packet\n" );
            cltBuf[0] = 'E';
            endOfInput = PR_TRUE;
        }
        
        /* --- SendTo the socket --- */
        if ( writeThisMany > UDP_DGRAM_SIZE )
            numBytes = UDP_DGRAM_SIZE;
        else
            numBytes = writeThisMany;
        writeThisMany -= numBytes;
        {
            char   mbuf[256];
            sprintf( mbuf, "udpsrv: UDP_Client(): write_this_many: %d, numbytes: %d\n", 
                writeThisMany, numBytes );
            DPRINTF( mbuf );
        }
        
        DPRINTF("udpsrv: UDP_Client(): SendTo(): socket\n" );
        rv = PR_SendTo( cltSock, cltBuf, numBytes, 0, &netaddr, UDP_TIMEOUT );
        if ( rv == -1 )
        {
            passed = PR_FALSE;
            if (debug_mode)
                PR_fprintf(output,
                    "udpsrv: UDP_Client(): PR_SendTo(): failed with error: %ld\n",
                        PR_GetError() );
            PR_Close( cltSock );
            return;
        }
        ListNetAddr( "UDP_Client after SendTo", &netaddr );

        /* --- RecvFrom the socket --- */
        memset( cltBufin, 0, UDP_BUF_SIZE );
        DPRINTF("udpsrv: UDP_Client(): RecvFrom(): socket\n" );
        rv = PR_RecvFrom( cltSock, cltBufin, numBytes, 0, &netaddrx, UDP_TIMEOUT );
        if ( rv == -1 )
        {
            passed = PR_FALSE;
            if (debug_mode) PR_fprintf(output,
                "udpsrv: UDP_Client(): PR_RecvFrom(): failed with error: %ld\n",
                   PR_GetError() );
            PR_Close( cltSock );
            return;
        }
        ListNetAddr( "UDP_Client after RecvFrom()", &netaddr );
        cltBytesRead += rv;
        
        /* --- verify buffer --- */
        for ( i = 0; i < rv ; i++ )
        {
            if ( cltBufin[i] != i )
            {
                /* --- special case, end of input --- */
                if ( endOfInput && i == 0 && cltBufin[0] == 'E' )
                    continue;
                passed = PR_FALSE;
                if (debug_mode) PR_fprintf(output,
                    "udpsrv: UDP_Client(): return data mismatch\n" );
                PR_Close( cltSock );
                return;
            }
        }
        if (debug_mode) PR_fprintf(output, ".");
    }
    
    /* --- Close the socket --- */
    DPRINTF("udpsrv: UDP_Server(): Closing socket\n" );
    rv = PR_Close( cltSock );
    if ( rv != PR_SUCCESS )
    {
        passed = PR_FALSE;
        if (debug_mode) PR_fprintf(output,
            "udpsrv: UDP_Client(): PR_Close(): failed to close socket\n" );
        return;
    }
    DPRINTF("udpsrv: UDP_Client(): ending\n" );
} /* --- end UDP_Client() --- */

/********************************************************************
** main() -- udpsrv
**
** arguments:
**
** Returns:
** 0 -- Successful execution
** 1 -- Test failed.
**
** Description:
**
** Standard test case setup.
**
** Calls the function UDP_Server()
**
********************************************************************
*/

int main(int argc, char **argv)
{
    PRThread    *srv, *clt;
/* The command line argument: -d is used to determine if the test is being run
	in debug mode. The regress tool requires only one line output:PASS or FAIL.
	All of the printfs associated with this test has been handled with a if (debug_mode)
	test.
	Usage: test_name -d -v
	*/
	PLOptStatus os;
	PLOptState *opt = PL_CreateOptState(argc, argv, "dv");
	while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
		if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'd':  /* debug mode */
			debug_mode = 1;
            break;
        case 'v':  /* verbose mode */
			_debug_on = 1;
            break;
         default:
            break;
        }
    }
	PL_DestroyOptState(opt);
		
    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    PR_STDIO_INIT();
    output = PR_STDERR;

#ifdef XP_MAC
    SetupMacPrintfLog("udpsrv.log");
#endif

    PR_SetConcurrency(4);
    
    /*
    ** Create the Server thread
    */    
    DPRINTF( "udpsrv: Creating Server Thread\n" );
    srv =  PR_CreateThread( PR_USER_THREAD,
            UDP_Server,
            (void *) 0,
            PR_PRIORITY_LOW,
            PR_LOCAL_THREAD,
            PR_JOINABLE_THREAD,
            0 );
    if ( srv == NULL )
    {
        if (debug_mode) PR_fprintf(output, "udpsrv: Cannot create server thread\n" );
        passed = PR_FALSE;
    }
    
    /*
    ** Give the Server time to Start
    */    
    DPRINTF( "udpsrv: Pausing to allow Server to start\n" );
    PR_Sleep( PR_MillisecondsToInterval(200) );
    
    /*
    ** Create the Client thread
    */    
    DPRINTF( "udpsrv: Creating Client Thread\n" );
    clt = PR_CreateThread( PR_USER_THREAD,
            UDP_Client,
            (void *) 0,
            PR_PRIORITY_LOW,
            PR_LOCAL_THREAD,
            PR_JOINABLE_THREAD,
            0 );
    if ( clt == NULL )
    {
        if (debug_mode) PR_fprintf(output, "udpsrv: Cannot create server thread\n" );
        passed = PR_FALSE;
    }
    
    /*
    **
    */
    DPRINTF("udpsrv: Waiting to join Server & Client Threads\n" );
    PR_JoinThread( srv );
    PR_JoinThread( clt );    
    
    /*
    ** Evaluate test results
    */
    if (debug_mode) PR_fprintf(output, "\n\nudpsrv: main(): cltBytesRead(%ld), \
		srvBytesRead(%ld), expected(%ld)\n",
         cltBytesRead, srvBytesRead, UDP_AMOUNT_TO_WRITE );
    if ( cltBytesRead != srvBytesRead || cltBytesRead != UDP_AMOUNT_TO_WRITE )
    {
        passed = PR_FALSE;
    }
    PR_Cleanup();
    if ( passed )
        return 0;
    else
		return 1;
} /* --- end main() --- */
