/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include <windows.h>
#endif

#include "nspr.h"

#include "nsISocketTransportService.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsITransport.h"
#include "nsIStreamObserver.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIByteBufferInputStream.h"
#include "nsIThread.h"

#include "nsCRT.h"

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID,      NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kEventQueueCID, NS_EVENTQUEUE_CID);

static PRTime gElapsedTime;
static int gKeepRunning = 1;
static nsIEventQueue* gEventQ = nsnull;

class TestConnection;
class InputConsumer;
class OutputObserver;

class TestConnection : public nsIRunnable
{
public:
  TestConnection(const char* aHostName, PRInt32 aPort);
  ~TestConnection();

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  // nsIRunnable interface...
  NS_IMETHOD Run(void);


  nsresult WriteBuffer(void);
  nsresult ReadBuffer(void);

  nsresult Process(void);

  void Lock(void)   { PR_EnterMonitor(mMonitor); }
  void Notify(void) { PR_Notify(mMonitor); }
  void Wait(void)   { PR_Wait(mMonitor, PR_INTERVAL_NO_TIMEOUT); }
  void Unlock(void) { PR_ExitMonitor(mMonitor); }

  PRInt32 GetBufferLength(void) { return mBufferLength; }
  char    GetBufferChar(void)   { return mBufferChar; }

protected:
  nsIByteBufferInputStream* mStream;


  InputConsumer*  mInputConsumer;
  OutputObserver* mOutputObserver;

  nsITransport*   mTransport;

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
  mConnection->Lock();

  printf("\n+++ OutputObserver::OnStopBinding (status = %x) +++.  Context = %p\n", aStatus, context);
  mConnection->Notify();

  mConnection->Unlock();

  return NS_OK;
}



// -----
//
// TestConnection class...
//
// -----

TestConnection::TestConnection(const char* aHostName, PRInt32 aPort)
{
  nsresult rv;

  NS_INIT_REFCNT();

  mBufferLength = 255;
  mBufferChar = 'a';

  mTransport = nsnull;
  mStream    = nsnull;
  mMonitor   = nsnull;

  // Create the input and output stream observers...
  mInputConsumer  = new InputConsumer(this);
  mOutputObserver = new OutputObserver(this);

  NS_IF_ADDREF(mInputConsumer);
  NS_IF_ADDREF(mOutputObserver);

  // Create a socket transport...
  NS_WITH_SERVICE(nsISocketTransportService, sts, kSocketTransportServiceCID, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = sts->CreateTransport(aHostName, aPort, &mTransport);
  }

  // Create a stream for the data being written to the server...
  if (NS_SUCCEEDED(rv)) {
    rv = NS_NewByteBufferInputStream(&mStream);
  }

  mMonitor = PR_NewMonitor();
}


TestConnection::~TestConnection()
{
  NS_IF_RELEASE(mInputConsumer);
  NS_IF_RELEASE(mOutputObserver);
  NS_IF_RELEASE(mTransport);
  NS_IF_RELEASE(mStream);

  if (mMonitor) {
    PR_DestroyMonitor(mMonitor);
  }
}

NS_IMPL_ISUPPORTS(TestConnection,nsIRunnable::GetIID());

NS_IMETHODIMP
TestConnection::Run(void)
{
  nsresult rv = NS_OK;

  if (mInputConsumer && mOutputObserver && mTransport && mStream) {
    Lock();
    ReadBuffer();
    while (1) {
      WriteBuffer();
      Wait();

      if (mBufferChar == 'z') {
        mBufferChar = 'a';
      } else {
        mBufferChar++;
      }
    }
    Unlock();
  }

  return rv;
}


nsresult TestConnection::WriteBuffer(void)
{
  nsresult rv;
  char *buffer;
  PRUint32 bytesWritten;
  int i;

  // Create and fill a test buffer of data...
  buffer = (char*)PR_Malloc(mBufferLength + 4);

  if (buffer) {
    for (i=0; i<mBufferLength-2; i++) {
      buffer[i] = mBufferChar;
    }
    buffer[i]   = '\r';
    buffer[i+1] = '\n';
    buffer[i+2] = 0;

    rv = mStream->Fill(buffer, strlen(buffer), &bytesWritten);
    printf("\n+++ Request is: %c.  Context = %p\n", mBufferChar, mTransport);

    PR_Free(buffer);
  } else {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  // Write the buffer to the server...
  if (NS_SUCCEEDED(rv)) {
    rv = mTransport->AsyncWrite(mStream, mTransport, gEventQ, mOutputObserver);
  } 
  if (NS_FAILED(rv)) {
    printf("Error: AsyncWrite failed...");
  }

  return rv;
}


nsresult TestConnection::ReadBuffer(void)
{
  nsresult rv;

  rv = mTransport->AsyncRead(mTransport, gEventQ, mInputConsumer);

  if (NS_FAILED(rv)) {
    printf("Error: AsyncRead failed...");
  }

  return rv;
}

nsresult TestConnection::Process(void)
{
  return NS_OK;
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

  TestConnection* connections[5];
  nsIThread* threads[5];
  int i;

  for (i=0; i<5; i++) {
    connections[i] = new TestConnection("chainsaw", 7);
    rv = NS_NewThread(&threads[i], connections[i]);
  }

  
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
#endif
  }


  PRTime endTime;
  endTime = PR_Now();
  printf("Elapsed time: %ld\n", (PRInt32)(endTime/1000UL-gElapsedTime/1000UL));

  NS_RELEASE(eventQService);

  return 0;
}
