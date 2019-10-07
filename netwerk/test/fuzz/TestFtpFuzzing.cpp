#include "mozilla/Preferences.h"

#include "nsCOMPtr.h"
#include "nsNetCID.h"
#include "nsString.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsIPrincipal.h"
#include "nsScriptSecurityManager.h"
#include "nsServiceManagerUtils.h"
#include "nsNetUtil.h"
#include "NullPrincipal.h"
#include "nsCycleCollector.h"

#include "nsFtpProtocolHandler.h"
#include "nsIFTPChannel.h"

#include "FuzzingInterface.h"
#include "FuzzingStreamListener.h"
#include "FuzzyLayer.h"

namespace mozilla {
namespace net {

static int FuzzingInitNetworkFtp(int* argc, char*** argv) {
  Preferences::SetBool("network.dns.native-is-localhost", true);
  Preferences::SetBool("fuzzing.necko.enabled", true);
  Preferences::SetBool("network.connectivity-service.enabled", false);
  return 0;
}

static int FuzzingRunNetworkFtp(const uint8_t* data, size_t size) {
  // Set the data to be processed
  if (size > 1024) {
    // If we have more than 1024 bytes, we use the excess for
    // an optional data connection.
    addNetworkFuzzingBuffer(data, 1024, true);
    addNetworkFuzzingBuffer(data+1024, size-1024, true, true);
  } else {
    addNetworkFuzzingBuffer(data, size, true);
  }

  nsWeakPtr channelRef;

  {
    nsCOMPtr<nsIURI> url;
    nsAutoCString spec;
    nsresult rv;

    spec = "ftp://127.0.0.1/";
    if (NS_NewURI(getter_AddRefs(url), spec) != NS_OK) {
      MOZ_CRASH("Call to NS_NewURI failed.");
    }

    nsLoadFlags loadFlags;
    loadFlags = nsIRequest::LOAD_BACKGROUND | nsIRequest::LOAD_BYPASS_CACHE |
                nsIRequest::INHIBIT_CACHING |
                nsIRequest::LOAD_FRESH_CONNECTION |
                nsIChannel::LOAD_INITIAL_DOCUMENT_URI;
    nsSecurityFlags secFlags;
    secFlags = nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL |
               nsILoadInfo::SEC_SANDBOXED;
    nsCOMPtr<nsIChannel> channel;
    rv = NS_NewChannel(getter_AddRefs(channel), url,
                       nsContentUtils::GetSystemPrincipal(), secFlags,
                       nsIContentPolicy::TYPE_INTERNAL_XMLHTTPREQUEST,
                       nullptr,    // aCookieSettings
                       nullptr,    // aPerformanceStorage
                       nullptr,    // loadGroup
                       nullptr,    // aCallbacks
                       loadFlags,  // aLoadFlags
                       nullptr     // aIoService
    );

    if (rv != NS_OK) {
      MOZ_CRASH("Call to NS_NewChannel failed.");
    }

    RefPtr<FuzzingStreamListener> gStreamListener;

    gStreamListener = new FuzzingStreamListener();

    rv = channel->AsyncOpen(gStreamListener);

    if (NS_FAILED(rv)) {
      MOZ_CRASH("asyncOpen failed");
    }

    FUZZING_LOG(("Spinning for StopRequest"));

    // Wait for StopRequest
    gStreamListener->waitUntilDone();

    // The FTP code can cache control connections which would cause
    // a strong reference to be held to our channel. After performing
    // all requests here, we need to prune all cached connections to
    // let the channel die. In the future we can test the caching further
    // with multiple requests using a cached control connection.
    gFtpHandler->Observe(nullptr, "net:clear-active-logins", nullptr);

    channelRef = do_GetWeakReference(channel);
  }

  FUZZING_LOG(("Spinning for channel == nullptr"));

  // Wait for the channel to be destroyed
  SpinEventLoopUntil([&]() -> bool {
    nsCycleCollector_collect(nullptr);
    nsCOMPtr<nsIChannel> channel = do_QueryReferent(channelRef);
    return channel == nullptr;
  });

  if (!signalNetworkFuzzingDone()) {
    // Wait for the connection to indicate closed
    FUZZING_LOG(("Spinning for gFuzzingConnClosed"));
    SpinEventLoopUntil([&]() -> bool { return gFuzzingConnClosed; });
  }

  FUZZING_LOG(("End of iteration"));
  return 0;
}

MOZ_FUZZING_INTERFACE_RAW(FuzzingInitNetworkFtp, FuzzingRunNetworkFtp,
                          NetworkFtp);

}  // namespace net
}  // namespace mozilla
