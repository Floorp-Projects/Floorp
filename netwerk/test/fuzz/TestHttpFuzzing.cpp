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

#include "FuzzingInterface.h"

namespace mozilla {
namespace net {

// Used to determine if the fuzzing target should use https:// in spec.
static bool fuzzHttps = false;

class FuzzingStreamListener final : public nsIStreamListener {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  FuzzingStreamListener() = default;

  void waitUntilDone() {
    SpinEventLoopUntil([&]() { return mChannelDone; });
  }

 private:
  ~FuzzingStreamListener() = default;
  bool mChannelDone = false;
};

NS_IMPL_ISUPPORTS(FuzzingStreamListener, nsIStreamListener, nsIRequestObserver)

NS_IMETHODIMP
FuzzingStreamListener::OnStartRequest(nsIRequest* aRequest) {
  FUZZING_LOG(("FuzzingStreamListener::OnStartRequest"));
  return NS_OK;
}

NS_IMETHODIMP
FuzzingStreamListener::OnDataAvailable(nsIRequest* aRequest,
                                       nsIInputStream* aInputStream,
                                       uint64_t aOffset, uint32_t aCount) {
  FUZZING_LOG(("FuzzingStreamListener::OnDataAvailable"));
  static uint32_t const kCopyChunkSize = 128 * 1024;
  uint32_t toRead = std::min<uint32_t>(aCount, kCopyChunkSize);
  nsCString data;

  while (aCount) {
    nsresult rv = NS_ReadInputStreamToString(aInputStream, data, toRead);
    if (NS_FAILED(rv)) {
      return rv;
    }
    aOffset += toRead;
    aCount -= toRead;
    toRead = std::min<uint32_t>(aCount, kCopyChunkSize);
  }
  return NS_OK;
}

NS_IMETHODIMP
FuzzingStreamListener::OnStopRequest(nsIRequest* aRequest,
                                     nsresult aStatusCode) {
  FUZZING_LOG(("FuzzingStreamListener::OnStopRequest"));
  mChannelDone = true;
  return NS_OK;
}

// Forward declaration to the function in FuzzyLayer.cpp,
// used to set the buffer to the global defined there.
void setNetworkFuzzingBuffer(const uint8_t* data, size_t size);
extern Atomic<bool> gFuzzingConnClosed;

static int FuzzingInitNetworkHttp(int* argc, char*** argv) {
  Preferences::SetBool("network.dns.native-is-localhost", true);
  Preferences::SetBool("fuzzing.necko.enabled", true);
  Preferences::SetInt("network.http.speculative-parallel-limit", 0);
  return 0;
}

static int FuzzingInitNetworkHttp2(int* argc, char*** argv) {
  fuzzHttps = true;
  Preferences::SetInt("network.http.spdy.default-concurrent", 1);
  return FuzzingInitNetworkHttp(argc, argv);
}

static int FuzzingRunNetworkHttp(const uint8_t* data, size_t size) {
  // Set the data to be processed
  setNetworkFuzzingBuffer(data, size);

  nsWeakPtr channelRef;

  {
    nsCOMPtr<nsIURI> url;
    nsAutoCString spec;
    nsresult rv;

    if (fuzzHttps) {
      spec = "https://127.0.0.1/";
    } else {
      spec = "http://127.0.0.1/";
    }

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
    nsCOMPtr<nsIHttpChannel> gHttpChannel;

    gHttpChannel = do_QueryInterface(channel);
    rv = gHttpChannel->SetRequestMethod(NS_LITERAL_CSTRING("GET"));
    if (rv != NS_OK) {
      MOZ_CRASH("SetRequestMethod on gHttpChannel failed.");
    }

    gStreamListener = new FuzzingStreamListener();
    gHttpChannel->AsyncOpen(gStreamListener);

    // Wait for StopRequest
    gStreamListener->waitUntilDone();

    channelRef = do_GetWeakReference(gHttpChannel);
  }

  // Wait for the channel to be destroyed
  SpinEventLoopUntil([&]() -> bool {
    nsCycleCollector_collect(nullptr);
    nsCOMPtr<nsIHttpChannel> channel = do_QueryReferent(channelRef);
    return channel == nullptr;
  });

  // Wait for the connection to indicate closed
  SpinEventLoopUntil([&]() -> bool { return gFuzzingConnClosed; });

  return 0;
}

MOZ_FUZZING_INTERFACE_RAW(FuzzingInitNetworkHttp, FuzzingRunNetworkHttp,
                          NetworkHttp);

MOZ_FUZZING_INTERFACE_RAW(FuzzingInitNetworkHttp2, FuzzingRunNetworkHttp,
                          NetworkHttp2);

}  // namespace net
}  // namespace mozilla
