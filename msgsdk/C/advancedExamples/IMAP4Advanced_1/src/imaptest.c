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
 *This test program demonstrates multi-threaded usage of the IMAP4 module in the 
 *message SDK.  It performs a fetch and a search with two threads and demonstrates
 *the use of a mutex.
 *USAGE:imaptest.exe server user password mailbox
 *NOTE:By default this test program attempts to fetch message 1
 *@author derekt@netscape.com
 *@version 1.0
 */

#include "imap4.h"
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
imap4Client_t * g_pClient;
#ifdef WIN32
    HANDLE g_handle;
#else
    pthread_t g_threadID1;
    pthread_t g_threadID2;
    pthread_mutex_t g_mp;
#endif
 
/*Function prototype for settting sink pointers*/
void setSink( imap4Sink_t * pSink );
void IMAP4_lock();
void IMAP4_unlock();

/*A function to perform a search*/
void IMAP4_SearchThread( void * dummy )
{
    char * l_szTagID;
    int l_nReturn;

    printf( "Inside IMAP4_SearchThread\n" );

    IMAP4_lock();

    /*Select the INBOX.*/
	l_nReturn = imap4_search( g_pClient, "BODY \"World\"", &l_szTagID );

    if ( l_nReturn != NSMAIL_OK )
    {
        printf( "imaptest.c::IMAP4_SearchThread" );
        IMAP4_unlock();
        return;
    }

    imap4Tag_free(&l_szTagID);

    l_nReturn = imap4_processResponses( g_pClient );

    if ( l_nReturn != NSMAIL_OK || g_serverError == TRUE )
    {
        printf( "imaptest.c::IMAP4_SearchThread" );
        IMAP4_unlock();
        return;
    }

    IMAP4_unlock();
}

/*A function to perform a fetch*/
void IMAP4_FetchThread( void * dummy )
{
    char * l_szTagID;
    int l_nReturn;

    printf( "Inside IMAP4_FetchThread\n" );

    IMAP4_lock();

    /*Fetch the message.*/
	l_nReturn = imap4_fetch( g_pClient, "1", "(BODY[])", &l_szTagID );

    if ( l_nReturn != NSMAIL_OK )
    {
        printf( "imaptest.c::IMAP4_SearchThread" );
        IMAP4_unlock();
        return;
    }

    imap4Tag_free(&l_szTagID);

    l_nReturn = imap4_processResponses( g_pClient );

    if ( l_nReturn != NSMAIL_OK || g_serverError == TRUE )
    {
        printf( "imaptest.c::IMAP4_SearchThread" );
        IMAP4_unlock();
        return;
    }

    IMAP4_unlock();
}

int main( int argc, char *argv[ ] )
{
    int l_nReturn;
    char * l_szTagID;
    unsigned long l_threadHandle1;
    unsigned long l_threadHandle2;
    imap4Sink_t * pSink = NULL;
    g_pClient = NULL;

    /*Initialize the response sink.*/
    l_nReturn = imap4Sink_initialize( &pSink );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    setSink( pSink );

	l_nReturn = imap4_initialize( &g_pClient, pSink );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Connect to the server.*/
	l_nReturn = imap4_connect( g_pClient, argv[1], 143, &l_szTagID );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    imap4Tag_free(&l_szTagID);

    l_nReturn = imap4_processResponses( g_pClient );

    if ( l_nReturn != NSMAIL_OK || g_serverError == TRUE )
    {
        return l_nReturn;
    }

    /*Login.*/
	l_nReturn = imap4_login( g_pClient, argv[2], argv[3], &l_szTagID );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    imap4Tag_free(&l_szTagID);

    l_nReturn = imap4_processResponses( g_pClient );

    if ( l_nReturn != NSMAIL_OK || g_serverError == TRUE )
    {
        return l_nReturn;
    }

    /*Select the mailbox.*/
	l_nReturn = imap4_select( g_pClient, argv[4], &l_szTagID );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    imap4Tag_free(&l_szTagID);

    l_nReturn = imap4_processResponses( g_pClient );

    if ( l_nReturn != NSMAIL_OK || g_serverError == TRUE )
    {
        return l_nReturn;
    }

/*Setup the mutex and the threads*/
#ifdef WIN32
    g_handle = CreateMutex( NULL, FALSE, NULL );

    l_threadHandle1 = _beginthread( IMAP4_FetchThread, 0, NULL );
    l_threadHandle2 = _beginthread( IMAP4_SearchThread, 0, NULL );

    WaitForSingleObject( (HANDLE)l_threadHandle1, INFINITE );
    WaitForSingleObject( (HANDLE)l_threadHandle2, INFINITE );
#else
    pthread_mutex_init( &g_mp, NULL );
    pthread_create( &g_threadID1, NULL, IMAP4_FetchThread, NULL );
    pthread_create( &g_threadID2, NULL, IMAP4_SearchThread, NULL );

    pthread_join( g_threadID1, NULL );
    pthread_join( g_threadID2, NULL );

    pthread_mutex_destroy( &g_mp );
#endif

    /*Logout.*/
	l_nReturn = imap4_logout( g_pClient, &l_szTagID );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    imap4Tag_free(&l_szTagID);

    l_nReturn = imap4_processResponses( g_pClient );

    if ( l_nReturn != NSMAIL_OK || g_serverError == TRUE )
    {
        return l_nReturn;
    }

    /*Free the client structure.*/
    imap4_free( &g_pClient );
    /*Free the sink structure.*/
    imap4Sink_free( &pSink );

    return 0;
}

/*Function to set the sink pointers.*/
void setSink( imap4Sink_t * pSink )
{
	pSink->taggedLine = taggedLine;
	pSink->error = error;
	pSink->ok = ok;
	pSink->rawResponse = rawResponse;
	pSink->fetchStart = fetchStart;
	pSink->fetchEnd = fetchEnd;
	pSink->fetchSize = fetchSize;
	pSink->fetchData = fetchData;
	pSink->fetchFlags = fetchFlags;
	pSink->fetchBodyStructure = fetchBodyStructure;
	pSink->fetchEnvelope = fetchEnvelope;
	pSink->fetchInternalDate = fetchInternalDate;
	pSink->fetchHeader = fetchHeader;
	pSink->fetchUid = fetchUid;
	pSink->lsub = lsub;
	pSink->list = list;
	pSink->searchStart = searchStart;
	pSink->search = search;
	pSink->searchEnd = searchEnd;
	pSink->statusMessages = statusMessages;
	pSink->statusRecent = statusRecent;
	pSink->statusUidnext = statusUidnext;
	pSink->statusUidvalidity = statusUidvalidity;
	pSink->statusUnseen = statusUnseen;
	pSink->capability = capability;
	pSink->exists = exists;
	pSink->expunge = expunge;
	pSink->recent = recent;
	pSink->flags = flags;
	pSink->bye = bye;
	pSink->nameSpaceStart = nameSpaceStart;
	pSink->nameSpacePersonal = nameSpacePersonal;
	pSink->nameSpaceOtherUsers = nameSpaceOtherUsers;
	pSink->nameSpaceShared = nameSpaceShared;
	pSink->nameSpaceEnd = nameSpaceEnd;
	pSink->aclStart = aclStart;
	pSink->aclIdentifierRight = aclIdentifierRight;
	pSink->aclEnd = aclEnd;
	pSink->listRightsStart = listRightsStart;
	pSink->listRightsRequiredRights = listRightsRequiredRights;
	pSink->listRightsOptionalRights = listRightsOptionalRights;
	pSink->listRightsEnd = listRightsEnd;
	pSink->myRights = myRights;
}

/*Function to lock the mutex*/
void IMAP4_lock()
{
#ifdef WIN32
    if ( WaitForSingleObject( g_handle, INFINITE ) == WAIT_FAILED )
    {
        printf( "imaptest.c::IMAP4_lock" );
        return;
    }
#else
    if ( pthread_mutex_lock( &g_mp ) != 0 )
    {
        printf( "imaptest.c::IMAP4_lock" );
        return;
    }
#endif
}

/*Function to unlock the mutex*/
void IMAP4_unlock()
{
#ifdef WIN32
    if ( ReleaseMutex( g_handle ) == 0 )
    {
        printf( "imaptest.c::IMAP4_unlock" );
        return;
    }
#else
    if ( pthread_mutex_unlock( &g_mp ) != 0 )
    {
        printf( "imaptest.c::IMAP4_unlock" );
        return;
    }
#endif
}
