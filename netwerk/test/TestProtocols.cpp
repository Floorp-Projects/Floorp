/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* 
    The TestProtocols tests the basic protocols architecture and can 
    be used to test individual protocols as well. If this grows too
    big then we should split it to individual protocols. 

    -Gagan Saksena 04/29/99
*/

#include "TestCommon.h"
#include <algorithm>

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
#include "nsIChannelEventSink.h" 
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIInterfaceRequestor.h" 
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDNSService.h" 
#include "nsIAuthPrompt.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPropertyBag2.h"
#include "nsIWritablePropertyBag2.h"
#include "nsITimedChannel.h"
#include "mozilla/Attributes.h"
#include "mozilla/unused.h"
#include "nsIScriptSecurityManager.h"

#include "nsISimpleEnumerator.h"
#include "nsStringAPI.h"
#include "nsNetUtil.h"
#include "prlog.h"

using namespace mozilla;

namespace TestProtocols {

#if defined(PR_LOGGING)
//
// set NSPR_LOG_MODULES=Test:5
//
static PRLogModuleInfo *gTestLog = nullptr;
#endif
#define LOG(args) PR_LOG(gTestLog, PR_LOG_DEBUG, args)

static NS_DEFINE_CID(kIOServiceCID,              NS_IOSERVICE_CID);

//static PRTime gElapsedTime; // enable when we time it...
static int gKeepRunning = 0;
static bool gVerbose = false;
static bool gAskUserForInput = false;
static bool gResume = false;
static uint64_t gStartAt = 0;

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
  nsAutoCString proxyHost;
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

static nsresult
SetPACFile(const char* pacURL)
{
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs)
  {
    prefs->SetCharPref("network.proxy.autoconfig_url", pacURL);
    prefs->SetIntPref("network.proxy.type", 2); // PAC file
  }
  LOG(("connecting using PAC file %s\n", pacURL));
  return NS_OK;
}

//-----------------------------------------------------------------------------
// Timing information
//-----------------------------------------------------------------------------

void PrintTimingInformation(nsITimedChannel* channel) {
#define PRINT_VALUE(property)                                              \
    {                                                                      \
        PRTime value;                                                      \
        channel->Get##property(&value);                                    \
        if (value) {                                                       \
          PRExplodedTime exploded;                                         \
          PR_ExplodeTime(value, PR_LocalTimeParameters, &exploded);        \
          char buf[256];                                                   \
          PR_FormatTime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &exploded); \
          LOG(("  " #property ":\t%s (%i usec)", buf, exploded.tm_usec));  \
        } else {                                                           \
          LOG(("  " #property ":\t0"));                                    \
        }                                                                  \
    }
    LOG(("Timing data:"));
    PRINT_VALUE(ChannelCreationTime)
    PRINT_VALUE(AsyncOpenTime)
    PRINT_VALUE(DomainLookupStartTime)
    PRINT_VALUE(DomainLookupEndTime)
    PRINT_VALUE(ConnectStartTime)
    PRINT_VALUE(ConnectEndTime)
    PRINT_VALUE(RequestStartTime)
    PRINT_VALUE(ResponseStartTime)
    PRINT_VALUE(ResponseEndTime)
    PRINT_VALUE(CacheReadStartTime)
    PRINT_VALUE(CacheReadEndTime)
}

//-----------------------------------------------------------------------------
// HeaderVisitor
//-----------------------------------------------------------------------------

class HeaderVisitor : public nsIHttpHeaderVisitor
{
  virtual ~HeaderVisitor() {}
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHTTPHEADERVISITOR

  HeaderVisitor() { }
};
NS_IMPL_ISUPPORTS(HeaderVisitor, nsIHttpHeaderVisitor)

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
  virtual ~URLLoadInfo();

public:

  explicit URLLoadInfo(const char* aUrl);

  // ISupports interface...
  NS_DECL_THREADSAFE_ISUPPORTS

  const char* Name() { return mURLString.get(); }
  int64_t   mBytesRead;
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


NS_IMPL_ISUPPORTS0(URLLoadInfo)

//-----------------------------------------------------------------------------
// TestChannelEventSink
//-----------------------------------------------------------------------------

class TestChannelEventSink : public nsIChannelEventSink
{
  virtual ~TestChannelEventSink();

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICHANNELEVENTSINK

  TestChannelEventSink();
};

TestChannelEventSink::TestChannelEventSink()
{
}

TestChannelEventSink::~TestChannelEventSink()
{
}


NS_IMPL_ISUPPORTS(TestChannelEventSink, nsIChannelEventSink)

NS_IMETHODIMP
TestChannelEventSink::AsyncOnChannelRedirect(nsIChannel *channel,
                                             nsIChannel *newChannel,
                                             uint32_t flags,
                                             nsIAsyncVerifyRedirectCallback *callback)
{
    LOG(("\n+++ TestChannelEventSink::OnChannelRedirect (with flags %x) +++\n",
         flags));
    callback->OnRedirectVerifyCallback(NS_OK);
    return NS_OK;
}

//-----------------------------------------------------------------------------
// TestAuthPrompt
//-----------------------------------------------------------------------------

class TestAuthPrompt : public nsIAuthPrompt
{
  virtual ~TestAuthPrompt();

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIAUTHPROMPT

  TestAuthPrompt();
};

NS_IMPL_ISUPPORTS(TestAuthPrompt, nsIAuthPrompt)

TestAuthPrompt::TestAuthPrompt()
{
}

TestAuthPrompt::~TestAuthPrompt()
{
}

NS_IMETHODIMP
TestAuthPrompt::Prompt(const char16_t *dialogTitle,
                       const char16_t *text,
                       const char16_t *passwordRealm,
                       uint32_t savePassword,
                       const char16_t *defaultText,
                       char16_t **result,
                       bool *_retval)
{
    *_retval = false;
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TestAuthPrompt::PromptUsernameAndPassword(const char16_t *dialogTitle,
                                          const char16_t *dialogText,
                                          const char16_t *passwordRealm,
                                          uint32_t savePassword,
                                          char16_t **user,
                                          char16_t **pwd,
                                          bool *_retval)
{
    NS_ConvertUTF16toUTF8 text(passwordRealm);
    printf("* --------------------------------------------------------------------------- *\n");
    printf("* Authentication Required [%s]\n", text.get());
    printf("* --------------------------------------------------------------------------- *\n");

    char buf[256];
    int n;

    printf("Enter username: ");
    unused << fgets(buf, sizeof(buf), stdin);
    n = strlen(buf);
    buf[n-1] = '\0'; // trim trailing newline
    *user = NS_StringCloneData(NS_ConvertUTF8toUTF16(buf));

    const char *p;
#if defined(XP_UNIX) && !defined(ANDROID)
    p = getpass("Enter password: ");
#else
    printf("Enter password: ");
    fgets(buf, sizeof(buf), stdin);
    n = strlen(buf);
    buf[n-1] = '\0'; // trim trailing newline
    p = buf;
#endif
    *pwd = NS_StringCloneData(NS_ConvertUTF8toUTF16(p));

    // zap buf 
    memset(buf, 0, sizeof(buf));

    *_retval = true;
    return NS_OK;
}

NS_IMETHODIMP
TestAuthPrompt::PromptPassword(const char16_t *dialogTitle,
                               const char16_t *text,
                               const char16_t *passwordRealm,
                               uint32_t savePassword,
                               char16_t **pwd,
                               bool *_retval)
{
    *_retval = false;
    return NS_ERROR_NOT_IMPLEMENTED;
}

//-----------------------------------------------------------------------------
// InputTestConsumer
//-----------------------------------------------------------------------------

class InputTestConsumer : public nsIStreamListener
{
  virtual ~InputTestConsumer();

public:

  InputTestConsumer();

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

NS_IMPL_ISUPPORTS(InputTestConsumer, nsIStreamListener, nsIRequestObserver)

NS_IMETHODIMP
InputTestConsumer::OnStartRequest(nsIRequest *request, nsISupports* context)
{
  LOG(("InputTestConsumer::OnStartRequest\n"));

  URLLoadInfo* info = (URLLoadInfo*)context;
  if (info)
    info->mConnectTime = PR_Now() - info->mConnectTime;

  if (gVerbose) {
    LOG(("\nStarted loading: %s\n", info ? info->Name() : "UNKNOWN URL"));
  }

  nsAutoCString value;

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

      int64_t length = -1;
      if (NS_SUCCEEDED(channel->GetContentLength(&length))) {
        LOG(("\tContent-Length: %lld\n", length));
      } else {
        LOG(("\tContent-Length: Unknown\n"));
      }
    }

    nsCOMPtr<nsISupports> owner;
    channel->GetOwner(getter_AddRefs(owner));
    LOG(("\tChannel Owner: %x\n", owner.get()));
  }

  nsCOMPtr<nsIPropertyBag2> props = do_QueryInterface(request);
  if (props) {
      nsCOMPtr<nsIURI> foo;
      props->GetPropertyAsInterface(NS_LITERAL_STRING("test.foo"),
                                    NS_GET_IID(nsIURI),
                                    getter_AddRefs(foo));
      if (foo) {
          nsAutoCString spec;
          foo->GetSpec(spec);
          LOG(("\ttest.foo: %s\n", spec.get()));
      }
  }

  nsCOMPtr<nsIHttpChannelInternal> httpChannelInt(do_QueryInterface(request));
  if (httpChannelInt) {
      uint32_t majorVer, minorVer;
      nsresult rv = httpChannelInt->GetResponseVersion(&majorVer, &minorVer);
      if (NS_SUCCEEDED(rv)) {
          LOG(("HTTP Response version: %u.%u\n", majorVer, minorVer));
      }
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
      nsAutoCString entityID;
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
                                   uint64_t aSourceOffset,
                                   uint32_t aLength)
{
  char buf[1025];
  uint32_t amt, size;
  nsresult rv;
  URLLoadInfo* info = (URLLoadInfo*)context;

  while (aLength) {
    size = std::min<uint32_t>(aLength, sizeof(buf));

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
InputTestConsumer::OnStopRequest(nsIRequest *request, nsISupports* context,
                                 nsresult aStatus)
{
  LOG(("InputTestConsumer::OnStopRequest [status=%x]\n", aStatus));

  URLLoadInfo* info = (URLLoadInfo*)context;

  if (info) {
    uint32_t httpStatus;
    bool bHTTPURL = false;

    info->mTotalTime = PR_Now() - info->mTotalTime;

    double readTime = ((info->mTotalTime-info->mConnectTime)/1000.0)/1000.0;

    nsCOMPtr<nsIHttpChannel> pHTTPCon(do_QueryInterface(request));
    if (pHTTPCon) {
        pHTTPCon->GetResponseStatus(&httpStatus);
        bHTTPURL = true;
    }

    LOG(("\nFinished loading: %s  Status Code: %x\n", info->Name(), aStatus));
    if (bHTTPURL) {
      LOG(("\tHTTP Status: %u\n", httpStatus));
    }
    if (NS_ERROR_UNKNOWN_HOST == aStatus ||
        NS_ERROR_UNKNOWN_PROXY_HOST == aStatus) {
      LOG(("\tDNS lookup failed.\n"));
    }
    LOG(("\tTime to connect: %.3f seconds\n", (info->mConnectTime/1000.0)/1000.0));
    LOG(("\tTime to read: %.3f seconds.\n", readTime));
    LOG(("\tRead: %lld bytes.\n", info->mBytesRead));
    if (info->mBytesRead == int64_t(0)) {
    } else if (readTime > 0.0) {
      LOG(("\tThroughput: %.0f bps.\n", (double)(info->mBytesRead*int64_t(8))/readTime));
    } else {
      LOG(("\tThroughput: REAL FAST!!\n"));
    }

    nsCOMPtr<nsITimedChannel> timed(do_QueryInterface(request));
    if (timed)
        PrintTimingInformation(timed);
  } else {
    LOG(("\nFinished loading: UNKNOWN URL. Status Code: %x\n", aStatus));
  }

  if (--gKeepRunning == 0)
    QuitPumpingEvents();
  return NS_OK;
}

//-----------------------------------------------------------------------------
// NotificationCallbacks
//-----------------------------------------------------------------------------

class NotificationCallbacks MOZ_FINAL : public nsIInterfaceRequestor {

    ~NotificationCallbacks() {}

public:
    NS_DECL_ISUPPORTS

    NotificationCallbacks() {
    }

    NS_IMETHOD GetInterface(const nsIID& iid, void* *result) {
        nsresult rv = NS_ERROR_FAILURE;

        if (iid.Equals(NS_GET_IID(nsIChannelEventSink))) {
          TestChannelEventSink *sink;

          sink = new TestChannelEventSink();
          if (sink == nullptr)
            return NS_ERROR_OUT_OF_MEMORY;
          NS_ADDREF(sink);
          rv = sink->QueryInterface(iid, result);
          NS_RELEASE(sink);
        }

        if (iid.Equals(NS_GET_IID(nsIAuthPrompt))) {
          TestAuthPrompt *prompt;

          prompt = new TestAuthPrompt();
          if (prompt == nullptr)
            return NS_ERROR_OUT_OF_MEMORY;
          NS_ADDREF(prompt);
          rv = prompt->QueryInterface(iid, result);
          NS_RELEASE(prompt);
        }
        return rv;
    }
};

NS_IMPL_ISUPPORTS(NotificationCallbacks, nsIInterfaceRequestor)

//-----------------------------------------------------------------------------
// helpers...
//-----------------------------------------------------------------------------

nsresult StartLoadingURL(const char* aUrlString)
{
    nsresult rv;

    nsCOMPtr<nsIIOService> pService(do_GetService(kIOServiceCID, &rv));
    if (pService) {
        nsCOMPtr<nsIURI> pURL;

        rv = pService->NewURI(nsDependentCString(aUrlString), nullptr, nullptr, getter_AddRefs(pURL));
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

        nsCOMPtr<nsIScriptSecurityManager> secman =
          do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
           nsCOMPtr<nsIPrincipal> systemPrincipal;
        rv = secman->GetSystemPrincipal(getter_AddRefs(systemPrincipal));
        NS_ENSURE_SUCCESS(rv, rv);

        // Async reading thru the calls of the event sink interface
        rv = NS_NewChannel(getter_AddRefs(pChannel),
                           pURL,
                           systemPrincipal,
                           nsILoadInfo::SEC_NORMAL,
                           nsIContentPolicy::TYPE_OTHER,
                           nullptr,  // loadGroup
                           callbacks,
                           nsIRequest::LOAD_NORMAL,
                           pService);

        NS_RELEASE(callbacks);
        if (NS_FAILED(rv)) {
            LOG(("ERROR: NS_OpenURI failed for %s [rv=%x]\n", aUrlString, rv));
            return rv;
        }

        nsCOMPtr<nsITimedChannel> timed(do_QueryInterface(pChannel));
        if (timed)
            timed->SetTimingEnabled(true);

        nsCOMPtr<nsIWritablePropertyBag2> props = do_QueryInterface(pChannel);
        if (props) {
            rv = props->SetPropertyAsInterface(NS_LITERAL_STRING("test.foo"),
                                               pURL);
            if (NS_SUCCEEDED(rv)) {
                LOG(("set prop 'test.foo'\n"));
            }
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
                                            false);
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
            nsAutoCString id;
            if (gEntityID)
                id = gEntityID;
            LOG(("* resuming at %llu bytes, with entity id |%s|\n", gStartAt, id.get()));
            res->ResumeAt(gStartAt, id);
        }
        rv = pChannel->AsyncOpen(listener,  // IStreamListener consumer
                                 info);

        if (NS_SUCCEEDED(rv)) {
            gKeepRunning++;
        }
        else {
            LOG(("ERROR: AsyncOpen failed [rv=%x]\n", rv));
        }
        NS_RELEASE(listener);
        NS_RELEASE(info);
    }

    return rv;
}

static int32_t
FindChar(nsCString& buffer, char c)
{
    const char *b;
    int32_t len = NS_CStringGetData(buffer, &b);

    for (int32_t offset = 0; offset < len; ++offset) {
        if (b[offset] == c)
            return offset;
    }

    return -1;
}
        

static void
StripChar(nsCString& buffer, char c)
{
    const char *b;
    uint32_t len = NS_CStringGetData(buffer, &b) - 1;

    for (; len > 0; --len) {
        if (b[len] == c) {
            buffer.Cut(len, 1);
            NS_CStringGetData(buffer, &b);
        }
    }
}

nsresult LoadURLsFromFile(char *aFileName)
{
    nsresult rv = NS_OK;
    int32_t len, offset;
    PRFileDesc* fd;
    char buffer[1024];
    nsCString fileBuffer;
    nsCString urlString;

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
            while ((offset = FindChar(fileBuffer, '\n')) != -1) {
                urlString = StringHead(fileBuffer, offset);
                fileBuffer.Cut(0, offset+1);

                StripChar(urlString, '\r');
                if (urlString.Length()) {
                    LOG(("\t%s\n", urlString.get()));
                    rv = StartLoadingURL(urlString.get());
                    if (NS_FAILED(rv)) {
                        // No need to log an error -- StartLoadingURL already
                        // did that for us, probably.
                        PR_Close(fd);
                        return rv;
                    }
                }
            }
        }
    } while (len>0);

    // If anything is left in the fileBuffer, treat it as a URL...
    StripChar(fileBuffer, '\r');
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
    unused << scanf("%s", buffer);
    if (buffer[0]=='q') 
        gAskUserForInput = false;
    else
        StartLoadingURL(buffer);
    return NS_OK;
}

} // namespace

using namespace TestProtocols;

int
main(int argc, char* argv[])
{
    if (test_common_init(&argc, &argv) != 0)
        return -1;

    nsresult rv= (nsresult)-1;
    if (argc < 2) {
        printf("usage: %s [-verbose] [-file <name>] [-resume <startoffset>"
               "[-entityid <entityid>]] [-proxy <proxy>] [-pac <pacURL>]"
               "[-console] <url> <url> ... \n", argv[0]);
        return -1;
    }

#if defined(PR_LOGGING)
    gTestLog = PR_NewLogModule("Test");
#endif

    /* 
      The following code only deals with XPCOM registration stuff. and setting
      up the event queues. Copied from TestSocketIO.cpp
    */

    rv = NS_InitXPCOM2(nullptr, nullptr, nullptr);
    if (NS_FAILED(rv)) return -1;

    {
        int i;
        LOG(("Trying to load:\n"));
        for (i=1; i<argc; i++) {
            // Turn on verbose printing...
            if (PL_strcasecmp(argv[i], "-verbose") == 0) {
                gVerbose = true;
                continue;
            }

            // Turn on netlib tracing...
            if (PL_strcasecmp(argv[i], "-file") == 0) {
                LoadURLsFromFile(argv[++i]);
                continue;
            }

            if (PL_strcasecmp(argv[i], "-console") == 0) {
                gAskUserForInput = true;
                continue;
            }

            if (PL_strcasecmp(argv[i], "-resume") == 0) {
                gResume = true;
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

            if (PL_strcasecmp(argv[i], "-pac") == 0) {
                SetPACFile(argv[++i]);
                continue;
            }

            LOG(("\t%s\n", argv[i]));
            rv = StartLoadingURL(argv[i]);
        }
        // Enter the message pump to allow the URL load to proceed.
        PumpEvents();
    } // this scopes the nsCOMPtrs
    // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
    NS_ShutdownXPCOM(nullptr);
    return NS_FAILED(rv) ? -1 : 0;
}
