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
#include <windows.h>
#endif

#define NSPIPE2

#include "nspr.h"
#include "nscore.h"
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
#include "nsIBufferOutputStream.h"
#endif
#include "nsIBufferInputStream.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kEventQueueCID, NS_EVENTQUEUE_CID);

static PRTime gElapsedTime;
static int gKeepRunning = 1;
static nsIEventQueue* gEventQ = nsnull;

class InputTestConsumer : public nsIStreamListener
{
public:

  InputTestConsumer();
  virtual ~InputTestConsumer();

  // ISupports interface...
  NS_DECL_ISUPPORTS

  // IStreamListener interface...
  NS_IMETHOD OnStartRequest(nsIChannel* channel, nsISupports* context);

  NS_IMETHOD OnDataAvailable(nsIChannel* channel, nsISupports* context,
                             nsIInputStream *aIStream, 
                             PRUint32 aSourceOffset,
                             PRUint32 aLength);

  NS_IMETHOD OnStopRequest(nsIChannel* channel, nsISupports* context,
                           nsresult aStatus,
                           const PRUnichar* aMsg);

};


InputTestConsumer::InputTestConsumer()
{
  NS_INIT_REFCNT();
}

InputTestConsumer::~InputTestConsumer()
{
}


NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);
NS_IMPL_ISUPPORTS(InputTestConsumer,kIStreamListenerIID);


NS_IMETHODIMP
InputTestConsumer::OnStartRequest(nsIChannel* channel, nsISupports* context)
{
  printf("\n+++ InputTestConsumer::OnStartRequest +++\n");
  return NS_OK;
}


NS_IMETHODIMP
InputTestConsumer::OnDataAvailable(nsIChannel* channel,
                                   nsISupports* context,
                                   nsIInputStream *aIStream, 
                                   PRUint32 aSourceOffset,
                                   PRUint32 aLength)
{
  char buf[1025];
  PRUint32 amt;
  do {
    aIStream->Read(buf, 1024, &amt);
    buf[amt] = '\0';
    puts(buf);
  } while (amt != 0);

  return NS_OK;
}


NS_IMETHODIMP
InputTestConsumer::OnStopRequest(nsIChannel* channel,
                                 nsISupports* context,
                                 nsresult aStatus,
                                 const PRUnichar* aMsg)
{
  gKeepRunning = 0;
  printf("\n+++ InputTestConsumer::OnStopRequest (status = %x) +++\n", aStatus);
  return NS_OK;
}



class TestWriteObserver : public nsIStreamObserver
{
public:

  TestWriteObserver(nsIChannel* aTransport);
  virtual ~TestWriteObserver();

  // ISupports interface...
  NS_DECL_ISUPPORTS

  // IStreamObserver interface...
  NS_IMETHOD OnStartRequest(nsIChannel* channel, nsISupports* context);

  NS_IMETHOD OnStopRequest(nsIChannel* channel, nsISupports* context,
                           nsresult aStatus,
                           const PRUnichar* aMsg);

protected:
  nsIChannel* mTransport;
};


TestWriteObserver::TestWriteObserver(nsIChannel* aTransport)
{
  NS_INIT_REFCNT();
  mTransport = aTransport;
  NS_ADDREF(mTransport);
}

TestWriteObserver::~TestWriteObserver()
{
  NS_RELEASE(mTransport);
}


NS_IMPL_ISUPPORTS(TestWriteObserver,nsCOMTypeInfo<nsIStreamObserver>::GetIID());


NS_IMETHODIMP
TestWriteObserver::OnStartRequest(nsIChannel* channel, nsISupports* context)
{
  printf("\n+++ TestWriteObserver::OnStartRequest +++\n");
  return NS_OK;
}


NS_IMETHODIMP
TestWriteObserver::OnStopRequest(nsIChannel* channel, nsISupports* context,
                                 nsresult aStatus,
                                 const PRUnichar* aMsg)
{
  printf("\n+++ TestWriteObserver::OnStopRequest (status = %x) +++\n", aStatus);

  if (NS_SUCCEEDED(aStatus)) {
    mTransport->AsyncRead(0, -1, nsnull, new InputTestConsumer);
  } else {
    gKeepRunning = 0;
  }

  return NS_OK;
}

nsresult NS_AutoregisterComponents()
{
  nsresult rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, NULL /* default */);
  return rv;
}

int
main(int argc, char* argv[])
{
  nsresult rv;

  if (argc < 3) {
      printf("usage: %s <host> <path>\n", argv[0]);
      return -1;
  }

  char* hostName = argv[1];
  char* fileName = argv[2];
  int port = 80;

  rv =  NS_AutoregisterComponents();

  if (NS_FAILED(rv)) return rv;

  // Create the Event Queue for this thread...
  NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = eventQService->CreateThreadEventQueue();
  if (NS_FAILED(rv)) return rv;

  eventQService->GetThreadEventQueue(PR_CurrentThread(), &gEventQ);

  // Create the Socket transport service...
  NS_WITH_SERVICE(nsISocketTransportService, sts, kSocketTransportServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;

  // Create a stream for the data being written to the server...
  nsIBufferInputStream* stream;
  PRUint32 bytesWritten;

#ifndef NSPIPE2
  nsCOMPtr<nsIBuffer> buf;
  rv = NS_NewBuffer(&buf, 1024, 4096, nsnull);
  rv = NS_NewBufferInputStream(&stream, buf);
#else
  nsCOMPtr<nsIBufferOutputStream> out;
  rv = NS_NewPipe(&stream, getter_AddRefs(out), nsnull,
                  1024, 4096);
#endif
  if (NS_FAILED(rv)) return rv;

  char *buffer = PR_smprintf("GET %s HTML/1.0%s%s", fileName, CRLF, CRLF);
#if 0
  stream->Fill(buffer, strlen(buffer), &bytesWritten);
#else
#ifndef NSPIPE2
  buf->Write(buffer, strlen(buffer), &bytesWritten);
#else
  out->Write(buffer, strlen(buffer), &bytesWritten);
#endif
#endif
  printf("\n+++ Request is: %s\n", buffer);

  // Create the socket transport...
  nsIChannel* transport;
  rv = sts->CreateTransport(hostName, port, &transport);

// This stuff is used to test the output stream
#if 0
  nsIOutputStream* outStr = nsnull;
  rv = transport->OpenOutputStream(&outStr);

  if (NS_SUCCEEDED(rv)) {
    PRUint32 bytes;
    rv = outStr->Write("test", 4, &bytes);


    if (NS_FAILED(rv)) return rv;
  }

  rv = outStr->Close();
#else
  if (NS_SUCCEEDED(rv)) {
    TestWriteObserver* observer = new TestWriteObserver(transport);

    gElapsedTime = PR_Now();
    transport->AsyncWrite(stream, 0, bytesWritten, nsnull, observer);

    NS_RELEASE(transport);
  }
#endif

  if (NS_FAILED(rv)) return rv;

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
    rv = gEventQ->HandleEvent(gEvent);
#endif
#endif
  }

  PRTime endTime;
  endTime = PR_Now();
  printf("Elapsed time: %ld\n", (PRInt32)(endTime/1000UL-gElapsedTime/1000UL));

  sts->Shutdown();
  NS_RELEASE(sts);
  NS_RELEASE(eventQService);

  return 0;
}
