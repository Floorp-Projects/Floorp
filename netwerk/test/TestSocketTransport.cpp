/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
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
#include <stdio.h>

#ifdef WIN32
#define USE_TIMERS  // Only use nsITimer on Windows (for now...)
#include <windows.h>
#endif

#include "nspr.h"

#include "nsISocketTransportService.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsIChannel.h"
#include "nsIStreamObserver.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIByteBufferInputStream.h"
#include "nsIThread.h"
#include "nsITimer.h"

#include "nsCRT.h"

// Forward declarations...
class TestConnection;
class InputConsumer;
class OutputObserver;



static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID,      NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kEventQueueCID, NS_EVENTQUEUE_CID);

static PRTime gElapsedTime;
static int gKeepRunning = 1;
static nsIEventQueue* gEventQ = nsnull;

#define NUM_TEST_THREADS 5

static TestConnection* gConnections[NUM_TEST_THREADS];
static nsIThread*      gThreads[NUM_TEST_THREADS];
static nsITimer*       gPeriodicTimer;

class TestConnection : public nsIRunnable
{
public:
  TestConnection(const char* aHostName, PRInt32 aPort, PRBool aAsyncFlag);
  ~TestConnection();

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  // nsIRunnable interface...
  NS_IMETHOD Run(void);


  nsresult WriteBuffer(void);
  nsresult ReadBuffer(void);

  nsresult Process(void);

  nsresult Suspend(void);
  nsresult Resume(void);

  void Lock(void)   { PR_EnterMonitor(mMonitor); }
  void Notify(void) { PR_Notify(mMonitor); }
  void Wait(void)   { PR_Wait(mMonitor, PR_INTERVAL_NO_TIMEOUT); }
  void Unlock(void) { PR_ExitMonitor(mMonitor); }

  PRInt32 GetBufferLength(void) { return mBufferLength; }
  char    GetBufferChar(void)   { return mBufferChar; }

protected:
  nsIByteBufferInputStream* mStream;

  nsIInputStream*  mInStream;
  nsIOutputStream* mOutStream;

  InputConsumer*  mInputConsumer;
  OutputObserver* mOutputObserver;

  nsIChannel*   mTransport;

  PRBool  mIsAsync;
  PRInt32 mBufferLength;
  char    mBufferChar;

  PRMonitor* mMonitor;
};


// -----
//
// InputConsumer class...
//
// -----
class InputConsumer : public nsIStreamListener
{
public:
           InputConsumer(TestConnection* aConnection);
  virtual ~InputConsumer();

  // ISupports interface...
  NS_DECL_ISUPPORTS

  // IStreamListener interface...
  NS_IMETHOD OnStartBinding(nsISupports* context);

  NS_IMETHOD OnDataAvailable(nsISupports* context,
                             nsIInputStream *aIStream, 
                             PRUint32 aSourceOffset,
                             PRUint32 aLength);

  NS_IMETHOD OnStopBinding(nsISupports* context,
                           nsresult aStatus,
                           nsIString* aMsg);

  NS_IMETHOD OnStartRequest(nsISupports* context) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD OnStopRequest(nsISupports* context,
                           nsresult aStatus,
                           nsIString* aMsg) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  TestConnection* mConnection;
  PRInt32 mBytesRead;
};

InputConsumer::InputConsumer(TestConnection* aConnection)
{
  NS_INIT_REFCNT();

  mBytesRead = 0;

  mConnection = aConnection;
  NS_IF_ADDREF(mConnection);
}

InputConsumer::~InputConsumer()
{
  NS_IF_RELEASE(mConnection);
}



NS_IMPL_ISUPPORTS(InputConsumer,nsIStreamListener::GetIID());


NS_IMETHODIMP
InputConsumer::OnStartBinding(nsISupports* context)
{
  printf("\n+++ InputConsumer::OnStartBinding +++. Context = %p\n", context);
  return NS_OK;
}


NS_IMETHODIMP
InputConsumer::OnDataAvailable(nsISupports* context,
                               nsIInputStream *aIStream, 
                               PRUint32 aSourceOffset,
                               PRUint32 aLength)
{
  char buf[1025];
  PRUint32 amt;

  printf("\n+++ InputConsumer::OnDavaAvailable +++.  Context = %p\n", context);
  mConnection->Lock();
  do {
    nsresult rv = aIStream->Read(buf, 1024, &amt);
    mBytesRead += amt;
    buf[amt] = '\0';
///    printf(buf);
  } while (amt != 0);

  if (mConnection->GetBufferLength() == mBytesRead) {
    mBytesRead = 0;
    mConnection->Notify();
  }
  mConnection->Unlock();

  return NS_OK;
}


NS_IMETHODIMP
InputConsumer::OnStopBinding(nsISupports* context,
                             nsresult aStatus,
                            nsIString* aMsg)
{
  printf("\n+++ InputConsumer::OnStopBinding (status = %x) +++.  Context = %p\n", aStatus, context);
  mConnection->Lock();
  mConnection->Notify();
  mConnection->Unlock();
  return NS_OK;
}



// -----
//
// OutputObserver class...
//
// -----
class OutputObserver : public nsIStreamObserver
{
public:

  OutputObserver(TestConnection* aConnection);
  virtual ~OutputObserver();

  // ISupports interface...
  NS_DECL_ISUPPORTS

  // IStreamObserver interface...
  NS_IMETHOD OnStartBinding(nsISupports* context);

  NS_IMETHOD OnStopBinding(nsISupports* context,
                           nsresult aStatus,
                           nsIString* aMsg);

  NS_IMETHOD OnStartRequest(nsISupports* context) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD OnStopRequest(nsISupports* context,
                           nsresult aStatus,
                           nsIString* aMsg) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

protected:
  TestConnection* mConnection;
};


OutputObserver::OutputObserver(TestConnection* aConnection)
{
  NS_INIT_REFCNT();

  mConnection = aConnection;
  NS_IF_ADDREF(mConnection);
}

OutputObserver::~OutputObserver()
{
  NS_IF_RELEASE(mConnection);
}


NS_IMPL_ISUPPORTS(OutputObserver,nsIStreamObserver::GetIID());


NS_IMETHODIMP
OutputObserver::OnStartBinding(nsISupports* context)
{
  printf("\n+++ OutputObserver::OnStartBinding +++. Context = %p\n", context);
  return NS_OK;
}


NS_IMETHODIMP
OutputObserver::OnStopBinding(nsISupports* context,
                                 nsresult aStatus,
                                 nsIString* aMsg)
{
///  mConnection->Lock();

  printf("\n+++ OutputObserver::OnStopBinding (status = %x) +++.  Context = %p\n", aStatus, context);
///  mConnection->Notify();

///  mConnection->Unlock();

  return NS_OK;
}



// -----
//
// TestConnection class...
//
// -----

TestConnection::TestConnection(const char* aHostName, PRInt32 aPort, PRBool aAsyncFlag)
{
  nsresult rv;

  NS_INIT_REFCNT();

  mIsAsync      = aAsyncFlag;
  mBufferLength = 255;
  mBufferChar   = 'a';

  mTransport = nsnull;
  mStream    = nsnull;

  mInStream  = nsnull;
  mOutStream = nsnull;

  mInputConsumer  = nsnull;
  mOutputObserver = nsnull;

  // Create the monitor used for synchronization...
  mMonitor = PR_NewMonitor();

  // Create a socket transport...
  NS_WITH_SERVICE(nsISocketTransportService, sts, kSocketTransportServiceCID, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = sts->CreateTransport(aHostName, aPort, &mTransport);
  }

  if (NS_SUCCEEDED(rv)) {
    if (mIsAsync) {
      // Create the input and output stream observers...
      mInputConsumer  = new InputConsumer(this);
      mOutputObserver = new OutputObserver(this);

      NS_IF_ADDREF(mInputConsumer);
      NS_IF_ADDREF(mOutputObserver);

      // Create a stream for the data being written to the server...
      if (NS_SUCCEEDED(rv)) {
        rv = NS_NewByteBufferInputStream(&mStream);
      }
    } 
    // Synchronous transport...
    else {
      rv = mTransport->OpenInputStream (&mInStream);
      rv = mTransport->OpenOutputStream(&mOutStream);
    }
  }
}


TestConnection::~TestConnection()
{
  NS_IF_RELEASE(mTransport);
  // Async resources...
  NS_IF_RELEASE(mInputConsumer);
  NS_IF_RELEASE(mOutputObserver);
  NS_IF_RELEASE(mStream);

  // Sync resources...
  NS_IF_RELEASE(mInStream);
  NS_IF_RELEASE(mOutStream);

  if (mMonitor) {
    PR_DestroyMonitor(mMonitor);
  }
}

NS_IMPL_ISUPPORTS(TestConnection,nsIRunnable::GetIID());

NS_IMETHODIMP
TestConnection::Run(void)
{
  nsresult rv = NS_OK;

  //
  // Make sure that all resources were allocated in the constructor...
  //
  if (!mTransport) {
    rv = NS_ERROR_FAILURE;
  }
  if (mIsAsync && (!mInputConsumer || !mOutputObserver)) {
    rv = NS_ERROR_FAILURE;
  }

  if (NS_SUCCEEDED(rv)) {
    if (mIsAsync) {
      //
      // Initiate an async read...
      //
      rv = mTransport->AsyncRead(0, -1, mTransport, gEventQ, mInputConsumer);

      if (NS_FAILED(rv)) {
        printf("Error: AsyncRead failed...");
      }
    }

    Lock();
    while (NS_SUCCEEDED(rv)) {
      rv = WriteBuffer();

      if (NS_SUCCEEDED(rv)) {
        rv = ReadBuffer();
      }

      if (mBufferChar == 'z') {
        mBufferChar = 'a';
      } else {
        mBufferChar++;
      }
    }
    Unlock();
  }

  printf("Transport thread exiting...\n");
  return rv;
}


nsresult TestConnection::WriteBuffer(void)
{
  nsresult rv;
  char *buffer;
  PRInt32 size;
  PRUint32 bytesWritten;

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
      rv = mStream->Fill(buffer, size, &bytesWritten);

      // Write the buffer to the server...
      if (NS_SUCCEEDED(rv)) {
        PRUint32 count;
        rv = mStream->GetLength(&count);
        rv = mTransport->AsyncWrite(mStream, 0, count, mTransport, gEventQ, /* mOutputObserver */ nsnull);
      } 
      // Wait for the write to complete...
      if (NS_SUCCEEDED(rv)) {
        Wait();
      } else {
        printf("Error: AsyncWrite failed...");
      }
    } 
    //
    // Synchronous case...
    //
    else if (mOutStream) {
      rv = mOutStream->Write(buffer, size, &bytesWritten);
    }
    printf("\n+++ Request is: %c.  Context = %p\n", mBufferChar, mTransport);

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
      rv = mInStream->Read(buffer, mBufferLength+2, &bytesRead);

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

  NS_RELEASE(gPeriodicTimer);

  if (NS_OK == NS_NewTimer(&gPeriodicTimer)) {
    gPeriodicTimer->Init(TimerCallback, nsnull, 1000*5);
  }
}

#endif /* USE_TIMERS */


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
///      printf("usage: %s <host> <path>\n", argv[0]);
///      return -1;
///  }

  char* hostName = argv[1];
  char* fileName = argv[2];
  int port = 80;
 
  // -----
  //
  // Initialize XPCom...
  //
  // -----
  // XXX why do I have to do this?!
  rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup,
                                        "components");
  if (NS_FAILED(rv)) return rv;

  // Create the Event Queue for this thread...
  NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = eventQService->CreateThreadEventQueue();
  if (NS_FAILED(rv)) return rv;

  eventQService->GetThreadEventQueue(PR_CurrentThread(), &gEventQ);

  int i;

  //
  // Create the connections and threads...
  //
  for (i=0; i<NUM_TEST_THREADS; i++) {
    gConnections[i] = new TestConnection("chainsaw", 7, PR_TRUE);
    rv = NS_NewThread(&gThreads[i], gConnections[i]);
  }

#if defined(USE_TIMERS)
  //
  // Start up the timer to test Suspend/Resume APIs on the transport...
  //
  if (NS_OK == NS_NewTimer(&gPeriodicTimer)) {
    gPeriodicTimer->Init(TimerCallback, nsnull, 1000);
  }
#endif /* USE_TIMERS */
  

  // Enter the message pump to allow the URL load to proceed.
  while ( gKeepRunning ) {
#ifdef WIN32
    MSG msg;

    if (GetMessage(&msg, NULL, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      gKeepRunning = FALSE;
    }
#else
#ifdef XP_MAC
    /* Mac stuff is missing here! */
#else
    PLEvent *gEvent;
    rv = gEventQ->GetEvent(&gEvent);
    PL_HandleEvent(gEvent);
#endif /* XP_UNIX */
#endif /* !WIN32 */
  }


  PRTime endTime;
  endTime = PR_Now();
  printf("Elapsed time: %ld\n", (PRInt32)(endTime/1000UL-gElapsedTime/1000UL));

  NS_RELEASE(eventQService);

  return 0;
}
