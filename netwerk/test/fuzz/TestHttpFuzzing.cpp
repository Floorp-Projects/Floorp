#include "mozilla/LoadInfo.h"
#include "mozilla/Preferences.h"
#include "mozilla/SpinEventLoopUntil.h"

#include "nsCOMPtr.h"
#include "nsNetCID.h"
#include "nsString.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsILoadInfo.h"
#include "nsIProxiedProtocolHandler.h"
#include "nsIOService.h"
#include "nsProtocolProxyService.h"
#include "nsScriptSecurityManager.h"
#include "nsServiceManagerUtils.h"
#include "nsNetUtil.h"
#include "NullPrincipal.h"
#include "nsCycleCollector.h"
#include "RequestContextService.h"
#include "nsSandboxFlags.h"

#include "FuzzingInterface.h"
#include "FuzzingStreamListener.h"
#include "FuzzyLayer.h"

namespace mozilla {
namespace net {

// Target spec and optional proxy type to use, set by the respective
// initialization function so we can cover all combinations.
static nsAutoCString httpSpec;
static nsAutoCString proxyType;
static size_t minSize;

static int FuzzingInitNetworkHttp(int* argc, char*** argv) {
  Preferences::SetBool("network.dns.native-is-localhost", true);
  Preferences::SetBool("fuzzing.necko.enabled", true);
  Preferences::SetInt("network.http.speculative-parallel-limit", 0);
  Preferences::SetInt("network.http.spdy.default-concurrent", 1);

  if (httpSpec.IsEmpty()) {
    httpSpec = "http://127.0.0.1/";
  }

  net_EnsurePSMInit();

  return 0;
}

static int FuzzingInitNetworkHttp2(int* argc, char*** argv) {
  httpSpec = "https://127.0.0.1/";
  return FuzzingInitNetworkHttp(argc, argv);
}

static int FuzzingInitNetworkHttp3(int* argc, char*** argv) {
  Preferences::SetBool("fuzzing.necko.http3", true);
  Preferences::SetBool("network.http.http3.enabled", true);
  Preferences::SetCString("network.http.http3.alt-svc-mapping-for-testing",
                          "fuzz.bad.tld;h3=:443");
  httpSpec = "https://fuzz.bad.tld/";
  minSize = 1200;
  return FuzzingInitNetworkHttp(argc, argv);
}

static int FuzzingInitNetworkHttpProxyHttp2(int* argc, char*** argv) {
  // This is http over an https proxy
  proxyType = "https";

  return FuzzingInitNetworkHttp(argc, argv);
}

static int FuzzingInitNetworkHttp2ProxyHttp2(int* argc, char*** argv) {
  // This is https over an https proxy
  proxyType = "https";

  return FuzzingInitNetworkHttp2(argc, argv);
}

static int FuzzingInitNetworkHttpProxyPlain(int* argc, char*** argv) {
  // This is http over an http proxy
  proxyType = "http";

  return FuzzingInitNetworkHttp(argc, argv);
}

static int FuzzingInitNetworkHttp2ProxyPlain(int* argc, char*** argv) {
  // This is https over an http proxy
  proxyType = "http";

  return FuzzingInitNetworkHttp2(argc, argv);
}

static int FuzzingRunNetworkHttp(const uint8_t* data, size_t size) {
  if (size < minSize) {
    return 0;
  }

  // Set the data to be processed
  addNetworkFuzzingBuffer(data, size);

  nsWeakPtr channelRef;

  nsCOMPtr<nsIRequestContextService> rcsvc =
      mozilla::net::RequestContextService::GetOrCreate();
  uint64_t rcID;

  {
    nsCOMPtr<nsIURI> url;
    nsresult rv;

    if (NS_NewURI(getter_AddRefs(url), httpSpec) != NS_OK) {
      MOZ_CRASH("Call to NS_NewURI failed.");
    }

    nsLoadFlags loadFlags;
    loadFlags = nsIRequest::LOAD_BACKGROUND | nsIRequest::LOAD_BYPASS_CACHE |
                nsIRequest::INHIBIT_CACHING |
                nsIRequest::LOAD_FRESH_CONNECTION |
                nsIChannel::LOAD_INITIAL_DOCUMENT_URI;
    nsSecurityFlags secFlags;
    secFlags = nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL;
    uint32_t sandboxFlags = SANDBOXED_ORIGIN;

    nsCOMPtr<nsIChannel> channel;
    nsCOMPtr<nsILoadInfo> loadInfo;

    if (!proxyType.IsEmpty()) {
      nsAutoCString proxyHost("127.0.0.2");

      nsCOMPtr<nsIProtocolProxyService2> ps =
          do_GetService(NS_PROTOCOLPROXYSERVICE_CID);
      if (!ps) {
        MOZ_CRASH("Failed to create nsIProtocolProxyService2");
      }

      mozilla::net::nsProtocolProxyService* pps =
          static_cast<mozilla::net::nsProtocolProxyService*>(ps.get());

      nsCOMPtr<nsIProxyInfo> proxyInfo;
      rv = pps->NewProxyInfo(proxyType, proxyHost, 443,
                             ""_ns,       // aProxyAuthorizationHeader
                             ""_ns,       // aConnectionIsolationKey
                             0,           // aFlags
                             UINT32_MAX,  // aFailoverTimeout
                             nullptr,     // aFailoverProxy
                             getter_AddRefs(proxyInfo));

      if (NS_FAILED(rv)) {
        MOZ_CRASH("Call to NewProxyInfo failed.");
      }

      nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
      if (NS_FAILED(rv)) {
        MOZ_CRASH("do_GetIOService failed.");
      }

      nsCOMPtr<nsIProtocolHandler> handler;
      rv = ioService->GetProtocolHandler("http", getter_AddRefs(handler));
      if (NS_FAILED(rv)) {
        MOZ_CRASH("GetProtocolHandler failed.");
      }

      nsCOMPtr<nsIProxiedProtocolHandler> pph = do_QueryInterface(handler, &rv);
      if (NS_FAILED(rv)) {
        MOZ_CRASH("do_QueryInterface failed.");
      }

      loadInfo = new LoadInfo(
          nsContentUtils::GetSystemPrincipal(),  // loading principal
          nsContentUtils::GetSystemPrincipal(),  // triggering principal
          nullptr,                               // Context
          secFlags, nsIContentPolicy::TYPE_INTERNAL_XMLHTTPREQUEST,
          Maybe<mozilla::dom::ClientInfo>(),
          Maybe<mozilla::dom::ServiceWorkerDescriptor>(), sandboxFlags);

      rv = pph->NewProxiedChannel(url, proxyInfo,
                                  0,        // aProxyResolveFlags
                                  nullptr,  // aProxyURI
                                  loadInfo, getter_AddRefs(channel));

      if (NS_FAILED(rv)) {
        MOZ_CRASH("Call to newProxiedChannel failed.");
      }
    } else {
      rv = NS_NewChannel(getter_AddRefs(channel), url,
                         nsContentUtils::GetSystemPrincipal(), secFlags,
                         nsIContentPolicy::TYPE_INTERNAL_XMLHTTPREQUEST,
                         nullptr,    // aCookieJarSettings
                         nullptr,    // aPerformanceStorage
                         nullptr,    // loadGroup
                         nullptr,    // aCallbacks
                         loadFlags,  // aLoadFlags
                         nullptr,    // aIoService
                         sandboxFlags);

      if (NS_FAILED(rv)) {
        MOZ_CRASH("Call to NS_NewChannel failed.");
      }

      loadInfo = channel->LoadInfo();
    }

    if (NS_FAILED(loadInfo->SetSkipContentSniffing(true))) {
      MOZ_CRASH("Failed to call SetSkipContentSniffing");
    }

    RefPtr<FuzzingStreamListener> gStreamListener;
    nsCOMPtr<nsIHttpChannel> gHttpChannel;

    gHttpChannel = do_QueryInterface(channel);
    rv = gHttpChannel->SetRequestMethod("GET"_ns);
    if (NS_FAILED(rv)) {
      MOZ_CRASH("SetRequestMethod on gHttpChannel failed.");
    }

    nsCOMPtr<nsIRequestContext> rc;
    rv = rcsvc->NewRequestContext(getter_AddRefs(rc));
    if (NS_FAILED(rv)) {
      MOZ_CRASH("NewRequestContext failed.");
    }
    rcID = rc->GetID();

    rv = gHttpChannel->SetRequestContextID(rcID);
    if (NS_FAILED(rv)) {
      MOZ_CRASH("SetRequestContextID on gHttpChannel failed.");
    }

    if (!proxyType.IsEmpty()) {
      // NewProxiedChannel doesn't allow us to pass loadFlags directly
      rv = gHttpChannel->SetLoadFlags(loadFlags);
      if (rv != NS_OK) {
        MOZ_CRASH("SetRequestMethod on gHttpChannel failed.");
      }
    }

    gStreamListener = new FuzzingStreamListener();
    gHttpChannel->AsyncOpen(gStreamListener);

    // Wait for StopRequest
    gStreamListener->waitUntilDone();

    bool mainPingBack = false;

    NS_DispatchBackgroundTask(NS_NewRunnableFunction("Dummy", [&]() {
      NS_DispatchToMainThread(
          NS_NewRunnableFunction("Dummy", [&]() { mainPingBack = true; }));
    }));

    SpinEventLoopUntil("FuzzingRunNetworkHttp(mainPingBack)"_ns,
                       [&]() -> bool { return mainPingBack; });

    channelRef = do_GetWeakReference(gHttpChannel);
  }

  // Wait for the channel to be destroyed
  SpinEventLoopUntil(
      "FuzzingRunNetworkHttp(channel == nullptr)"_ns, [&]() -> bool {
        nsCycleCollector_collect(CCReason::API, nullptr);
        nsCOMPtr<nsIHttpChannel> channel = do_QueryReferent(channelRef);
        return channel == nullptr;
      });

  if (!signalNetworkFuzzingDone()) {
    // Wait for the connection to indicate closed
    SpinEventLoopUntil("FuzzingRunNetworkHttp(gFuzzingConnClosed)"_ns,
                       [&]() -> bool { return gFuzzingConnClosed; });
  }

  rcsvc->RemoveRequestContext(rcID);
  return 0;
}

MOZ_FUZZING_INTERFACE_RAW(FuzzingInitNetworkHttp, FuzzingRunNetworkHttp,
                          NetworkHttp);

MOZ_FUZZING_INTERFACE_RAW(FuzzingInitNetworkHttp2, FuzzingRunNetworkHttp,
                          NetworkHttp2);

MOZ_FUZZING_INTERFACE_RAW(FuzzingInitNetworkHttp3, FuzzingRunNetworkHttp,
                          NetworkHttp3);

MOZ_FUZZING_INTERFACE_RAW(FuzzingInitNetworkHttp2ProxyHttp2,
                          FuzzingRunNetworkHttp, NetworkHttp2ProxyHttp2);

MOZ_FUZZING_INTERFACE_RAW(FuzzingInitNetworkHttpProxyHttp2,
                          FuzzingRunNetworkHttp, NetworkHttpProxyHttp2);

MOZ_FUZZING_INTERFACE_RAW(FuzzingInitNetworkHttpProxyPlain,
                          FuzzingRunNetworkHttp, NetworkHttpProxyPlain);

MOZ_FUZZING_INTERFACE_RAW(FuzzingInitNetworkHttp2ProxyPlain,
                          FuzzingRunNetworkHttp, NetworkHttp2ProxyPlain);

}  // namespace net
}  // namespace mozilla
