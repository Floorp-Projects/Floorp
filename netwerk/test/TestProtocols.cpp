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

static NS_DEFINE_CID(kEventQueueServiceCID,      NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kIOServiceCID,              NS_IOSERVICE_CID);

static PRTime gElapsedTime;
static int gKeepRunning = 1;
static nsIEventQueue* gEventQ = nsnull;

class InputTestConsumer : public nsIHTTPEventSink
{
public:

  InputTestConsumer();
  virtual ~InputTestConsumer();

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

  // IStreamListener interface...
  NS_IMETHOD OnStartBinding(nsISupports* context);

  NS_IMETHOD OnDataAvailable(nsISupports* context,
                             nsIBufferInputStream *aIStream, 
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

};


InputTestConsumer::InputTestConsumer()
{
  NS_INIT_REFCNT();
}

InputTestConsumer::~InputTestConsumer()
{
}


//NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);
NS_IMPL_ISUPPORTS(InputTestConsumer,nsIHTTPEventSink::GetIID());


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
  do {
    nsresult rv = aIStream->Read(buf, 1024, &amt);
    buf[amt] = '\0';
    printf(buf);
  } while (amt != 0);

  return NS_OK;
}


NS_IMETHODIMP
InputTestConsumer::OnStopBinding(nsISupports* context,
                                 nsresult aStatus,
                                 nsIString* aMsg)
{
  gKeepRunning = 0;
  printf("\n+++ InputTestConsumer::OnStopBinding (status = %x) +++\n", aStatus);
  return NS_OK;
}

NS_IMETHODIMP
InputTestConsumer::OnAwaitingInput(nsISupports* context)
{
    printf("\n+++ InputTestConsumer::OnAwaitingInput +++\n");
    return NS_OK;
}

NS_IMETHODIMP
InputTestConsumer::OnHeadersAvailable(nsISupports* context)
{
    printf("\n+++ InputTestConsumer::OnHeadersAvailable +++\n");
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
InputTestConsumer::OnProgress(nsISupports* context, PRUint32 i_Progress, PRUint32 i_ProgressMax)
{
    printf("\n+++ InputTestConsumer::OnProgress +++\n");
    return NS_OK;
}

NS_IMETHODIMP
InputTestConsumer::OnRedirect(nsISupports* context, nsIURI* i_NewLocation)
{
    printf("\n+++ InputTestConsumer::OnRedirect +++\n");
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

class nsEventSinkGetter : public nsIEventSinkGetter {
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD GetEventSink(const char* verb, const nsIID& eventSinkIID,
                            nsISupports* *result) {
        nsresult rv = NS_ERROR_FAILURE;

        if (nsCRT::strcmp(verb, "load") == 0) { // makeshift verb for now
            if (eventSinkIID.Equals(nsIHTTPEventSink::GetIID())) {
                InputTestConsumer *sink;

                sink = new InputTestConsumer();
                if (sink) {
                    NS_ADDREF(sink);
                    rv = sink->QueryInterface(eventSinkIID, (void**)result);
                    NS_RELEASE(sink);
                } else {
                    rv = NS_ERROR_OUT_OF_MEMORY;
                }
            }
        }
        return rv;
    }
};

NS_IMPL_ISUPPORTS(nsEventSinkGetter, nsIEventSinkGetter::GetIID());

////////////////////////////////////////////////////////////////////////////////

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

    //Create the nsINetService...
    NS_WITH_SERVICE(nsIIOService, pService, kIOServiceCID, &rv);

    if (NS_FAILED(rv)) return rv;

    // The story of how to retrieve a document. 
  	// Now available in 2 different flavours.

    if (pService)
    {

        nsCOMPtr<nsIURI> pURL;

        if (NS_OK == pService->NewURI(argv[1], nsnull, getter_AddRefs(pURL)))
        {
            if (pURL)
            {
                nsCOMPtr<nsIChannel> pConnection;
                /* Flavour Two */
                nsEventSinkGetter* pMyConsumer = new nsEventSinkGetter();
                if (!pMyConsumer)
                {
                    NS_ERROR("Failed to create a new consumer!");
                    return -1;
                }
                // Async reading thru the calls of the event sink interface
                if (NS_OK == pService->NewChannelFromURI("load", pURL, pMyConsumer, 
                                                         getter_AddRefs(pConnection)))
                {
                    if (pConnection)
                    {
                        /* 
                            You may optionally add/set other headers on this
                            request object. This is done by QI for the specific
                            protocolConnection.
                        */
                        nsCOMPtr<nsIHTTPChannel> pHTTPCon(do_QueryInterface(pConnection));

                        if (pHTTPCon)
                        {
                            // Setting a sample user agent string.
                            if (NS_OK == pHTTPCon->SetRequestHeader("User-Agent", "Mozilla/5.0 [en] (Win98; U)"))
                            {
                            }
                        }
                    
                        // But calling the open is required!
//                        pConnection->Open();
                    }
                } else {
                    printf("NewChannelFromURI failed!\n");
                }
            }
        } else {
            printf("NewURI failed!\n");
        }
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
