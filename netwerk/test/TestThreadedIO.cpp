/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Bill Law          <law@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include <stdio.h>
#include "nsCOMPtr.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
//#include "prthread.h"

// This test attempts to load a URL on a separate thread.  It is currently
// designed simply to expose the problems inherent in such an ambitous task
// (i.e., it don't work).

// Utility functions...

// Create event queue for current thread.
static nsCOMPtr<nsIEventQueue>
createEventQueue() {
    nsCOMPtr<nsIEventQueue> result;
    // Get event queue service.
    nsresult rv = NS_OK;
    nsCOMPtr<nsIEventQueueService> eqs = 
             do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID, &rv);
    if ( NS_SUCCEEDED( rv ) ) {
        // Have service create us an event queue.
        rv = eqs->CreateThreadEventQueue();
        if ( NS_SUCCEEDED( rv ) ) {
            // Get the event queue it created.  BTW, why the heck doesn't
            // CreateThreadEventQueue just return it?
            eqs->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(result));
        } else {
            printf( "%s %d: nsIEventQueueService::CreateThreadEventQueue failed, rv=0x%08X\n",
                    (char*)__FILE__, (int)__LINE__, (int)rv );
        }
    } else {
        printf( "%s %d: NS_WITH_SERVICE(nsIEventQueueService) failed, rv=0x%08X\n",
                (char*)__FILE__, (int)__LINE__, (int)rv );
    }
    return result;
}

// Create channel for requested URL.
static nsCOMPtr<nsIChannel>
createChannel( const char *url ) {
    nsCOMPtr<nsIChannel> result;

    nsCOMPtr<nsIURI> uri;
    printf( "Calling NS_NewURI for %s...\n", url );
    nsresult rv = NS_NewURI( getter_AddRefs( uri ), url );

    if ( NS_SUCCEEDED( rv ) ) {
        printf( "...NS_NewURI completed OK\n" );

        // Allocate a new input channel on this thread.
        printf( "Calling NS_OpenURI...\n" );
        nsresult rv = NS_OpenURI( getter_AddRefs( result ), uri, 0 );

        if ( NS_SUCCEEDED( rv ) ) {
            printf( "...NS_OpenURI completed OK\n" );
        } else {
            printf( "%s %d: NS_OpenURI failed, rv=0x%08X\n",
                    (char*)__FILE__, (int)__LINE__, (int)rv );
        }
    } else {
        printf( "%s %d: NS_NewURI failed, rv=0x%08X\n",
                (char*)__FILE__, (int)__LINE__, (int)rv );
    }
    return result;
}

// Test listener.  It basically dumps incoming data to console.
class TestListener : public nsIStreamListener {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSISTREAMOBSERVER

    TestListener();
    ~TestListener();
    static void IOThread( void *p );

private:
    PRBool mDone;
    int    mThreadNo;
    FILE  *mFile;
    static int threadCount;
}; // class TestListener

int TestListener::threadCount = 0;

TestListener::TestListener()
    : mDone( PR_FALSE ), mThreadNo( ++threadCount ) {
    printf( "TestListener ctor called on thread %d\n", mThreadNo );
    NS_INIT_REFCNT();
}

TestListener::~TestListener() {
    printf( "TestListener dtor called on thread %d\n", mThreadNo );
}

NS_IMPL_ISUPPORTS2( TestListener, nsIStreamListener, nsIRequestObserver );

NS_IMETHODIMP
TestListener::OnStartRequest( nsIChannel *aChannel, nsISupports *aContext ) {
    nsresult rv = NS_OK;

    printf( "TestListener::OnStartRequest called on thread %d\n", mThreadNo );

    // Open output file.
    char fileName[32];
    sprintf( fileName, "%s%d", "thread", mThreadNo );
    mFile = fopen( fileName, "wb" );
    setbuf( mFile, 0 );

    return rv;
}

NS_IMETHODIMP
TestListener::OnStopRequest( nsIChannel *aChannel,
                             nsISupports *aContext,
                             nsresult aStatus,
                             const PRUnichar *aMsg ) {
    nsresult rv = NS_OK;

    printf( "TestListener::OnStopRequest called on thread %d\n", mThreadNo );

    fclose( mFile );
    mDone = PR_TRUE;

    return rv;
}

NS_IMETHODIMP
TestListener::OnDataAvailable( nsIChannel *aChannel,
                               nsISupports *aContext,
                               nsIInputStream *aStream,
                               PRUint32 offset,
                               PRUint32 aLength ) {
    nsresult rv = NS_OK;

    printf( "TestListener::OnDataAvailable called on thread %d\n", mThreadNo );

    // Write the data to the console.
    // Read a buffer full till aLength bytes have been processed.
    char buffer[ 8192 ];
    unsigned long bytesRemaining = aLength;
    while ( bytesRemaining ) {
        unsigned int bytesRead;
        // Read a buffer full or the number remaining (whichever is smaller).
        rv = aStream->Read( buffer,
                            PR_MIN( sizeof( buffer ), bytesRemaining ),
                            &bytesRead );
        if ( NS_SUCCEEDED( rv ) ) {
            // Write the bytes just read to the output file.
            fwrite( buffer, 1, bytesRead, mFile );
            bytesRemaining -= bytesRead;
        } else {
            printf( "%s %d: Read error, rv=0x%08X\n",
                    (char*)__FILE__, (int)__LINE__, (int)rv );
            break;
        }
    }
    printf( "\n" );

    return rv;
}

// IOThread: this function creates a new TestListener object (on the new
// thread), opens a channel, and does AsyncRead to it.
void
TestListener::IOThread( void *p ) {
    printf( "I/O thread (0x%08X) started...\n", (int)(void*)PR_GetCurrentThread() );

    // Argument is pointer to the nsIEventQueue for the main thread.
    nsIEventQueue *mainThreadQ = NS_STATIC_CAST( nsIEventQueue*, p );

    // Create channel for random web page.
    nsCOMPtr<nsIChannel> channel = createChannel( (const char*)p );

    if ( channel ) {
        // Create event queue.
        nsCOMPtr<nsIEventQueue> ioEventQ = createEventQueue();

        if ( ioEventQ ) {
            // Create test listener.
            TestListener *testListener = new TestListener();
            testListener->AddRef();

            // Read the channel.
            printf( "Doing AsyncRead...\n" );
            nsresult rv = channel->AsyncRead( testListener, 0 );

            if ( NS_SUCCEEDED( rv ) ) {
                printf( "...AsyncRead completed OK\n" );

                // Process events till testListener says stop.
                printf( "Start event loop on io thread %d...\n", testListener->mThreadNo );
                while ( !testListener->mDone ) {
                    PLEvent *event;
                    ioEventQ->GetEvent( &event );
                    ioEventQ->HandleEvent( event );
                }
                printf( "...io thread %d event loop exiting\n", testListener->mThreadNo );
            } else {
                printf( "%s %d: AsyncRead failed on thread %d, rv=0x%08X\n",
                        (char*)__FILE__, (int)__LINE__, testListener->mThreadNo, (int)rv );
            }

            // Release the test listener.
            testListener->Release();
        }
    }

    printf( "...I/O thread terminating\n" );
}

static const int maxThreads = 5;

int
main( int argc, char* argv[] ) {
    setbuf( stdout, 0 );
    if ( argc < 2 || argc > maxThreads + 1 ) {
        printf( "usage: testThreadedIO url1 <url2>...\n"
                "where <url#> is a location to be loaded on a separate thread\n"
                "limit is %d urls/threads", maxThreads );
        return -1;
    }

    nsresult rv= (nsresult)-1;

    printf( "Test starting...\n" );

    // Initialize XPCOM.
    printf( "Initializing XPCOM...\n" );
    rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
    if ( NS_SUCCEEDED( rv ) ) {
        printf( "...XPCOM initialized OK\n" );

        // Create the Event Queue for this thread...
        printf( "Creating event queue for main thread (0x%08X)...\n",
                (int)(void*)PR_GetCurrentThread() );
        nsCOMPtr<nsIEventQueue> mainThreadQ = createEventQueue();

        if ( mainThreadQ ) {
            printf( "...main thread's event queue created OK\n" );

            // Spawn threads to do I/O.
            int goodThreads = 0;
            PRThread *thread[ maxThreads ];
            for ( int threadNo = 1; threadNo < argc; threadNo++ ) {
                printf( "Creating I/O thread %d to load %s...\n", threadNo, argv[threadNo] );
                PRThread *ioThread = PR_CreateThread( PR_USER_THREAD,
                                                      TestListener::IOThread,
                                                      argv[threadNo],
                                                      PR_PRIORITY_NORMAL,
                                                      PR_LOCAL_THREAD,
                                                      PR_JOINABLE_THREAD,
                                                      0 );
                if ( ioThread ) {
                    thread[ goodThreads++ ] = ioThread;
                    printf( "...I/O thread %d (0x%08X) created OK\n",
                            threadNo, (int)(void*)ioThread );
                } else {
                    printf( "%s %d: PR_CreateThread for thread %d failed\n",
                            (char*)__FILE__, (int)__LINE__, threadNo );
                }
            }

            // Wait for all the threads to terminate.
            for ( int joinThread = 0; joinThread < goodThreads; joinThread++ ) {
                printf( "Waiting for thread %d to terminate...\n", joinThread+1 );
                PR_JoinThread( thread[ joinThread ] );
            }
        }
        // Shut down XPCOM.
        printf( "Shutting down XPCOM...\n" );
        NS_ShutdownXPCOM( 0 );
        printf( "...XPCOM shutdown complete\n" );
    } else {
        printf( "%s %d: NS_InitXPCOM failed, rv=0x%08X\n",
                (char*)__FILE__, (int)__LINE__, (int)rv );
        return rv;
    }
    
    // Exit.
    printf( "...test complete, rv=0x%08X\n", (int)rv );
    return rv;
}
