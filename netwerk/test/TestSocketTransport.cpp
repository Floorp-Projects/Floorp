/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include <stdio.h>

#ifdef WIN32
//#define USE_TIMERS  // Only use nsITimer on Windows (for now...)
#include <windows.h>
#endif
#ifdef XP_OS2
#include <os2.h>
#endif

#include "nspr.h"
#ifdef XP_MAC
#include "pprio.h"	// PR_Init_Log
#endif

#include "nsISocketTransportService.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsIChannel.h"
#include "nsIStreamObserver.h"
#include "nsIStreamListener.h"
#include "nsIPipe.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIRunnable.h"
#include "nsIThread.h"
#include "nsITimer.h"
#include "nsIInterfaceRequestor.h"
#include "nsIProgressEventSink.h"
#include "nsCRT.h"

#if defined(XP_MAC)
#include "macstdlibextras.h"
  // Set up the toolbox and (if DEBUG) the console.  Do this in a static initializer,
  // to make it as unlikely as possible that somebody calls printf() before we get initialized.
static struct MacInitializer { MacInitializer() { InitializeMacToolbox(); InitializeSIOUX(true); } } gInitializer;
#endif // XP_MAC

// forward declarations...
class TestConnection;

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID,      NS_EVENTQUEUESERVICE_CID);

//static PRTime gElapsedTime;
static int gKeepRunning = 1;

#define NUM_TEST_THREADS 15
#define TRANSFER_AMOUNT 64

static TestConnection*  gConnections[NUM_TEST_THREADS];
static nsIThread*       gThreads[NUM_TEST_THREADS];
//static nsITimer*      gPeriodicTimer;
static PRBool           gVerbose = PR_TRUE;


void Pump_PLEvents(nsIEventQueueService * eventQService);
void Pump_PLEvents(nsIEventQueueService * eventQService)
{
	nsIEventQueue* eventQ = nsnull;
  eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &eventQ);

  while ( gKeepRunning ) {
#ifdef WIN32
    MSG msg;

    if (GetMessage(&msg, NULL, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      gKeepRunning = FALSE;
    }
#elif defined(XP_OS2)
    QMSG qmsg;

    if (WinGetMsg(0, &qmsg, 0, 0, 0))
      WinDispatchMsg(0, &qmsg);
    else
      gKeepRunning = FALSE;
#else
    nsresult rv;  
    PLEvent *gEvent;
    rv = eventQ->GetEvent(&gEvent);
    rv = eventQ->HandleEvent(gEvent);
#endif /* !WIN32 */
  }

}

// -----
//
// TestConnection class...
//
// -----

class TestConnection : public nsIRunnable, 
                       public nsIStreamListener,
                       public nsIInterfaceRequestor,
                       public nsIProgressEventSink
{
public:
  TestConnection(const char* aHostName, PRInt32 aPort,
                 PRBool aAsyncFlag);
  virtual ~TestConnection();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSISTREAMOBSERVER
  NS_DECL_NSISTREAMLISTENER

  // TestConnection methods...
  nsresult WriteBuffer(void);
  nsresult ReadBuffer(void);

  nsresult Process(void);

  nsresult Suspend(void);
  nsresult Resume(void);

  NS_IMETHOD GetInterface(const nsIID & uuid, void * *result) {
    if (uuid.Equals(NS_GET_IID(nsIProgressEventSink))) {
      *result = (nsIProgressEventSink*)this;
      NS_ADDREF_THIS();
      return NS_OK;
    }
    return NS_NOINTERFACE;
  }

  NS_IMETHOD OnProgress(nsIChannel *channel, nsISupports *ctxt, 
                        PRUint32 aProgress, PRUint32 aProgressMax) {
    putc('+', stderr);
    return NS_OK;
  }

  NS_IMETHOD OnStatus(nsIChannel *channel, nsISupports *ctxt, 
                      nsresult aStatus, const PRUnichar* aStatusArg) {
    putc('?', stderr);
    return NS_OK;
  }

protected:
  nsIOutputStream* mOut;
  nsIInputStream* mStream;

  nsIInputStream*  mInStream;
  nsIOutputStream* mOutStream;

  nsIChannel*   mTransport;

  PRBool  mIsAsync;
  PRInt32 mBufferLength;
  char    mBufferChar;

  PRInt32 mBytesRead;

};

////////////////////////////////////////////////////////////////////////////////

class TestConnectionOpenObserver : public nsIStreamObserver 
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMOBSERVER

  TestConnectionOpenObserver(TestConnection* test)
    : mTestConnection(test) { 
    NS_INIT_REFCNT();
  }
  virtual ~TestConnectionOpenObserver() {}

protected:
  TestConnection* mTestConnection;

};

NS_IMPL_ISUPPORTS1(TestConnectionOpenObserver, nsIStreamObserver);

NS_IMETHODIMP
TestConnectionOpenObserver::OnStartRequest(nsIChannel* channel, nsISupports* context)
{
  if (gVerbose)
    printf("\n+++ TestConnectionOpenObserver::OnStartRequest +++. Context = %p\n", context);

  return NS_OK;
}

NS_IMETHODIMP
TestConnectionOpenObserver::OnStopRequest(nsIChannel* channel, nsISupports* context,
                                          nsresult aStatus, const PRUnichar* aStatusArg)
{
  if (gVerbose || NS_FAILED(aStatus))
    printf("\n+++ TestConnectionOpenObserver::OnStopRequest (status = %x) +++."
           "\tContext = %p\n", 
           aStatus, context);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
TestConnection::OnStartRequest(nsIChannel* channel, nsISupports* context)
{
  if (gVerbose)
    printf("\n+++ TestConnection::OnStartRequest +++. Context = %p\n", context);
  return NS_OK;
}


NS_IMETHODIMP
TestConnection::OnDataAvailable(nsIChannel* channel, nsISupports* context,
                                nsIInputStream *aIStream, 
                                PRUint32 aSourceOffset,
                                PRUint32 aLength)
{
  nsresult rv;

  char buf[TRANSFER_AMOUNT];
  PRUint32 amt;

  if (gVerbose)
    printf("\n+++ TestConnection::OnDavaAvailable +++."
           "\tContext = %p length = %d\n", 
           context, aLength);

  while (aLength > 0) {
    PRInt32 cnt = PR_MIN(TRANSFER_AMOUNT, aLength);
    rv = aIStream->Read(buf, cnt, &amt);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Read failed");
    mBytesRead += amt;
    aLength -= amt;
    buf[amt] = '\0';
    if (gVerbose)
      puts(buf);
  }

  if (mBufferLength == mBytesRead) {
    mBytesRead = 0;
    WriteBuffer();
  }

  return NS_OK;
}


NS_IMETHODIMP
TestConnection::OnStopRequest(nsIChannel* channel, nsISupports* context,
                              nsresult aStatus, const PRUnichar* aStatusArg)
{
  if (gVerbose || NS_FAILED(aStatus))
    printf("\n+++ TestConnection::OnStopRequest (status = %x) +++."
           "\tContext = %p\n", 
           aStatus, context);
  return NS_OK;
}


TestConnection::TestConnection(const char* aHostName, PRInt32 aPort, 
                               PRBool aAsyncFlag)
{
  nsresult rv;

  NS_INIT_REFCNT();

  mIsAsync      = aAsyncFlag;

  mBufferLength = TRANSFER_AMOUNT;
  mBufferChar   = 'a';
  mBytesRead    = 0;

  mTransport = nsnull;
  mOut       = nsnull;
  mStream    = nsnull;

  mInStream  = nsnull;
  mOutStream = nsnull;

  // Create a socket transport...
  NS_WITH_SERVICE(nsISocketTransportService, sts, kSocketTransportServiceCID, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = sts->CreateTransport(aHostName, aPort, nsnull, -1, 0, 0, &mTransport);
    if (NS_SUCCEEDED(rv)) {
      // Set up the notification callbacks to provide a progress event sink.
      // That way we exercise the progress notification proxy code.
      rv = mTransport->SetNotificationCallbacks(this);
    }
  }


  if (NS_SUCCEEDED(rv)) {
    if (mIsAsync) {
      // Create a stream for the data being written to the server...
      if (NS_SUCCEEDED(rv)) {
        rv = NS_NewPipe(&mStream, &mOut, 1024, 4096);
      }
    } 
    // Synchronous transport...
    else {
      rv = mTransport->OpenInputStream(&mInStream);
      rv = mTransport->OpenOutputStream(&mOutStream);
    }
  }
}


TestConnection::~TestConnection()
{
  NS_IF_RELEASE(mTransport);
  // Async resources...
  NS_IF_RELEASE(mStream);
  NS_IF_RELEASE(mOut);

  // Sync resources...
  NS_IF_RELEASE(mInStream);
  NS_IF_RELEASE(mOutStream);
}

NS_IMPL_THREADSAFE_ISUPPORTS5(TestConnection,
                              nsIRunnable,
                              nsIStreamListener,
                              nsIStreamObserver,
                              nsIInterfaceRequestor,
                              nsIProgressEventSink);

NS_IMETHODIMP
TestConnection::Run(void)
{
  nsresult rv = NS_OK;

  // Create the Event Queue for this thread...
  NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = eventQService->CreateThreadEventQueue();
  if (NS_FAILED(rv)) return rv;

  //
  // Make sure that all resources were allocated in the constructor...
  //
  if (!mTransport) {
    rv = NS_ERROR_FAILURE;
  }

  if (NS_SUCCEEDED(rv)) {
    if (mIsAsync) {
      
      //
      // Initiate an async read...
      //
      rv = mTransport->AsyncRead(this, mTransport);

      if (NS_FAILED(rv)) {
        printf("Error: AsyncRead failed...");
      } else {
        rv = WriteBuffer();
      }
      Pump_PLEvents(eventQService);    
    }
    else {
      while (NS_SUCCEEDED(rv)) {
        rv = WriteBuffer();

        if (NS_SUCCEEDED(rv)) {
          rv = ReadBuffer();
        }
      }
    }
  }

  if (gVerbose)
    printf("Transport thread exiting...\n");
  return rv;
}


nsresult TestConnection::WriteBuffer(void)
{
  nsresult rv;
  char *buffer;
  PRInt32 size;
  PRUint32 bytesWritten;

  if (mBufferChar == 'z') {
    mBufferChar = 'a';
  } else {
    mBufferChar++;
  }

  if (gVerbose)
    printf("\n+++ Request is: %c.  Context = %p\n", mBufferChar, mTransport);

  // Create and fill a test buffer of data...
  buffer = (char*)PR_Malloc(mBufferLength + 4);

  if (buffer) {
    for (size=0; size<mBufferLength-2; size++) {
      buffer[size] = mBufferChar;
    }
    buffer[size++] = '\r';
    buffer[size++] = '\n';
    buffer[size]   = 0;

    //
    // Async case...
    //
    if (mStream) {
      rv = mOut->Write(buffer, size, &bytesWritten);

      // Write the buffer to the server...
      if (NS_SUCCEEDED(rv)) {
        rv = mTransport->SetTransferCount(bytesWritten);
        if (NS_SUCCEEDED(rv)) {
          rv = mTransport->AsyncWrite(mStream, /* mOutputObserver */ nsnull, mTransport);
        }
      } 
      // Wait for the write to complete...
      if (NS_FAILED(rv)) {
        printf("Error: AsyncWrite failed...");
      }
    } 
    //
    // Synchronous case...
    //
    else if (mOutStream) {
      rv = mOutStream->Write(buffer, size, &bytesWritten);
      NS_ASSERTION((PRUint32)size == bytesWritten, "Not enough was written...");
    }
    PR_Free(buffer);
  } else {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}


nsresult TestConnection::ReadBuffer(void)
{
  nsresult rv = NS_OK;

  //
  // Synchronous case...
  //
  if (mInStream) {
    char *buffer;
    PRUint32 bytesRead;

    buffer = (char*)PR_Malloc(mBufferLength + 4);

    if (buffer) {
      rv = mInStream->Read(buffer, mBufferLength, &bytesRead);

      if (NS_SUCCEEDED(rv) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        if (gVerbose)
          printf("TestConnection::ReadBuffer.  Read %d bytes\n", bytesRead);
        puts(buffer);
      }
      PR_Free(buffer);
    }
  }

  return rv;
}

nsresult TestConnection::Process(void)
{
  return NS_OK;
}


nsresult TestConnection::Suspend(void)
{
  nsresult rv;

  if (mTransport) {
    rv = mTransport->Suspend();
  } else {
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}

nsresult TestConnection::Resume(void)
{
  nsresult rv;

  if (mTransport) {
    rv = mTransport->Resume();
  } else {
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}






#if defined(USE_TIMERS)

void TimerCallback(nsITimer* aTimer, void* aClosure)
{
  static PRBool flag = PR_FALSE;
  int i;

  if (flag) {
    printf("Resuming connections...\n");
  } else {
    printf("Suspending connections...\n");
  }


  for (i=0; i<NUM_TEST_THREADS; i++) {
    TestConnection* connection;

    connection = gConnections[i];
    if (connection) {
      if (flag) {
        connection->Resume();
      } else {
        connection->Suspend();
      }
    }
  }
  flag = !flag;

  gPeriodicTimer = do_CreateInstance("component://netscape/timer", &rv);
  if (NS_SUCCEEDED(rv)) {
    gPeriodicTimer->Init(TimerCallback, nsnull, 1000*5);
  }
}

#endif /* USE_TIMERS */

nsresult NS_AutoregisterComponents();
nsresult NS_AutoregisterComponents()
{
  nsresult rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, NULL /* default */);
  return rv;
}

int
main(int argc, char* argv[])
{
  nsresult rv;

  // -----
  //
  // Parse the command line args...
  //
  // -----
///  if (argc < 3) {
///      printf("usage: %s [-sync|-silent] <host> <path>\n", argv[0]);
///      return -1;
///  }

  PRBool bIsAsync = PR_TRUE;
  char* hostName = nsnull;
  int i;


  for (i=1; i<argc; i++) {
    // Turn on synchronous mode...
    if (PL_strcasecmp(argv[i], "-sync") == 0) {
      bIsAsync = PR_FALSE;
      continue;
    } 
    if (PL_strcasecmp(argv[i], "-silent") == 0) {
      gVerbose = PR_FALSE;
      continue;
    }

    hostName = argv[i];
  }
  if (!hostName) {
    hostName = "chainsaw";
  }
  printf("Using %s as echo server...\n", hostName);

#ifdef XP_MAC
	(void) PR_PutEnv("NSPR_LOG_MODULES=nsSocketTransport:5");
	(void) PR_PutEnv("NSPR_LOG_FILE=nspr.log");
	PR_Init_Log();
#endif

  // -----
  //
  // Initialize XPCom...
  //
  // -----

  rv = NS_AutoregisterComponents();
  if (NS_FAILED(rv)) return rv;

  // Create the Event Queue for this thread...
  NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = eventQService->CreateThreadEventQueue();
  if (NS_FAILED(rv)) return rv;

  //
  // Create the connections and threads...
  //
  for (i=0; i<NUM_TEST_THREADS; i++) {
    gConnections[i] = new TestConnection(hostName, 7, bIsAsync);
    rv = NS_NewThread(&gThreads[i], gConnections[i], 0, PR_JOINABLE_THREAD);
  }


#if defined(USE_TIMERS)
  //
  // Start up the timer to test Suspend/Resume APIs on the transport...
  //
  nsresult rv;
  gPeriodicTimer = do_CreateInstance("component://netscape/timer", &rv);
  if (NS_SUCCEEDED(rv)) {
    gPeriodicTimer->Init(TimerCallback, nsnull, 1000);
  }
#endif /* USE_TIMERS */
  

  // Enter the message pump to allow the URL load to proceed.
  Pump_PLEvents(eventQService);

  PRTime endTime;
  endTime = PR_Now();

//  printf("Elapsed time: %d\n", (PRInt32)(endTime/1000UL - gElapsedTime/1000UL));

  return 0;
}
