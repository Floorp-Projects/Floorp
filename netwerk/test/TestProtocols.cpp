/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 sw=4 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* 
    The TestProtocols tests the basic protocols architecture and can 
    be used to test individual protocols as well. If this grows too
    big then we should split it to individual protocols. 

    -Gagan Saksena 04/29/99
*/

#include "TestCommon.h"

#define FORCE_PR_LOG
#include <stdio.h>
#ifdef WIN32 
#include <windows.h>
#endif
#ifdef XP_UNIX
#include <unistd.h>
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
#include "nsIResumableChannel.h"
#include "nsIURL.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsIHttpEventSink.h" 
#include "nsIInterfaceRequestor.h" 
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDNSService.h" 
#include "nsIAuthPrompt.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

#include "nsISimpleEnumerator.h"
#include "nsXPIDLString.h"
#include "nsNetUtil.h"
#include "prlog.h"
#include "prtime.h"

#include "nsInt64.h"

#if defined(PR_LOGGING)
//
// set NSPR_LOG_MODULES=Test:5
//
static PRLogModuleInfo *gTestLog = nsnull;
#endif
#define LOG(args) PR_LOG(gTestLog, PR_LOG_DEBUG, args)

static NS_DEFINE_CID(kEventQueueServiceCID,      NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kIOServiceCID,              NS_IOSERVICE_CID);

//static PRTime gElapsedTime; // enable when we time it...
static int gKeepRunning = 0;
static PRBool gVerbose = PR_FALSE;
static nsIEventQueue* gEventQ = nsnull;
static PRBool gAskUserForInput = PR_FALSE;
static PRBool gResume = PR_FALSE;
static PRUint64 gStartAt = 0;

static const char* gEntityID;

//-----------------------------------------------------------------------------
// Set proxy preferences for testing
//-----------------------------------------------------------------------------

static nsresult
SetHttpProxy(const char *proxy)
{
  const char *colon = strchr(proxy, ':');
  if (!colon)
  {
    NS_WARNING("invalid proxy token; use host:port");
    return NS_ERROR_UNEXPECTED;
  }
  int port = atoi(colon + 1);
  if (port == 0)
  {
    NS_WARNING("invalid proxy port; must be an integer");
    return NS_ERROR_UNEXPECTED;
  }
  nsCAutoString proxyHost;
  proxyHost = Substring(proxy, colon);

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs)
  {
    prefs->SetCharPref("network.proxy.http", proxyHost.get());
    prefs->SetIntPref("network.proxy.http_port", port);
    prefs->SetIntPref("network.proxy.type", 1); // manual proxy config
  }
  LOG(("connecting via proxy=%s:%d\n", proxyHost.get(), port));
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HeaderVisitor
//-----------------------------------------------------------------------------

class HeaderVisitor : public nsIHttpHeaderVisitor
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHTTPHEADERVISITOR

  HeaderVisitor() { }
  virtual ~HeaderVisitor() {}
};
NS_IMPL_ISUPPORTS1(HeaderVisitor, nsIHttpHeaderVisitor)

NS_IMETHODIMP
HeaderVisitor::VisitHeader(const nsACString &header, const nsACString &value)
{
  LOG(("  %s: %s\n",
    PromiseFlatCString(header).get(),
    PromiseFlatCString(value).get()));
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
  nsInt64   mBytesRead;
  PRTime    mTotalTime;
  PRTime    mConnectTime;
  nsCString mURLString;
};

URLLoadInfo::URLLoadInfo(const char *aUrl) : mURLString(aUrl)
{
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
}

TestHttpEventSink::~TestHttpEventSink()
{
}


NS_IMPL_ISUPPORTS1(TestHttpEventSink, nsIHttpEventSink)

NS_IMETHODIMP
TestHttpEventSink::OnRedirect(nsIHttpChannel *channel, nsIChannel *newChannel)
{
    LOG(("\n+++ TestHTTPEventSink::OnRedirect +++\n"));
    return NS_OK;
}

//-----------------------------------------------------------------------------
// TestAuthPrompt
//-----------------------------------------------------------------------------

class TestAuthPrompt : public nsIAuthPrompt
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIAUTHPROMPT

  TestAuthPrompt();
  virtual ~TestAuthPrompt();
};

NS_IMPL_ISUPPORTS1(TestAuthPrompt, nsIAuthPrompt)

TestAuthPrompt::TestAuthPrompt()
{
}

TestAuthPrompt::~TestAuthPrompt()
{
}

NS_IMETHODIMP
TestAuthPrompt::Prompt(const PRUnichar *dialogTitle,
                       const PRUnichar *text,
                       const PRUnichar *passwordRealm,
                       PRUint32 savePassword,
                       const PRUnichar *defaultText,
                       PRUnichar **result,
                       PRBool *_retval)
{
    *_retval = PR_FALSE;
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TestAuthPrompt::PromptUsernameAndPassword(const PRUnichar *dialogTitle,
                                          const PRUnichar *dialogText,
                                          const PRUnichar *passwordRealm,
                                          PRUint32 savePassword,
                                          PRUnichar **user,
                                          PRUnichar **pwd,
                                          PRBool *_retval)
{
    NS_ConvertUTF16toUTF8 text(passwordRealm);
    printf("* --------------------------------------------------------------------------- *\n");
    printf("* Authentication Required [%s]\n", text.get());
    printf("* --------------------------------------------------------------------------- *\n");

    char buf[256];
    int n;

    printf("Enter username: ");
    fgets(buf, sizeof(buf), stdin);
    n = strlen(buf);
    buf[n-1] = '\0'; // trim trailing newline
    *user = ToNewUnicode(nsDependentCString(buf));

    const char *p;
#ifdef XP_UNIX
    p = getpass("Enter password: ");
#else
    printf("Enter password: ");
    fgets(buf, sizeof(buf), stdin);
    n = strlen(buf);
    buf[n-1] = '\0'; // trim trailing newline
    p = buf;
#endif
    *pwd = ToNewUnicode(nsDependentCString(p));

    // zap buf 
    memset(buf, 0, sizeof(buf));

    *_retval = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
TestAuthPrompt::PromptPassword(const PRUnichar *dialogTitle,
                               const PRUnichar *text,
                               const PRUnichar *passwordRealm,
                               PRUint32 savePassword,
                               PRUnichar **pwd,
                               PRBool *_retval)
{
    *_retval = PR_FALSE;
    return NS_ERROR_NOT_IMPLEMENTED;
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
}

InputTestConsumer::~InputTestConsumer()
{
}

NS_IMPL_ISUPPORTS2(InputTestConsumer, nsIStreamListener, nsIRequestObserver)

NS_IMETHODIMP
InputTestConsumer::OnStartRequest(nsIRequest *request, nsISupports* context)
{
  LOG(("InputTestConsumer::OnStartRequest\n"));

  URLLoadInfo* info = (URLLoadInfo*)context;
  if (info)
    info->mConnectTime = PR_Now() - info->mConnectTime;

  if (gVerbose)
    LOG(("\nStarted loading: %s\n", info ? info->Name() : "UNKNOWN URL"));

  nsCAutoString value;

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  if (channel) {
    nsresult status;
    channel->GetStatus(&status);
    LOG(("Channel Status: %08x\n", status));
    if (NS_SUCCEEDED(status)) {
      LOG(("Channel Info:\n"));

      channel->GetName(value);
      LOG(("\tName: %s\n", value.get()));

      channel->GetContentType(value);
      LOG(("\tContent-Type: %s\n", value.get()));

      channel->GetContentCharset(value);
      LOG(("\tContent-Charset: %s\n", value.get()));

      PRInt32 length = -1;
      if (NS_SUCCEEDED(channel->GetContentLength(&length)))
        LOG(("\tContent-Length: %d\n", length));
      else
        LOG(("\tContent-Length: Unknown\n"));
    }

    nsCOMPtr<nsISupports> owner;
    channel->GetOwner(getter_AddRefs(owner));
    LOG(("\tChannel Owner: %x\n", owner.get()));
  }

  nsCOMPtr<nsIProperties> props = do_QueryInterface(request);
  if (props) {
      nsCOMPtr<nsIURI> foo;
      props->Get("test.foo", NS_GET_IID(nsIURI), getter_AddRefs(foo));
      if (foo) {
          nsCAutoString spec;
          foo->GetSpec(spec);
          LOG(("\ttest.foo: %s\n", spec.get()));
      }
  }

  nsCOMPtr<nsIHttpChannelInternal> httpChannelInt(do_QueryInterface(request));
  if (httpChannelInt) {
      PRUint32 majorVer, minorVer;
      nsresult rv = httpChannelInt->GetResponseVersion(&majorVer, &minorVer);
      if (NS_SUCCEEDED(rv))
          LOG(("HTTP Response version: %u.%u\n", majorVer, minorVer));
  }
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(request));
  if (httpChannel) {
    HeaderVisitor *visitor = new HeaderVisitor();
    if (!visitor)
      return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(visitor);

    LOG(("HTTP request headers:\n"));
    httpChannel->VisitRequestHeaders(visitor);

    LOG(("HTTP response headers:\n"));
    httpChannel->VisitResponseHeaders(visitor);

    NS_RELEASE(visitor);
  }

  nsCOMPtr<nsIResumableChannel> resChannel = do_QueryInterface(request);
  if (resChannel) {
      LOG(("Resumable entity identification:\n"));
      nsCAutoString entityID;
      nsresult rv = resChannel->GetEntityID(entityID);
      if (NS_SUCCEEDED(rv)) {
          LOG(("\t|%s|\n", entityID.get()));
      }
      else {
          LOG(("\t<none>\n"));
      }
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
    PR_DELETE(((void*)self));
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
  LOG(("InputTestConsumer::OnStopRequest [status=%x]\n", aStatus));

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

    LOG(("\nFinished loading: %s  Status Code: %x\n", info->Name(), aStatus));
    if (bHTTPURL)
        LOG(("\tHTTP Status: %u\n", httpStatus));
     if (NS_ERROR_UNKNOWN_HOST == aStatus ||
         NS_ERROR_UNKNOWN_PROXY_HOST == aStatus) {
         LOG(("\tDNS lookup failed.\n"));
     }
    LOG(("\tTime to connect: %.3f seconds\n", connectTime));
    LOG(("\tTime to read: %.3f seconds.\n", readTime));
    LOG(("\tRead: %lld bytes.\n", info->mBytesRead.mValue));
    if (info->mBytesRead == nsInt64(0)) {
    } else if (readTime > 0.0) {
      LOG(("\tThroughput: %.0f bps.\n", (PRFloat64)(info->mBytesRead*nsInt64(8))/readTime));
    } else {
      LOG(("\tThroughput: REAL FAST!!\n"));
    }
  } else {
    LOG(("\nFinished loading: UNKNOWN URL. Status Code: %x\n", aStatus));
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
    }

    NS_IMETHOD GetInterface(const nsIID& iid, void* *result) {
        nsresult rv = NS_ERROR_FAILURE;

        if (iid.Equals(NS_GET_IID(nsIHttpEventSink))) {
          TestHttpEventSink *sink;

          sink = new TestHttpEventSink();
          if (sink == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
          NS_ADDREF(sink);
          rv = sink->QueryInterface(iid, result);
          NS_RELEASE(sink);
        }

        if (iid.Equals(NS_GET_IID(nsIAuthPrompt))) {
          TestAuthPrompt *prompt;

          prompt = new TestAuthPrompt();
          if (prompt == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
          NS_ADDREF(prompt);
          rv = prompt->QueryInterface(iid, result);
          NS_RELEASE(prompt);
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

        rv = pService->NewURI(nsDependentCString(aUrlString), nsnull, nsnull, getter_AddRefs(pURL));
        if (NS_FAILED(rv)) {
            LOG(("ERROR: NewURI failed for %s [rv=%x]\n", aUrlString));
            return rv;
        }
        nsCOMPtr<nsIChannel> pChannel;

        NotificationCallbacks* callbacks = new NotificationCallbacks();
        if (!callbacks) {
            LOG(("Failed to create a new consumer!"));
            return NS_ERROR_OUT_OF_MEMORY;;
        }
        NS_ADDREF(callbacks);

        // Async reading thru the calls of the event sink interface
        rv = NS_NewChannel(getter_AddRefs(pChannel), pURL, pService,
                           nsnull,     // loadGroup
                           callbacks); // notificationCallbacks
        NS_RELEASE(callbacks);
        if (NS_FAILED(rv)) {
            LOG(("ERROR: NS_OpenURI failed for %s [rv=%x]\n", aUrlString, rv));
            return rv;
        }

        nsCOMPtr<nsIProperties> props = do_QueryInterface(pChannel);
        if (props) {
            if (NS_SUCCEEDED(props->Set("test.foo", pURL)))
                LOG(("set prop 'test.foo'\n"));
        }

        /* 
           You may optionally add/set other headers on this
           request object. This is done by QI for the specific
           protocolConnection.
        */
        nsCOMPtr<nsIHttpChannel> pHTTPCon(do_QueryInterface(pChannel));

        if (pHTTPCon) {
            // Setting a sample header.
            rv = pHTTPCon->SetRequestHeader(NS_LITERAL_CSTRING("sample-header"),
                                            NS_LITERAL_CSTRING("Sample-Value"),
                                            PR_FALSE);
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

        if (gResume) {
            nsCOMPtr<nsIResumableChannel> res = do_QueryInterface(pChannel);
            if (!res) {
                NS_ERROR("Channel is not resumable!");
                return NS_ERROR_UNEXPECTED;
            }
            nsCAutoString id;
            if (gEntityID)
                id = gEntityID;
            LOG(("* resuming at %llu bytes, with entity id |%s|\n", gStartAt, id.get()));
            res->ResumeAt(gStartAt, id);
        }
        rv = pChannel->AsyncOpen(listener,  // IStreamListener consumer
                                 info);

        if (NS_SUCCEEDED(rv)) {
            gKeepRunning += 1;
        }
        else {
            LOG(("ERROR: AsyncOpen failed [rv=%x]\n", rv));
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
                    LOG(("\t%s\n", urlString.get()));
                    rv = StartLoadingURL(urlString.get());
                }
            }
        }
    } while (len>0);

    // If anything is left in the fileBuffer, treat it as a URL...
    fileBuffer.StripChars("\r");
    if (fileBuffer.Length()) {
        LOG(("\t%s\n", fileBuffer.get()));
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
    if (test_common_init(&argc, &argv) != 0)
        return -1;

    nsresult rv= (nsresult)-1;
    if (argc < 2) {
        printf("usage: %s [-verbose] [-file <name>] [-resume <startoffset> [-entityid <entityid>]] [-proxy <proxy>] [-console] <url> <url> ... \n", argv[0]);
        return -1;
    }

#if defined(PR_LOGGING)
    gTestLog = PR_NewLogModule("Test");
#endif

    /* 
      The following code only deals with XPCOM registration stuff. and setting
      up the event queues. Copied from TestSocketIO.cpp
    */

    rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
    if (NS_FAILED(rv)) return rv;

    {
        // Create the Event Queue for this thread...
        nsCOMPtr<nsIEventQueueService> eventQService =
                 do_GetService(kEventQueueServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &gEventQ);

        int i;
        LOG(("Trying to load:\n"));
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

            if (PL_strcasecmp(argv[i], "-resume") == 0) {
                gResume = PR_TRUE;
                PR_sscanf(argv[++i], "%llu", &gStartAt);
                continue;
            }

            if (PL_strcasecmp(argv[i], "-entityid") == 0) {
                gEntityID = argv[++i];
                continue;
            }

            if (PL_strcasecmp(argv[i], "-proxy") == 0) {
                SetHttpProxy(argv[++i]);
                continue;
            }

            LOG(("\t%s\n", argv[i]));
            rv = StartLoadingURL(argv[i]);
        }
      // Enter the message pump to allow the URL load to proceed.
        while ( gKeepRunning ) {
            PLEvent *gEvent;
            gEventQ->WaitForEvent(&gEvent);
            gEventQ->HandleEvent(gEvent);
        }
    } // this scopes the nsCOMPtrs
    // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
    NS_ShutdownXPCOM(nsnull);
    return rv;
}
