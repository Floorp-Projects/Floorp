/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIInputStream.h"
#include "nsCRT.h"
#include "nsIChannel.h"
#include "nsIURL.h"
#include "nsIHTTPChannel.h"
#include "nsIHTTPEventSink.h" 
#include "nsIInterfaceRequestor.h" 
#include "nsIDNSService.h" 

#include "nsISimpleEnumerator.h"
#include "nsIHTTPHeader.h"
#include "nsXPIDLString.h"
#include "nsNetUtil.h"

// this test app handles cookies.
#include "nsICookieService.h"
static NS_DEFINE_CID(nsCookieServiceCID, NS_COOKIESERVICE_CID);

static NS_DEFINE_CID(kEventQueueServiceCID,      NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kIOServiceCID,              NS_IOSERVICE_CID);

//static PRTime gElapsedTime; // enable when we time it...
static int gKeepRunning = 0;
static PRBool gVerbose = PR_FALSE;
static nsIEventQueue* gEventQ = nsnull;

class URLLoadInfo : public nsISupports
{
public:

  URLLoadInfo(const char* aUrl);
  virtual ~URLLoadInfo();

  // ISupports interface...
  NS_DECL_ISUPPORTS

  const char* Name() { return mURLString.GetBuffer(); }
  PRInt32   mBytesRead;
  PRTime    mTotalTime;
  PRTime    mConnectTime;
  nsCString mURLString;
};

URLLoadInfo::URLLoadInfo(const char *aUrl) : mURLString(aUrl)
{
  NS_INIT_REFCNT();

  mBytesRead = 0;
  mConnectTime = mTotalTime = PR_Now();
}

URLLoadInfo::~URLLoadInfo()
{
}


NS_IMPL_THREADSAFE_ISUPPORTS(URLLoadInfo,NS_GET_IID(nsISupports));


class TestHTTPEventSink : public nsIHTTPEventSink
{
public:

  TestHTTPEventSink();
  virtual ~TestHTTPEventSink();

  // ISupports interface...
  NS_DECL_ISUPPORTS

  // nsIHTTPEventSink interface...

  NS_IMETHOD      OnHeadersAvailable(nsISupports* i_Context);

  // OnRedirect gets fired only if you have set FollowRedirects on the handler!
  NS_IMETHOD      OnRedirect(nsISupports* i_Context, 
                             nsIURI* i_NewLocation);
};

TestHTTPEventSink::TestHTTPEventSink()
{
  NS_INIT_REFCNT();
}

TestHTTPEventSink::~TestHTTPEventSink()
{
}


NS_IMPL_ISUPPORTS(TestHTTPEventSink,NS_GET_IID(nsIHTTPEventSink));

NS_IMETHODIMP
TestHTTPEventSink::OnHeadersAvailable(nsISupports* context)
{
    nsCOMPtr<nsISimpleEnumerator> enumerator;
    nsCOMPtr<nsIHTTPChannel> pHTTPCon(do_QueryInterface(context));
    PRBool bMoreHeaders;

    if (pHTTPCon) {
        nsXPIDLCString value;

        printf("Channel Info:\n");
        pHTTPCon->GetContentType(getter_Copies(value));
        printf("\tContent-Type: %s\n", (const char*)value);

        pHTTPCon->GetCharset(getter_Copies(value));
        printf("\tCharset: %s\n", (const char*)value);

        pHTTPCon->GetRequestHeaderEnumerator(getter_AddRefs(enumerator));

        printf("Request headers:\n");
        enumerator->HasMoreElements(&bMoreHeaders);
        while (bMoreHeaders) {
            nsCOMPtr<nsISupports> item;
            nsCOMPtr<nsIHTTPHeader> header;

            enumerator->GetNext(getter_AddRefs(item));
            header = do_QueryInterface(item);

            if (header) {
                nsCOMPtr<nsIAtom> key;
                nsAutoString field;
                header->GetField(getter_AddRefs(key));
                key->ToString(field);
                nsCAutoString theField;
		theField.AssignWithConversion(field);
                printf("\t%s: ", theField.GetBuffer());

                header->GetValue(getter_Copies(value));
                printf("%s\n", (const char*)value);
            }
    
            enumerator->HasMoreElements(&bMoreHeaders);
        }

        pHTTPCon->GetResponseHeaderEnumerator(getter_AddRefs(enumerator));

        printf("Response headers:\n");
        enumerator->HasMoreElements(&bMoreHeaders);
        while (bMoreHeaders) {
            nsCOMPtr<nsISupports> item;
            nsCOMPtr<nsIHTTPHeader> header;

            enumerator->GetNext(getter_AddRefs(item));
            header = do_QueryInterface(item);

            if (header) {
                nsCOMPtr<nsIAtom> key;
                nsAutoString field;

                header->GetField(getter_AddRefs(key));
                key->ToString(field);
                nsCAutoString theField;
		theField.AssignWithConversion(field);
                printf("\t%s: ", theField.GetBuffer());

                header->GetValue(getter_Copies(value));
                printf("%s\n", (const char*)value);
            }
    
            enumerator->HasMoreElements(&bMoreHeaders);
        }

    }


    if (gVerbose) {
        printf("\n+++ TestHTTPEventSink::OnHeadersAvailable +++\n");
        if (pHTTPCon) {
            char* type;
            //optimize later TODO allow atoms here...! intead of just the header strings
            pHTTPCon->GetContentType(&type);
            if (type) {
                printf("\nReceiving ... %s\n", type);
                nsCRT::free(type);
            }
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
TestHTTPEventSink::OnRedirect(nsISupports* context, nsIURI* i_NewLocation)
{
    printf("\n+++ TestHTTPEventSink::OnRedirect +++\n");
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

class InputTestConsumer : public nsIStreamListener
{
public:

  InputTestConsumer();
  virtual ~InputTestConsumer();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMOBSERVER
  NS_DECL_NSISTREAMLISTENER
};

////////////////////////////////////////////////////////////////////////////////

class OpenObserver : public nsIStreamObserver 
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMOBSERVER

    OpenObserver(InputTestConsumer* cons)
        : mInputConsumer(cons) { 
        NS_INIT_REFCNT();
    }
    virtual ~OpenObserver() {}

protected:
    InputTestConsumer* mInputConsumer;
};

NS_IMPL_ISUPPORTS1(OpenObserver, nsIStreamObserver);

NS_IMETHODIMP
OpenObserver::OnStartRequest(nsIChannel* channel, nsISupports* context)
{
    printf("\n+++ OpenObserver::OnStartRequest +++. Context = %p\n", context);

    char* type;
    PRInt32 length = -1;
    nsresult rv;
    rv = channel->GetContentType(&type);
    NS_ASSERTION(NS_SUCCEEDED(rv), "GetContentType failed");
    rv = channel->GetContentLength(&length);
    NS_ASSERTION(NS_SUCCEEDED(rv), "GetContentLength failed");
    printf("    contentType = %s length = %d\n", type, length);
    nsCRT::free(type);

    return NS_OK;
}

NS_IMETHODIMP
OpenObserver::OnStopRequest(nsIChannel* channel, nsISupports* context,
                            nsresult aStatus, const PRUnichar* aStatusArg)
{
    printf("\n+++ OpenObserver::OnStopRequest (status = %x) +++."
           "\tContext = %p\n", 
           aStatus, context);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

InputTestConsumer::InputTestConsumer()
{
  NS_INIT_REFCNT();
}

InputTestConsumer::~InputTestConsumer()
{
}


NS_IMPL_ISUPPORTS(InputTestConsumer,NS_GET_IID(nsIStreamListener));


NS_IMETHODIMP
InputTestConsumer::OnStartRequest(nsIChannel* channel, nsISupports* context)
{
  URLLoadInfo* info = (URLLoadInfo*)context;
  if (info) {
    info->mConnectTime = PR_Now() - info->mConnectTime;
  }

  if (gVerbose) {
    printf("\nStarted loading: %s\n", info ? info->Name() : "UNKNOWN URL");
  }
/*
  nsCOMPtr<nsIURI> pURI(do_QueryInterface(context));
  char* location = nsnull;

  if (pURI) {
    pURI->GetSpec(&location);
  }

  printf("\nStarted loading: %s\n", location ? location : "UNKNOWN URL");
  if (location) {
    nsCRT::free(location);
  }
*/
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
  PRUint32 amt, size;
  nsresult rv;
  URLLoadInfo* info = (URLLoadInfo*)context;

  while (aLength) {
    size = PR_MIN(aLength, sizeof(buf));

    rv = aIStream->Read(buf, size, &amt);
    if (NS_FAILED(rv)) {
      NS_ASSERTION((NS_BASE_STREAM_WOULD_BLOCK != rv), 
                   "The stream should never block.");
      return rv;
    }
    if (gVerbose) {
      buf[amt] = '\0';
      puts(buf);
    }
    if (info) {
      info->mBytesRead += amt;
    }

    aLength -= amt;
  }
  return NS_OK;
}


NS_IMETHODIMP
InputTestConsumer::OnStopRequest(nsIChannel* channel, nsISupports* context,
                                 nsresult aStatus, const PRUnichar* aStatusArg)
{
  URLLoadInfo* info = (URLLoadInfo*)context;

  if (info) {
    double connectTime;
    double readTime;
    PRUint32 httpStatus;
    PRBool bHTTPURL = PR_FALSE;

    info->mTotalTime = PR_Now() - info->mTotalTime;

    connectTime = (info->mConnectTime/1000.0)/1000.0;
    readTime    = ((info->mTotalTime-info->mConnectTime)/1000.0)/1000.0;

    nsCOMPtr<nsIHTTPChannel> pHTTPCon(do_QueryInterface(channel));
    if (pHTTPCon) {
        pHTTPCon->GetResponseStatus(&httpStatus);
        bHTTPURL = PR_TRUE;
    }

    printf("\nFinished loading: %s  Status Code: %x\n", info->Name(), aStatus);
    if (bHTTPURL)
        printf("\tHTTP Status: %u\n", httpStatus);
     if (NS_ERROR_UNKNOWN_HOST == aStatus) {
         printf("\tDNS lookup failed.\n");
     }
    printf("\tRead: %d bytes.\n", info->mBytesRead);
    printf("\tTime to connect: %.3f seconds\n", connectTime);
    printf("\tTime to read: %.3f seconds.\n", readTime);
    if (readTime > 0.0) {
      printf("\tThroughput: %.0f bps.\n", (info->mBytesRead*8)/readTime);
    } else {
      printf("\tThroughput: REAL FAST!!\n");
    }
  } else {
    printf("\nFinished loading: UNKNOWN URL. Status Code: %x\n", aStatus);
  }
/*
  nsCOMPtr<nsIURI> pURI(do_QueryInterface(context));
  char* location = nsnull;

  if (pURI) {
    pURI->GetSpec(&location);
  }

  printf("\nFinished loading: %s  Status Code: %x\n", location ? location : "UNKNOWN URL", status);

  if (location) {
    nsCRT::free(location);
  }
*/
  gKeepRunning -= 1;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

class nsNotificationCallbacks : public nsIInterfaceRequestor {
public:
    NS_DECL_ISUPPORTS

    nsNotificationCallbacks() {
        NS_INIT_REFCNT();
    }

    NS_IMETHOD GetInterface(const nsIID& eventSinkIID, void* *result) {
        nsresult rv = NS_ERROR_FAILURE;

        if (eventSinkIID.Equals(NS_GET_IID(nsIHTTPEventSink))) {
          TestHTTPEventSink *sink;

          sink = new TestHTTPEventSink();
          if (sink == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
          NS_ADDREF(sink);
          rv = sink->QueryInterface(eventSinkIID, result);
          NS_RELEASE(sink);
        }
        return rv;
    }
};

NS_IMPL_ISUPPORTS1(nsNotificationCallbacks, nsIInterfaceRequestor)

////////////////////////////////////////////////////////////////////////////////

nsresult StartLoadingURL(const char* aUrlString)
{
    nsresult rv;

    NS_WITH_SERVICE(nsIIOService, pService, kIOServiceCID, &rv);
    if (pService) {
        nsCOMPtr<nsIURI> pURL;

        rv = pService->NewURI(aUrlString, nsnull, getter_AddRefs(pURL));
        if (NS_FAILED(rv)) {
            printf("ERROR: NewURI failed for %s\n", aUrlString);
            return rv;
        }
        nsCOMPtr<nsIChannel> pChannel;

        nsNotificationCallbacks* callbacks = new nsNotificationCallbacks();
        if (!callbacks) {
            NS_ERROR("Failed to create a new consumer!");
            return NS_ERROR_OUT_OF_MEMORY;;
        }
        NS_ADDREF(callbacks);

        // Async reading thru the calls of the event sink interface
        rv = NS_OpenURI(getter_AddRefs(pChannel), pURL, pService,
                        nsnull,     // loadGroup
                        callbacks); // notificationCallbacks
        NS_RELEASE(callbacks);
        if (NS_FAILED(rv)) {
            printf("ERROR: NS_OpenURI failed for %s\n", aUrlString);
            return rv;
        }

        /* 
           You may optionally add/set other headers on this
           request object. This is done by QI for the specific
           protocolConnection.
        */
        nsCOMPtr<nsIHTTPChannel> pHTTPCon(do_QueryInterface(pChannel));

        if (pHTTPCon) {
            // Setting a sample header.
            nsCOMPtr<nsIAtom> sampleHeader;

            sampleHeader = NS_NewAtom("sample-header-minus-the-colon");
            rv = pHTTPCon->SetRequestHeader(sampleHeader, "Sample-Value");
            if (NS_FAILED(rv)) return rv;
        }            
        InputTestConsumer* listener;

        listener = new InputTestConsumer;
        NS_IF_ADDREF(listener);
        if (!listener) {
            NS_ERROR("Failed to create a new stream listener!");
            return NS_ERROR_OUT_OF_MEMORY;;
        }

        URLLoadInfo* info;
        info = new URLLoadInfo(aUrlString);
        NS_IF_ADDREF(info);
        if (!info) {
            NS_ERROR("Failed to create a load info!");
            return NS_ERROR_OUT_OF_MEMORY;
        }
        

        rv = pChannel->AsyncRead(listener,  // IStreamListener consumer
                                 info);     // ISupports context

        if (NS_SUCCEEDED(rv)) {
            gKeepRunning += 1;
        }
        NS_RELEASE(listener);
        NS_RELEASE(info);
    }

    return rv;
}


nsresult LoadURLsFromFile(char *aFileName)
{
    nsresult rv = NS_OK;
    PRInt32 len, offset;
    PRFileDesc* fd;
    char buffer[1024];
    nsCString fileBuffer;
    nsCAutoString urlString;

    fd = PR_Open(aFileName, PR_RDONLY, 777);
    if (!fd) {
        return NS_ERROR_FAILURE;
    }

    // Keep reading the file until EOF (or an error) is reached...        
    do {
        len = PR_Read(fd, buffer, sizeof(buffer));
        if (len>0) {
            fileBuffer.Append(buffer, len);
            // Treat each line as a URL...
            while ((offset = fileBuffer.FindChar('\n')) != -1) {
                fileBuffer.Left(urlString, offset);
                fileBuffer.Cut(0, offset+1);

                urlString.StripChars("\r");
                if (urlString.Length()) {
                    printf("\t%s\n", urlString.GetBuffer());
                    rv = StartLoadingURL(urlString.GetBuffer());
                }
            }
        }
    } while (len>0);

    // If anything is left in the fileBuffer, treat it as a URL...
    fileBuffer.StripChars("\r");
    if (fileBuffer.Length()) {
        printf("\t%s\n", fileBuffer.GetBuffer());
        StartLoadingURL(fileBuffer.GetBuffer());
    }
    
    PR_Close(fd);
    return NS_OK;
}

int
main(int argc, char* argv[])
{
    nsresult rv= (nsresult)-1;
    if (argc < 2) {
        printf("usage: %s [-verbose] [-file <name>] <url> <url> ... \n", argv[0]);
        return -1;
    }

    /* 
      The following code only deals with XPCOM registration stuff. and setting
      up the event queues. Copied from TestSocketIO.cpp
    */

    rv = NS_InitXPCOM(nsnull, nsnull);
    if (NS_FAILED(rv)) return rv;

    
    // Create the Event Queue for this thread...
    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = eventQService->CreateThreadEventQueue();
    if (NS_FAILED(rv)) return rv;

    eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &gEventQ);

#if 0 // Jud sez 
    // fire up an instance of the cookie manager.
    // I'm doing this using the serviceManager for convenience's sake.
    // Presumably an application will init it's own cookie service a 
    // different way (this way works too though).
    NS_WITH_SERVICE(nsICookieService, cookieService, nsCookieServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
#endif // NECKO

    int i;
    printf("\nTrying to load:\n");
    for (i=1; i<argc; i++) {
        // Turn on verbose printing...
        if (PL_strcasecmp(argv[i], "-verbose") == 0) {
            gVerbose = PR_TRUE;
            continue;
        } 

        // Turn on netlib tracing...
        if (PL_strcasecmp(argv[i], "-file") == 0) {
            LoadURLsFromFile(argv[++i]);
            continue;
        } 

        printf("\t%s\n", argv[i]);
        rv = StartLoadingURL(argv[i]);
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

    NS_ShutdownXPCOM(nsnull);
    return rv;
}
