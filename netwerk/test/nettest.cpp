/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include <assert.h>

#ifdef WIN32
#include <windows.h>
#endif

#include "plstr.h"
#include "nsIEventQueue.h"

#include "nsIProtocolConnection.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIUrl.h"
#include "nsINetService.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsString2.h"


int urlLoaded;
PRBool bTraceEnabled;
PRBool bLoadAsync;

//#include "nsIPostToServer.h"
#include "nsINetService.h"

// Define CIDs...
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kEventQueueCID, NS_EVENTQUEUE_CID);

// Define IIDs...
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);
//NS_DEFINE_IID(kIPostToServerIID, NS_IPOSTTOSERVER_IID);


class TestConsumer : public nsIStreamListener
{

public:
    NS_DECL_ISUPPORTS
    
    TestConsumer();

	// stream listener methods
    NS_DECL_NSISTREAMLISTENER

	// stream observer methods
    NS_DECL_NSISTREAMOBSERVER

protected:
    ~TestConsumer();
	nsIUrl* mUrl;
};


TestConsumer::TestConsumer()
{
    NS_INIT_REFCNT();
}


NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);
NS_IMPL_ISUPPORTS(TestConsumer,kIStreamListenerIID);


TestConsumer::~TestConsumer()
{
    if (bTraceEnabled) {
        printf("\n+++ TestConsumer is being deleted...\n");
    }
}

NS_IMETHODIMP TestConsumer::OnDataAvailable(nsISupports *context,
                                            nsIInputStream* aIStream, 
                                            PRUint32 aSourceOffset,
                                            PRUint32 aLength) {
    PRUint32 len;

    if (bTraceEnabled) {
        printf("\n+++ TestConsumer::OnDataAvailable: URL: %p, %d bytes available...\n", mUrl, aLength);
    }

    do {
        nsresult err;
        char buffer[80];
        PRUint32 i;

        err = aIStream->Read(buffer, 80, &len);
        if (err == NS_OK) {
            for (i=0; i<len; i++) {
                putchar(buffer[i]);
            }
        }
    } while (len > 0);

    return 0;
}

NS_IMETHODIMP TestConsumer::OnStartRequest(nsIChannel* channel, nsISupports *context) {
    if (bTraceEnabled) {
        printf("\n+++ TestConsumer::OnStartRequest: URL: %p, Content type: %s\n", mUrl, "XXX some context");
    }

    return 0;
}

NS_IMETHODIMP TestConsumer::OnStopRequest(nsISupports *context, nsIChannel* channel,
                                          nsresult aStatus, nsIString *aMsg) {
    if (bTraceEnabled) {
        printf("\n+++ TestConsumer::OnStopRequest... URL: %p status: %d\n", mUrl, aStatus);
    }

    if (NS_FAILED(aStatus)) {
      char* url;
      mUrl->ToNewCString(&url);

      printf("Unable to load URL %s\n", url);
      nsAllocator::Free(url);
	}
	
    urlLoaded = 1;
    return 0;
}

nsresult ReadStreamSynchronously(nsIInputStream* aIn)
{
    nsresult rv;
    char buffer[1024];

    if (nsnull != aIn) {
        PRUint32 len;

        do {
            PRUint32 i;

            rv = aIn->Read(buffer, sizeof(buffer), &len);
            for (i=0; i<len; i++) {
                putchar(buffer[i]);
            }
        } while (len > 0);
    }
    return NS_OK;
}



int main(int argc, char **argv)
{
    nsAutoString2 url_address;
    char buf[256];
    nsIStreamListener *pConsumer;
    nsIEventQueueService* pEventQService;
    nsIUrl *pURL;
    nsresult result;
    int i;

    if (argc < 2) {
        printf("test: [-trace] [-sync] <URL>\n");
        return 0;
    }

    result = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup,
                                              "components");
    if (NS_FAILED(result)) return result;

    // Create the Event Queue for this thread...
    pEventQService = nsnull;
    result = nsServiceManager::GetService(kEventQueueServiceCID,
                                          kIEventQueueServiceIID,
                                          (nsISupports **)&pEventQService);
    if (NS_SUCCEEDED(result)) {
      // XXX: What if this fails?
      result = pEventQService->CreateThreadEventQueue();
    }

    bTraceEnabled = PR_FALSE;
    bLoadAsync    = PR_TRUE;

    for (i=1; i < argc; i++) {
        // Turn on netlib tracing...
        if (PL_strcasecmp(argv[i], "-trace") == 0) {
            //NET_ToggleTrace();
            bTraceEnabled = PR_TRUE;
            continue;
        } 
        // Turn on synchronous URL loading...
        if (PL_strcasecmp(argv[i], "-sync") == 0) {
            bLoadAsync = PR_FALSE;
            continue;
        }

        urlLoaded = 0;

        url_address = argv[i];
        if (bTraceEnabled) {
            url_address.ToCString(buf, 256);
            printf("+++ loading URL: %s...\n", buf);
        }

        pConsumer = new TestConsumer;
        pConsumer->AddRef();
        
        // Create the URL object...
        pURL = NULL;
#if 1
#ifndef NECKO
        result = NS_NewURL(&pURL, argv[i], nsnull);
#else
        NS_WITH_SERVICE(nsIIOService, service, kIOServiceCID, &result);
        if (NS_FAILED(result)) return result;

        nsIURI *uri = nsnull;
        = service->NewURI(argv[i], nsnull, &uri);
        if (NS_FAILED(result)) return result;

        result = uri->QueryInterface(nsCOMTypeInfo<nsIURL>::GetIID(), (void**)&pURL);
        NS_RELEASE(uri);
        if (NS_FAILED(result)) return result;
#endif // NECKO
        if (NS_OK != result) {
            if (bTraceEnabled) {
                printf("NS_NewURL() failed...\n");
            }
            return 1;
        }
#else
		return 1;
#endif // 0

#if 0
        nsIPostToServer *pPoster;
        result = pURL->QueryInterface(kIPostToServerIID, (void**)&pPoster);
        if (result == NS_OK) {
            pPoster->SendFile("foo.txt");
        }
        NS_IF_RELEASE(pPoster);
#endif
        

#if 0
        // Start the URL load...
        if (PR_TRUE == bLoadAsync) {
            result = NS_OpenURL(pURL, pConsumer);

            /* If the open failed, then do not drop into the message loop... */
            if (NS_OK != result) {
                char* url;
                pURL->ToNewCString(&url);

                printf("Unable to load URL %s\n", url);
				nsAllocator::Free(url);
                urlLoaded = 1;
            }
        } 
        // Load the URL synchronously...
        else {
            nsIInputStream *in;

            result = NS_OpenURL(pURL, &in);
            ReadStreamSynchronously(in);
            NS_IF_RELEASE(in);
            urlLoaded = 1;
        }
#else

		nsresult rv;
		nsIEventQueue* eventQ = nsnull;
		NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
		if (NS_SUCCEEDED(rv)) {
		  rv = eventQService->GetThreadEventQueue(PR_CurrentThread(), &eventQ);
		}
		if (NS_FAILED(rv)) return rv;

        // Start the URL load...
        nsIProtocolConnection* protoConn = nsnull;
        result = NS_NewConnection(pURL, 
            // nsISupports* eventSink,
            eventQ,
            // nsIConnectionGroup* group,
            nsnull,
            // nsIProtocolConnection* *result
            &protoConn);

        if (NS_FAILED(result)) return 1;

        protoConn->Open();

#endif // 0
        
        
        // Enter the message pump to allow the URL load to proceed.
        while ( !urlLoaded ) {
#ifdef XP_PC
            MSG msg;

            if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
#endif
        }
        
        pURL->Release();
    }
    
    if (nsnull != pEventQService) {
        pEventQService->DestroyThreadEventQueue();
        nsServiceManager::ReleaseService(kEventQueueServiceCID, pEventQService);
    }
    return 0;
}
