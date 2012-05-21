/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
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
            eqs->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(result));
    } else {
        printf( "%s %d: NS_WITH_SERVICE(nsIEventQueueService) failed, rv=0x%08X\n",
                (char*)__FILE__, (int)__LINE__, (int)rv );
    }
    return result;
}

// Create channel for requested URL.
static nsCOMPtr<nsIChannel>
createChannel( const char *url ) {
    nsCOMPtr<nsIInputStream> result;

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
    bool mDone;
    int    mThreadNo;
    FILE  *mFile;
    static int threadCount;
}; // class TestListener

int TestListener::threadCount = 0;

TestListener::TestListener()
    : mDone( false ), mThreadNo( ++threadCount ) {
    printf( "TestListener ctor called on thread %d\n", mThreadNo );
}

TestListener::~TestListener() {
    printf( "TestListener dtor called on thread %d\n", mThreadNo );
}

NS_IMPL_ISUPPORTS2( TestListener, nsIStreamListener, nsIRequestObserver )

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
    mDone = true;

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
                            NS_MIN( sizeof( buffer ), bytesRemaining ),
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
    nsIEventQueue *mainThreadQ = static_cast<nsIEventQueue*>(p);

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
    if ( NS_FAILED( rv ) ) {
        printf( "%s %d: NS_InitXPCOM failed, rv=0x%08X\n",
                (char*)__FILE__, (int)__LINE__, (int)rv );
        return rv;
    }
    printf( "...XPCOM initialized OK\n" );
    // Create the Event Queue for this thread...
    printf( "Creating event queue for main thread (0x%08X)...\n",
            (int)(void*)PR_GetCurrentThread() );
    {
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
    } // this scopes the nsCOMPtrs
    // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
    // Shut down XPCOM.
    printf( "Shutting down XPCOM...\n" );
    NS_ShutdownXPCOM( 0 );
    printf( "...XPCOM shutdown complete\n" );

    // Exit.
    printf( "...test complete, rv=0x%08X\n", (int)rv );
    return rv;
}
