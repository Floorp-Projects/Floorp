/*
 *	 The contents of this file are subject to the Netscape Public
 *	 License Version 1.1 (the "License"); you may not use this file
 *	 except in compliance with the License. You may obtain a copy of
 *	 the License at http://www.mozilla.org/NPL/
 *	
 *	 Software distributed under the License is distributed on an "AS
 *	 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 *	 implied. See the License for the specific language governing
 *	 rights and limitations under the License.
 *	
 *	 The Original Code is the Netscape Messaging Access SDK Version 3.5 code, 
 *	released on or about June 15, 1998.  *	
 *	 The Initial Developer of the Original Code is Netscape Communications 
 *	 Corporation. Portions created by Netscape are
 *	 Copyright (C) 1998 Netscape Communications Corporation. All
 *	 Rights Reserved.
 *	
 *	 Contributor(s): ______________________________________. 
*/
  
/* 
 * Copyright (c) 1997 and 1998 Netscape Communications Corporation
 * (http://home.netscape.com/misc/trademarks.html) 
 */

/*
 *This test program demonstrates multi-threaded usage of the SMTP module in the 
 *message SDK.  It starts a thread to send a message and uses another thread to
 *cancel it.
 *USAGE:smtptest.exe server domain sender recipient data
 *NOTE: data must be 1 word
 *@author derekt@netscape.com
 *@version 1.0
 */

#include "smtp.h"
#include "nsStream.h"
#include "testsink.h"
#include <time.h>
#ifdef WIN32
#include <windows.h>
#include <process.h>    /* _beginthread, _endthread */
#include <stddef.h>
#include <stdlib.h>
#include <conio.h>
#else
#include <pthread.h>
#endif

/* Global Server Error Flag */
boolean g_serverError = FALSE;

/* Global Data */
smtpClient_t * g_pClient;
char * g_szServer;
char * g_szDomain;
char * g_szSender;
char * g_szRecipient;
char * g_szData;

#ifdef WIN32
    HANDLE g_handle;
    HANDLE g_threadHandle;
#else
    pthread_t g_threadID1;
    pthread_t g_threadID2;
    pthread_mutex_t g_mp;
#endif
 
/*Function prototype for settting sink pointers*/
void setSink( smtpSink_t * pSink );
void SMTP_lock();
void SMTP_unlock();

/*Perform a disconnect from the server*/
void SMTP_CancelThread( void * dummy )
{
    int l_nReturn = smtp_disconnect( g_pClient );

    printf( "Inside CancelThread\n" );

    if ( l_nReturn != NSMAIL_OK )
    {
        printf( "smtptest.c::SMTP_CancelThread" );
        return;
    }

}

/*Attempt to send a message*/
void SMTP_SendThread( void * dummy )
{
    int l_nReturn;
    
    SMTP_lock();

    /*Connect to the SMTP server.*/
    l_nReturn = smtp_connect( g_pClient, g_szServer, 25 );

    if ( l_nReturn != NSMAIL_OK )
    {
        printf( "smtptest.c::SMTP_SendThread" );
        SMTP_unlock();
        return;
    }

    l_nReturn = smtp_processResponses( g_pClient );

    if ( l_nReturn != NSMAIL_OK || g_serverError == TRUE )
    {
        printf( "smtptest.c::SMTP_SendThread" );
        SMTP_unlock();
        return;
    }

    /*Send the EHLO command passing in the domain name.*/
    l_nReturn = smtp_ehlo( g_pClient, g_szDomain );

    if ( l_nReturn != NSMAIL_OK )
    {
        printf( "smtptest.c::SMTP_SendThread" );
        SMTP_unlock();
        return;
    }

    l_nReturn = smtp_processResponses( g_pClient );

    if ( l_nReturn != NSMAIL_OK || g_serverError == TRUE )
    {
        printf( "smtptest.c::SMTP_SendThread" );
        SMTP_unlock();
        return;
    }

    /*Send the MAIL FROM command.*/
    l_nReturn = smtp_mailFrom( g_pClient, g_szSender, NULL );

    if ( l_nReturn != NSMAIL_OK )
    {
        printf( "smtptest.c::SMTP_SendThread" );
        SMTP_unlock();
        return;
    }

    l_nReturn = smtp_processResponses( g_pClient );

    if ( l_nReturn != NSMAIL_OK || g_serverError == TRUE )
    {
        printf( "smtptest.c::SMTP_SendThread" );
        SMTP_unlock();
        return;
    }

    /*Send the RCPT TO command.*/
    l_nReturn = smtp_rcptTo( g_pClient, g_szRecipient, NULL );

    if ( l_nReturn != NSMAIL_OK )
    {
        printf( "smtptest.c::SMTP_SendThread" );
        SMTP_unlock();
        return;
    }

    l_nReturn = smtp_processResponses( g_pClient );

    if ( l_nReturn != NSMAIL_OK || g_serverError == TRUE )
    {
        printf( "smtptest.c::SMTP_SendThread" );
        SMTP_unlock();
        return;
    }

    /*Send the DATA command.*/
    l_nReturn = smtp_data( g_pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        printf( "smtptest.c::SMTP_SendThread" );
        SMTP_unlock();
        return;
    }

    l_nReturn = smtp_processResponses( g_pClient );

    if ( l_nReturn != NSMAIL_OK || g_serverError == TRUE )
    {
        printf( "smtptest.c::SMTP_SendThread" );
        SMTP_unlock();
        return;
    }

    /* Send the message.*/
    l_nReturn = smtp_send( g_pClient, g_szData );

    if ( l_nReturn != NSMAIL_OK )
    {
        printf( "smtptest.c::SMTP_SendThread" );
        SMTP_unlock();
        return;
    }

    l_nReturn = smtp_processResponses( g_pClient );

    if ( l_nReturn != NSMAIL_OK || g_serverError == TRUE )
    {
        printf( "smtptest.c::SMTP_SendThread" );
        SMTP_unlock();
        return;
    }

    l_nReturn = smtp_quit( g_pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        printf( "smtptest.c::SMTP_SendThread" );
        SMTP_unlock();
        return;
    }

    l_nReturn = smtp_processResponses( g_pClient );

    if ( l_nReturn != NSMAIL_OK || g_serverError == TRUE )
    {
        printf( "smtptest.c::SMTP_SendThread" );
        SMTP_unlock();
        return;
    }

    SMTP_unlock();
}

int main( int argc, char *argv[] )
{
    int l_nReturn;
    unsigned long l_threadHandle1;
    unsigned long l_threadHandle2;
    smtpSink_t * pSink = NULL;
    g_pClient = NULL;

    /*Initialize the global parameters*/
    g_szServer = argv[1];
    g_szDomain = argv[2];
    g_szSender = argv[3];
    g_szRecipient = argv[4];
    g_szData = argv[5];

    /*Initialize the response sink.*/
    l_nReturn = smtpSink_initialize( &pSink );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Set the function pointers on the response sink.*/
    setSink( pSink );

    /*Initialize the client passing in the response sink.*/
    l_nReturn = smtp_initialize( &g_pClient, pSink );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

/*Setup the mutex and the threads*/
#ifdef WIN32
    g_handle = CreateMutex( NULL, FALSE, NULL );

    l_threadHandle1 = _beginthread( SMTP_SendThread, 0, NULL );
    l_threadHandle2 = _beginthread( SMTP_CancelThread, 0, NULL );

    WaitForSingleObject( (HANDLE)l_threadHandle1, INFINITE );
    WaitForSingleObject( (HANDLE)l_threadHandle2, INFINITE );
#else
    pthread_mutex_init( &g_mp, NULL );
    pthread_create( &g_threadID1, NULL, SMTP_SendThread, NULL );
    pthread_create( &g_threadID2, NULL, SMTP_CancelThread, NULL );

    pthread_join( g_threadID1, NULL );
    pthread_join( g_threadID2, NULL );

    pthread_mutex_destroy( &g_mp );
#endif

    /*Free the client structure.*/
    smtp_free( &g_pClient );
    /*Free the sink structure.*/
    smtpSink_free( &pSink );

    return 0;
}

/*Function to set the sink pointers.*/
void setSink( smtpSink_t * pSink )
{
    pSink->bdat = SMTPTest_bdat;
    pSink->connect = SMTPTest_connect;
    pSink->data = SMTPTest_data;
    pSink->ehlo = SMTPTest_ehlo;
    pSink->ehloComplete = SMTPTest_ehloComplete;
    pSink->expand = SMTPTest_expand;
    pSink->expandComplete = SMTPTest_expandComplete;
    pSink->help = SMTPTest_help;
    pSink->helpComplete = SMTPTest_helpComplete;
    pSink->mailFrom = SMTPTest_mailFrom;
    pSink->noop = SMTPTest_noop;
    pSink->quit = SMTPTest_quit;
    pSink->rcptTo = SMTPTest_rcptTo;
    pSink->reset = SMTPTest_reset;
    pSink->send = SMTPTest_send;
    pSink->sendCommand = SMTPTest_sendCommand;
    pSink->sendCommandComplete = SMTPTest_sendCommandComplete;
    pSink->verify = SMTPTest_verify;
}

/*Function to lock the mutex*/
void SMTP_lock()
{
#ifdef WIN32
    if ( WaitForSingleObject( g_handle, INFINITE ) == WAIT_FAILED )
    {
        printf( "smtptest.c::SMTP_lock" );
        return;
    }
#else
    if ( pthread_mutex_lock( &g_mp ) != 0 )
    {
        printf( "smtptest.c::SMTP_lock" );
        return;
    }
#endif
}

/*Function to unlock the mutex*/
void SMTP_unlock()
{
#ifdef WIN32
    if ( ReleaseMutex( g_handle ) == 0 )
    {
        printf( "smtptest.c::SMTP_unlock" );
        return;
    }
#else
    if ( pthread_mutex_unlock( &g_mp ) != 0 )
    {
        printf( "smtptest.c::SMTP_unlock" );
        return;
    }
#endif
}
