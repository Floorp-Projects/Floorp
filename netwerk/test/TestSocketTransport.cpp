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
//#define USE_TIMERS  // Only use nsITimer on Windows (for now...)
#include <windows.h>
#endif

#include "nspr.h"

#define NSPIPE2

#include "nsISocketTransportService.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsIChannel.h"
#include "nsIStreamObserver.h"
#include "nsIStreamListener.h"
#ifndef NSPIPE2
#include "nsIBuffer.h"
#else
#include "nsIPipe.h"
#endif
#include "nsIBufferInputStream.h"
#include "nsIBufferOutputStream.h"
#include "nsIThread.h"
#include "nsITimer.h"

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

static PRTime gElapsedTime;
static int gKeepRunning = 1;
static nsIEventQueue* gEventQ = nsnull;

#define NUM_TEST_THREADS 5

static TestConnection* gConnections[NUM_TEST_THREADS];
static nsIThread*      gThreads[NUM_TEST_THREADS];
//static nsITimer*       gPeriodicTimer;


void Pump_PLEvents(nsIEventQueueService * eventQService);
void Pump_PLEvents(nsIEventQueueService * eventQService)
{
	nsIEventQueue* eventQ = nsnull;
  eventQService->GetThreadEventQueue(PR_CurrentThread(), &eventQ);

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
                       public nsIStreamListener
{
public:
  TestConnection(const char* aHostName, PRInt32 aPort, PRBool aAsyncFlag);
  virtual ~TestConnection();

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  // nsIRunnable interface...
  NS_IMETHOD Run(void);

  // IStreamListener interface...
  NS_IMETHOD OnStartRequest(nsIChannel* channel, nsISupports* context);

  NS_IMETHOD OnDataAvailable(nsIChannel* channel, nsISupports* context,
                             nsIInputStream *aIStream, 
                             PRUint32 aSourceOffset,
                             PRUint32 aLength);

  NS_IMETHOD OnStopRequest(nsIChannel* channel, nsISupports* context,
                           nsresult aStatus,
                           const PRUnichar* aMsg);

  // TestConnection methods...
  nsresult WriteBuffer(void);
  nsresult ReadBuffer(void);

  nsresult Process(void);

  nsresult Suspend(void);
  nsresult Resume(void);

protected:
#ifndef NSPIPE2
  nsIBuffer* mBuffer;
#else
  nsIBufferOutputStream* mOut;
#endif
  nsIBufferInputStream* mStream;

  nsIInputStream*  mInStream;
  nsIOutputStream* mOutStream;

  nsIChannel*   mTransport;

  PRBool  mIsAsync;
  PRInt32 mBufferLength;
  char    mBufferChar;

  PRInt32 mBytesRead;
};


NS_IMETHODIMP
TestConnection::OnStartRequest(nsIChannel* channel, nsISupports* context)
{
  printf("\n+++ TestConnection::OnStartRequest +++. Context = %p\n", context);
  return NS_OK;
}


NS_IMETHODIMP
TestConnection::OnDataAvailable(nsIChannel* channel, nsISupports* context,
                                nsIInputStream *aIStream, 
                                PRUint32 aSourceOffset,
                                PRUint32 aLength)
{
  char buf[1025];
  PRUint32 amt;

  printf("\n+++ TestConnection::OnDavaAvailable +++."
         "\tContext = %p length = %d\n", 
         context, aLength);
  do {
    aIStream->Read(buf, 1024, &amt);
    mBytesRead += amt;
    buf[amt] = '\0';
    puts(buf);
  } while (amt != 0);

  if (mBufferLength == mBytesRead) {
    mBytesRead = 0;
    WriteBuffer();
  }

  return NS_OK;
}


NS_IMETHODIMP
TestConnection::OnStopRequest(nsIChannel* channel, 
                              nsISupports* context,
                              nsresult aStatus,
                              const PRUnichar* aMsg)
{
  printf("\n+++ TestConnection::OnStopRequest (status = %x) +++."
         "\tContext = %p\n", 
         aStatus, context);
  return NS_OK;
}


TestConnection::TestConnection(const char* aHostName, PRInt32 aPort, PRBool aAsyncFlag)
{
  nsresult rv;

  NS_INIT_REFCNT();

  mIsAsync      = aAsyncFlag;

  mBufferLength = 255;
  mBufferChar   = 'a';
  mBytesRead    = 0;

  mTransport = nsnull;
#ifndef NSPIPE2
  mBuffer    = nsnull;
#else
  mOut       = nsnull;
#endif
  mStream    = nsnull;

  mInStream  = nsnull;
  mOutStream = nsnull;

  // Create a socket transport...
  NS_WITH_SERVICE(nsISocketTransportService, sts, kSocketTransportServiceCID, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = sts->CreateTransport(aHostName, aPort, &mTransport);
  }


  if (NS_SUCCEEDED(rv)) {
    if (mIsAsync) {
      // Create a stream for the data being written to the server...
      if (NS_SUCCEEDED(rv)) {
#ifndef NSPIPE2
        rv = NS_NewBuffer(&mBuffer, 1024, 4096, nsnull);
        rv = NS_NewBufferInputStream(&mStream, mBuffer);
#else
        rv = NS_NewPipe(&mStream, &mOut, nsnull, 1024, 4096);
#endif
      }
    } 
    // Synchronous transport...
    else {
      rv = mTransport->OpenInputStream(0, -1, &mInStream);
      rv = mTransport->OpenOutputStream(0, &mOutStream);
    }
  }
}


TestConnection::~TestConnection()
{
  NS_IF_RELEASE(mTransport);
  // Async resources...
  NS_IF_RELEASE(mStream);
#ifndef NSPIPE2
  NS_IF_RELEASE(mBuffer);
#else
  NS_IF_RELEASE(mOut);
#endif

  // Sync resources...
  NS_IF_RELEASE(mInStream);
  NS_IF_RELEASE(mOutStream);
}

NS_IMPL_THREADSAFE_ADDREF(TestConnection);
NS_IMPL_THREADSAFE_RELEASE(TestConnection);

NS_IMETHODIMP
TestConnection::QueryInterface(const nsIID& aIID, void* *aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER; 
  } 
  if (aIID.Equals(nsCOMTypeInfo<nsIRunnable>::GetIID()) ||
      aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
    *aInstancePtr = NS_STATIC_CAST(nsIRunnable*, this); 
    NS_ADDREF_THIS(); 
    return NS_OK; 
  } 
  if (aIID.Equals(nsCOMTypeInfo<nsIStreamListener>::GetIID())) {
    *aInstancePtr = NS_STATIC_CAST(nsIStreamListener*, this); 
    NS_ADDREF_THIS(); 
    return NS_OK; 
  } 
  if (aIID.Equals(nsCOMTypeInfo<nsIStreamObserver>::GetIID())) {
    *aInstancePtr = NS_STATIC_CAST(nsIStreamObserver*, this); 
    NS_ADDREF_THIS(); 
    return NS_OK; 
  } 
  return NS_NOINTERFACE; 
}

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
      rv = mTransport->AsyncRead(0, -1, mTransport, this);

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
#if 0
      rv = mStream->Fill(buffer, size, &bytesWritten);
#else
#ifndef NSPIPE2
      rv = mBuffer->Write(buffer, size, &bytesWritten);
#else
      rv = mOut->Write(buffer, size, &bytesWritten);
#endif
#endif

      // Write the buffer to the server...
      if (NS_SUCCEEDED(rv)) {
        rv = mTransport->AsyncWrite(mStream, 0, bytesWritten, mTransport,
                                    /* mOutputObserver */ nsnull);
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

      if (NS_SUCCEEDED(rv)) {
        buffer[bytesRead] = '\0';
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

  NS_RELEASE(gPeriodicTimer);

  if (NS_OK == NS_NewTimer(&gPeriodicTimer)) {
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
///      printf("usage: %s <host> <path>\n", argv[0]);
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
  Pump_PLEvents(eventQService);

  PRTime endTime;
  endTime = PR_Now();

//  printf("Elapsed time: %ld\n", (PRInt32)(endTime/1000UL - gElapsedTime/1000UL));

  NS_RELEASE(eventQService);

  return 0;
}
