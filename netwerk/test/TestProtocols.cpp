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

/* 
    The TestProtocols tests the basic protocols architecture and can 
    be used to test individual protocols as well. If this grows too
    big then we should split it to individual protocols. 
    
    -Gagan Saksena 04/29/99
*/

#include <stdio.h>
#ifdef WIN32 
#include <windows.h>
#endif
#include "nspr.h"
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsIEventQueueService.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIStreamObserver.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIBufferInputStream.h"
#include "nsCRT.h"
#include "nsIChannel.h"
#include "nsIURL.h"
#include "nsIHTTPChannel.h"
#include "nsIHttpEventSink.h" 
#include "nsIEventSinkGetter.h" 

#if 0
// this test app handles cookies.
#include "nsICookieService.h"
static NS_DEFINE_CID(nsCookieServiceCID, NS_COOKIESERVICE_CID);
#endif // 0

static NS_DEFINE_CID(kEventQueueServiceCID,      NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kIOServiceCID,              NS_IOSERVICE_CID);

static PRTime gElapsedTime;
static int gKeepRunning = 0;
static nsIEventQueue* gEventQ = nsnull;

class TestHTTPEventSink : public nsIHTTPEventSink
{
public:

  TestHTTPEventSink();
  virtual ~TestHTTPEventSink();

  // ISupports interface...
  NS_DECL_ISUPPORTS

  // nsIHTTPEventSink interface...
  NS_IMETHOD      OnAwaitingInput(nsISupports* i_Context);

  NS_IMETHOD      OnHeadersAvailable(nsISupports* i_Context);

  NS_IMETHOD      OnProgress(nsISupports* i_Context, 
                             PRUint32 i_Progress, 
                             PRUint32 i_ProgressMax);

  // OnRedirect gets fired only if you have set FollowRedirects on the handler!
  NS_IMETHOD      OnRedirect(nsISupports* i_Context, 
                             nsIURI* i_NewLocation);
/*
  // IStreamListener interface...
  NS_IMETHOD OnStartBinding(nsISupports* context) { NS_ERROR("bad..."); return NS_ERROR_FAILURE; }

  NS_IMETHOD OnDataAvailable(nsISupports* context,
                             nsIBufferInputStream *aIStream, 
                             PRUint32 aSourceOffset,
                             PRUint32 aLength) { NS_ERROR("bad..."); return NS_ERROR_FAILURE; }

  NS_IMETHOD OnStopBinding(nsISupports* context,
                           nsresult aStatus,
                           const PRUnichar* aMsg) { NS_ERROR("bad..."); return NS_ERROR_FAILURE; }

  NS_IMETHOD OnStartRequest(nsISupports* context) { NS_ERROR("bad..."); return NS_ERROR_FAILURE; }

  NS_IMETHOD OnStopRequest(nsISupports* context,
                           nsresult aStatus,
                           const PRUnichar* aMsg) { NS_ERROR("bad..."); return NS_ERROR_FAILURE; }
*/
};

TestHTTPEventSink::TestHTTPEventSink()
{
  NS_INIT_REFCNT();
}

TestHTTPEventSink::~TestHTTPEventSink()
{
}


NS_IMPL_ISUPPORTS(TestHTTPEventSink,nsIHTTPEventSink::GetIID());

NS_IMETHODIMP
TestHTTPEventSink::OnAwaitingInput(nsISupports* context)
{
    printf("\n+++ TestHTTPEventSink::OnAwaitingInput +++\n");
    return NS_OK;
}

NS_IMETHODIMP
TestHTTPEventSink::OnHeadersAvailable(nsISupports* context)
{
    printf("\n+++ TestHTTPEventSink::OnHeadersAvailable +++\n");
    nsCOMPtr<nsIHTTPChannel> pHTTPCon(do_QueryInterface(context));
    if (pHTTPCon)
    {
      char* type;
      //optimize later TODO allow atoms here...! intead of just the header strings
      pHTTPCon->GetResponseHeader("Content-type", &type);
      if (type) {
        printf("\nRecieving ... %s\n", type);
        nsCRT::free(type);
      }
    }
    return NS_OK;
}

NS_IMETHODIMP
TestHTTPEventSink::OnProgress(nsISupports* context, PRUint32 i_Progress, PRUint32 i_ProgressMax)
{
    printf("\n+++ TestHTTPEventSink::OnProgress +++\n");
    return NS_OK;
}

NS_IMETHODIMP
TestHTTPEventSink::OnRedirect(nsISupports* context, nsIURI* i_NewLocation)
{
    printf("\n+++ TestHTTPEventSink::OnRedirect +++\n");
    return NS_OK;
}


class InputTestConsumer : public nsIStreamListener
{
public:

  InputTestConsumer();
  virtual ~InputTestConsumer();

  // ISupports interface...
  NS_DECL_ISUPPORTS

  // IStreamListener interface...
  NS_IMETHOD OnStartBinding(nsISupports* context);

  NS_IMETHOD OnDataAvailable(nsISupports* context,
                             nsIBufferInputStream *aIStream, 
                             PRUint32 aSourceOffset,
                             PRUint32 aLength);

  NS_IMETHOD OnStopBinding(nsISupports* context,
                           nsresult aStatus,
                           const PRUnichar* aMsg);

  NS_IMETHOD OnStartRequest(nsISupports* context) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD OnStopRequest(nsISupports* context,
                           nsresult aStatus,
                           const PRUnichar* aMsg) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

};


InputTestConsumer::InputTestConsumer()
{
  NS_INIT_REFCNT();
}

InputTestConsumer::~InputTestConsumer()
{
}


NS_IMPL_ISUPPORTS(InputTestConsumer,nsIStreamListener::GetIID());


NS_IMETHODIMP
InputTestConsumer::OnStartBinding(nsISupports* context)
{
  printf("\n+++ InputTestConsumer::OnStartBinding +++\n");
  return NS_OK;
}


NS_IMETHODIMP
InputTestConsumer::OnDataAvailable(nsISupports* context,
                                   nsIBufferInputStream *aIStream, 
                                   PRUint32 aSourceOffset,
                                   PRUint32 aLength)
{
  char buf[1025];
  PRUint32 amt;
  nsresult rv;

  do {
    rv = aIStream->Read(buf, 1024, &amt);
    if (rv == NS_BASE_STREAM_EOF) break;
    if (NS_FAILED(rv)) return rv;
    buf[amt] = '\0';
    puts(buf);
  } while (amt);

  return NS_OK;
}


NS_IMETHODIMP
InputTestConsumer::OnStopBinding(nsISupports* context,
                                 nsresult aStatus,
                                 const PRUnichar* aMsg)
{
  gKeepRunning -= 1;
  printf("\n+++ InputTestConsumer::OnStopBinding (status = %x) +++\n", aStatus);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

class nsEventSinkGetter : public nsIEventSinkGetter {
public:
    NS_DECL_ISUPPORTS

    nsEventSinkGetter() {
        NS_INIT_REFCNT();
    }

    NS_IMETHOD GetEventSink(const char* verb, const nsIID& eventSinkIID,
                            nsISupports* *result) {
        nsresult rv = NS_ERROR_FAILURE;

        if (nsCRT::strcmp(verb, "load") == 0) { // makeshift verb for now
            if (eventSinkIID.Equals(nsIHTTPEventSink::GetIID())) {
                TestHTTPEventSink *sink;

                sink = new TestHTTPEventSink();
                if (sink == nsnull)
                    return NS_ERROR_OUT_OF_MEMORY;
                NS_ADDREF(sink);
                rv = sink->QueryInterface(eventSinkIID, (void**)result);
                NS_RELEASE(sink);
            }
        }
        return rv;
    }
};

NS_IMPL_ISUPPORTS(nsEventSinkGetter, nsIEventSinkGetter::GetIID());

////////////////////////////////////////////////////////////////////////////////


nsresult StartLoadingURL(const char* aUrlString)
{
    nsresult rv;

    NS_WITH_SERVICE(nsIIOService, pService, kIOServiceCID, &rv);
    if (pService) {
        nsCOMPtr<nsIURI> pURL;

        rv = pService->NewURI(aUrlString, nsnull, getter_AddRefs(pURL));
        if (NS_FAILED(rv)) {
            NS_ERROR("NewURI failed!");
            return rv;
        }
        nsCOMPtr<nsIChannel> pChannel;
        nsEventSinkGetter* pMySink;

        pMySink = new nsEventSinkGetter();
        NS_IF_ADDREF(pMySink);
        if (!pMySink) {
            NS_ERROR("Failed to create a new consumer!");
            return NS_ERROR_OUT_OF_MEMORY;;
        }

        // Async reading thru the calls of the event sink interface
        rv = pService->NewChannelFromURI("load", pURL, pMySink, 
                                         getter_AddRefs(pChannel));
        if (NS_FAILED(rv)) {
            NS_ERROR("NewChannelFromURI failed!");
            return rv;
        }
        NS_RELEASE(pMySink);

        /* 
           You may optionally add/set other headers on this
           request object. This is done by QI for the specific
           protocolConnection.
        */
        nsCOMPtr<nsIHTTPChannel> pHTTPCon(do_QueryInterface(pChannel));

        if (pHTTPCon) {
            // Setting a sample user agent string.
            rv = pHTTPCon->SetRequestHeader("User-Agent", "Mozilla/5.0 [en] (Win98; U)");
            if (NS_FAILED(rv)) return rv;
        }
            
        InputTestConsumer* listener;

        listener = new InputTestConsumer;
        NS_IF_ADDREF(listener);
        if (!listener) {
            NS_ERROR("Failed to create a new stream listener!");
            return NS_ERROR_OUT_OF_MEMORY;;
        }


        rv = pChannel->AsyncRead(0,         // staring position
                                 -1,        // number of bytes to read
                                 nsnull,    // ISupports context
                                 gEventQ,   // nsIEventQ for marshalling
                                 listener); // IStreamListener consumer

        NS_RELEASE(listener);
    }

    return rv;
}

nsresult NS_AutoregisterComponents()
{
  nsresult rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, NULL /* default */);
  return rv;
}

int
main(int argc, char* argv[])
{
    nsresult rv= -1;
    if (argc < 2) {
        printf("usage: %s <url> \n", argv[0]);
        return -1;
    }

    /* 
      The following code only deals with XPCOM registration stuff. and setting
      up the event queues. Copied from TestSocketIO.cpp
    */

    rv = NS_AutoregisterComponents();
    if (NS_FAILED(rv)) return rv;

    
    // Create the Event Queue for this thread...
    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = eventQService->CreateThreadEventQueue();
    if (NS_FAILED(rv)) return rv;

    eventQService->GetThreadEventQueue(PR_CurrentThread(), &gEventQ);

#if 0
    // fire up an instance of the cookie manager.
    // I'm doing this using the serviceManager for convenience's sake.
    // Presumably an application will init it's own cookie service a 
    // different way (this way works too though).
    NS_WITH_SERVICE(nsICookieService, cookieService, nsCookieServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
#endif // 0

    int i;
    for (i=1; i<argc; i++) {
        rv = StartLoadingURL(argv[i]);
        if (NS_FAILED(rv)) return rv;
        gKeepRunning += 1;
    }

  // Enter the message pump to allow the URL load to proceed.
    while ( gKeepRunning ) {
#ifdef WIN32
        MSG msg;

        if (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            gKeepRunning = 0;
    }
#else
#ifdef XP_MAC
    /* Mac stuff is missing here! */
#else
    PLEvent *gEvent;
    rv = gEventQ->GetEvent(&gEvent);
    rv = gEventQ->HandleEvent(gEvent);
#endif /* XP_UNIX */
#endif /* !WIN32 */
    }

    return rv;
}
