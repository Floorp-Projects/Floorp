/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsIHttpChannel.h"
#include "nsIHttpEventSink.h" 
#include "nsIInterfaceRequestor.h" 
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDNSService.h" 

#include "nsISimpleEnumerator.h"
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
static PRBool gAskUserForInput = PR_FALSE;

//-----------------------------------------------------------------------------
// HeaderVisitor
//-----------------------------------------------------------------------------

class HeaderVisitor : public nsIHttpHeaderVisitor
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHTTPHEADERVISITOR

  HeaderVisitor() { NS_INIT_ISUPPORTS(); }
  virtual ~HeaderVisitor() {}
};
NS_IMPL_ISUPPORTS1(HeaderVisitor, nsIHttpHeaderVisitor)

NS_IMETHODIMP
HeaderVisitor::VisitHeader(const char *header, const char *value)
{
  printf("%s: %s\n", header, value);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// URLLoadInfo
//-----------------------------------------------------------------------------

class URLLoadInfo : public nsISupports
{
public:

  URLLoadInfo(const char* aUrl);
  virtual ~URLLoadInfo();

  // ISupports interface...
  NS_DECL_ISUPPORTS

  const char* Name() { return mURLString.get(); }
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


NS_IMPL_THREADSAFE_ISUPPORTS0(URLLoadInfo)

//-----------------------------------------------------------------------------
// TestHttpEventSink
//-----------------------------------------------------------------------------

class TestHttpEventSink : public nsIHttpEventSink
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHTTPEVENTSINK

  TestHttpEventSink();
  virtual ~TestHttpEventSink();
};

TestHttpEventSink::TestHttpEventSink()
{
  NS_INIT_REFCNT();
}

TestHttpEventSink::~TestHttpEventSink()
{
}


NS_IMPL_ISUPPORTS1(TestHttpEventSink, nsIHttpEventSink);

#if 0
NS_IMETHODIMP
TestHTTPEventSink::OnHeadersAvailable(nsISupports* context)
{
    return NS_OK;
}
#endif

NS_IMETHODIMP
TestHttpEventSink::OnRedirect(nsIHttpChannel *channel, nsIChannel *newChannel)
{
    printf("\n+++ TestHTTPEventSink::OnRedirect +++\n");
    return NS_OK;
}

//-----------------------------------------------------------------------------
// InputTestConsumer
//-----------------------------------------------------------------------------

class InputTestConsumer : public nsIStreamListener
{
public:

  InputTestConsumer();
  virtual ~InputTestConsumer();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
};

InputTestConsumer::InputTestConsumer()
{
  NS_INIT_REFCNT();
}

InputTestConsumer::~InputTestConsumer()
{
}

NS_IMPL_ISUPPORTS1(InputTestConsumer, nsIStreamListener)

NS_IMETHODIMP
InputTestConsumer::OnStartRequest(nsIRequest *request, nsISupports* context)
{
  URLLoadInfo* info = (URLLoadInfo*)context;
  if (info)
    info->mConnectTime = PR_Now() - info->mConnectTime;

  if (gVerbose)
    printf("\nStarted loading: %s\n", info ? info->Name() : "UNKNOWN URL");

  nsXPIDLCString value;

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  if (channel) {

    printf("Channel Info:\n");
    channel->GetContentType(getter_Copies(value));
    printf("\tContent-Type: %s\n", (const char*)value);

    PRInt32 length = -1;
    channel->GetContentLength(&length);
    printf("\tContent-Length: %d\n", length);
  }

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(request));
  if (httpChannel) {
    HeaderVisitor *visitor = new HeaderVisitor();
    if (!visitor)
      return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(visitor);

    httpChannel->GetCharset(getter_Copies(value));
    printf("\tCharset: %s\n", (const char*)value);

    printf("\nHTTP request headers:\n");
    httpChannel->VisitRequestHeaders(visitor);

    printf("\nHTTP response headers:\n");
    httpChannel->VisitResponseHeaders(visitor);

    NS_RELEASE(visitor);
  }
  return NS_OK;
}

NS_IMETHODIMP
InputTestConsumer::OnDataAvailable(nsIRequest *request, 
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

PR_STATIC_CALLBACK(void) DecrementDestroyHandler(PLEvent *self) 
{
    PR_DELETE(self);
}


PR_STATIC_CALLBACK(void*) DecrementEventHandler(PLEvent *self) 
{
    gKeepRunning--;
    return nsnull;
}

void FireDecrement()
{
    PLEvent *event = PR_NEW(PLEvent);
    PL_InitEvent(event, 
               nsnull,
               DecrementEventHandler,
               DecrementDestroyHandler);

    gEventQ->PostEvent(event);
}

NS_IMETHODIMP
InputTestConsumer::OnStopRequest(nsIRequest *request, nsISupports* context,
                                 nsresult aStatus)
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

    nsCOMPtr<nsIHttpChannel> pHTTPCon(do_QueryInterface(request));
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

  FireDecrement();
  return NS_OK;
}

//-----------------------------------------------------------------------------
// NotificationCallbacks
//-----------------------------------------------------------------------------

class NotificationCallbacks : public nsIInterfaceRequestor {
public:
    NS_DECL_ISUPPORTS

    NotificationCallbacks() {
        NS_INIT_REFCNT();
    }

    NS_IMETHOD GetInterface(const nsIID& eventSinkIID, void* *result) {
        nsresult rv = NS_ERROR_FAILURE;

        if (eventSinkIID.Equals(NS_GET_IID(nsIHttpEventSink))) {
          TestHttpEventSink *sink;

          sink = new TestHttpEventSink();
          if (sink == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
          NS_ADDREF(sink);
          rv = sink->QueryInterface(eventSinkIID, result);
          NS_RELEASE(sink);
        }
        return rv;
    }
};

NS_IMPL_ISUPPORTS1(NotificationCallbacks, nsIInterfaceRequestor)

//-----------------------------------------------------------------------------
// helpers...
//-----------------------------------------------------------------------------

nsresult StartLoadingURL(const char* aUrlString)
{
    nsresult rv;

    nsCOMPtr<nsIIOService> pService(do_GetService(kIOServiceCID, &rv));
    if (pService) {
        nsCOMPtr<nsIURI> pURL;

        rv = pService->NewURI(aUrlString, nsnull, getter_AddRefs(pURL));
        if (NS_FAILED(rv)) {
            printf("ERROR: NewURI failed for %s\n", aUrlString);
            return rv;
        }
        nsCOMPtr<nsIChannel> pChannel;

        NotificationCallbacks* callbacks = new NotificationCallbacks();
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
        nsCOMPtr<nsIHttpChannel> pHTTPCon(do_QueryInterface(pChannel));

        if (pHTTPCon) {
            // Setting a sample header.
            rv = pHTTPCon->SetRequestHeader("sample-header", "Sample-Value");
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
        
        rv = pChannel->AsyncOpen(listener,  // IStreamListener consumer
                                 info);

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
                    printf("\t%s\n", urlString.get());
                    rv = StartLoadingURL(urlString.get());
                }
            }
        }
    } while (len>0);

    // If anything is left in the fileBuffer, treat it as a URL...
    fileBuffer.StripChars("\r");
    if (fileBuffer.Length()) {
        printf("\t%s\n", fileBuffer.get());
        StartLoadingURL(fileBuffer.get());
    }
    
    PR_Close(fd);
    return NS_OK;
}


nsresult LoadURLFromConsole()
{
    char buffer[1024];
    printf("Enter URL (\"q\" to start): ");
    scanf("%s", buffer);
    if (buffer[0]=='q') 
        gAskUserForInput = PR_FALSE;
    else
        StartLoadingURL(buffer);
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
    nsCOMPtr<nsIEventQueueService> eventQService = 
             do_GetService(kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = eventQService->CreateThreadEventQueue();
    if (NS_FAILED(rv)) return rv;

    eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &gEventQ);

#if 0 // Jud sez 
    // fire up an instance of the cookie manager.
    // I'm doing this using the serviceManager for convenience's sake.
    // Presumably an application will init it's own cookie service a 
    // different way (this way works too though).
    nsCOMPtr<nsICookieService> cookieService = 
             do_GetService(nsCookieServiceCID, &rv);
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
        
        if (PL_strcasecmp(argv[i], "-console") == 0) {
            gAskUserForInput = PR_TRUE;
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
    rv = gEventQ->WaitForEvent(&gEvent);
    rv = gEventQ->HandleEvent(gEvent);
#endif /* XP_UNIX */
#endif /* !WIN32 */
    }
    NS_ShutdownXPCOM(nsnull);
    return rv;
}
