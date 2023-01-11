#include "mozilla/Preferences.h"

#include "FuzzingInterface.h"
#include "FuzzyLayer.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsCycleCollector.h"
#include "nsIPrincipal.h"
#include "nsIWebSocketChannel.h"
#include "nsIWebSocketListener.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsString.h"
#include "nsScriptSecurityManager.h"
#include "nsServiceManagerUtils.h"
#include "NullPrincipal.h"
#include "nsSandboxFlags.h"

namespace mozilla {
namespace net {

// Used to determine if the fuzzing target should use https:// in spec.
static bool fuzzWSS = true;

class FuzzingWebSocketListener final : public nsIWebSocketListener {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBSOCKETLISTENER

  FuzzingWebSocketListener() = default;

  void waitUntilDoneOrStarted() {
    SpinEventLoopUntil("FuzzingWebSocketListener::waitUntilDoneOrStarted"_ns,
                       [&]() { return mChannelDone || mChannelStarted; });
  }

  void waitUntilDone() {
    SpinEventLoopUntil("FuzzingWebSocketListener::waitUntilDone"_ns,
                       [&]() { return mChannelDone; });
  }

  void waitUntilDoneOrAck() {
    SpinEventLoopUntil("FuzzingWebSocketListener::waitUntilDoneOrAck"_ns,
                       [&]() { return mChannelDone || mChannelAck; });
  }

  bool isStarted() { return mChannelStarted; }

 private:
  ~FuzzingWebSocketListener() = default;
  bool mChannelDone = false;
  bool mChannelStarted = false;
  bool mChannelAck = false;
};

NS_IMPL_ISUPPORTS(FuzzingWebSocketListener, nsIWebSocketListener)

NS_IMETHODIMP
FuzzingWebSocketListener::OnStart(nsISupports* aContext) {
  FUZZING_LOG(("FuzzingWebSocketListener::OnStart"));
  mChannelStarted = true;
  return NS_OK;
}

NS_IMETHODIMP
FuzzingWebSocketListener::OnStop(nsISupports* aContext, nsresult aStatusCode) {
  FUZZING_LOG(("FuzzingWebSocketListener::OnStop"));
  mChannelDone = true;
  return NS_OK;
}

NS_IMETHODIMP
FuzzingWebSocketListener::OnAcknowledge(nsISupports* aContext, uint32_t aSize) {
  FUZZING_LOG(("FuzzingWebSocketListener::OnAcknowledge"));
  mChannelAck = true;
  return NS_OK;
}

NS_IMETHODIMP
FuzzingWebSocketListener::OnServerClose(nsISupports* aContext, uint16_t aCode,
                                        const nsACString& aReason) {
  FUZZING_LOG(("FuzzingWebSocketListener::OnServerClose"));
  return NS_OK;
}

NS_IMETHODIMP
FuzzingWebSocketListener::OnMessageAvailable(nsISupports* aContext,
                                             const nsACString& aMsg) {
  FUZZING_LOG(("FuzzingWebSocketListener::OnMessageAvailable"));
  return NS_OK;
}

NS_IMETHODIMP
FuzzingWebSocketListener::OnBinaryMessageAvailable(nsISupports* aContext,
                                                   const nsACString& aMsg) {
  FUZZING_LOG(("FuzzingWebSocketListener::OnBinaryMessageAvailable"));
  return NS_OK;
}

NS_IMETHODIMP
FuzzingWebSocketListener::OnError() {
  FUZZING_LOG(("FuzzingWebSocketListener::OnError"));
  return NS_OK;
}

static int FuzzingInitNetworkWebsocket(int* argc, char*** argv) {
  Preferences::SetBool("network.dns.native-is-localhost", true);
  Preferences::SetBool("fuzzing.necko.enabled", true);
  Preferences::SetBool("network.websocket.delay-failed-reconnects", false);
  Preferences::SetInt("network.http.speculative-parallel-limit", 0);
  Preferences::SetInt("network.proxy.type", 0);  // PROXYCONFIG_DIRECT
  return 0;
}

static int FuzzingInitNetworkWebsocketPlain(int* argc, char*** argv) {
  fuzzWSS = false;
  return FuzzingInitNetworkWebsocket(argc, argv);
}

static int FuzzingRunNetworkWebsocket(const uint8_t* data, size_t size) {
  // Set the data to be processed
  addNetworkFuzzingBuffer(data, size);

  nsWeakPtr channelRef;

  {
    nsresult rv;

    nsSecurityFlags secFlags;
    secFlags = nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL;
    uint32_t sandboxFlags = SANDBOXED_ORIGIN;

    nsCOMPtr<nsIURI> url;
    nsAutoCString spec;
    RefPtr<FuzzingWebSocketListener> gWebSocketListener;
    nsCOMPtr<nsIWebSocketChannel> gWebSocketChannel;

    if (fuzzWSS) {
      spec = "https://127.0.0.1/";
      gWebSocketChannel =
          do_CreateInstance("@mozilla.org/network/protocol;1?name=wss", &rv);
    } else {
      spec = "http://127.0.0.1/";
      gWebSocketChannel =
          do_CreateInstance("@mozilla.org/network/protocol;1?name=ws", &rv);
    }

    if (rv != NS_OK) {
      MOZ_CRASH("Failed to create WebSocketChannel");
    }

    if (NS_NewURI(getter_AddRefs(url), spec) != NS_OK) {
      MOZ_CRASH("Call to NS_NewURI failed.");
    }

    nsCOMPtr<nsIPrincipal> nullPrincipal =
        NullPrincipal::CreateWithoutOriginAttributes();

    rv = gWebSocketChannel->InitLoadInfoNative(
        nullptr, nullPrincipal, nsContentUtils::GetSystemPrincipal(), nullptr,
        secFlags, nsIContentPolicy::TYPE_WEBSOCKET, sandboxFlags);

    if (rv != NS_OK) {
      MOZ_CRASH("Failed to call InitLoadInfo");
    }

    gWebSocketListener = new FuzzingWebSocketListener();

    OriginAttributes attrs;
    rv = gWebSocketChannel->AsyncOpenNative(url, spec, attrs, 0,
                                            gWebSocketListener, nullptr);

    if (rv == NS_OK) {
      FUZZING_LOG(("Successful call to AsyncOpen"));

      // Wait for StartRequest or StopRequest
      gWebSocketListener->waitUntilDoneOrStarted();

      if (gWebSocketListener->isStarted()) {
        rv = gWebSocketChannel->SendBinaryMsg("Hello world"_ns);

        if (rv != NS_OK) {
          FUZZING_LOG(("Warning: Failed to call SendBinaryMsg"));
        } else {
          gWebSocketListener->waitUntilDoneOrAck();
        }

        rv = gWebSocketChannel->Close(1000, ""_ns);

        if (rv != NS_OK) {
          FUZZING_LOG(("Warning: Failed to call close"));
        }
      }

      // Wait for StopRequest
      gWebSocketListener->waitUntilDone();
    } else {
      FUZZING_LOG(("Warning: Failed to call AsyncOpen"));
    }

    channelRef = do_GetWeakReference(gWebSocketChannel);
  }

  // Wait for the channel to be destroyed
  SpinEventLoopUntil(
      "FuzzingRunNetworkWebsocket(channel == nullptr)"_ns, [&]() -> bool {
        nsCycleCollector_collect(CCReason::API, nullptr);
        nsCOMPtr<nsIWebSocketChannel> channel = do_QueryReferent(channelRef);
        return channel == nullptr;
      });

  if (!signalNetworkFuzzingDone()) {
    // Wait for the connection to indicate closed
    SpinEventLoopUntil("FuzzingRunNetworkWebsocket(gFuzzingConnClosed)"_ns,
                       [&]() -> bool { return gFuzzingConnClosed; });
  }

  return 0;
}

MOZ_FUZZING_INTERFACE_RAW(FuzzingInitNetworkWebsocket,
                          FuzzingRunNetworkWebsocket, NetworkWebsocket);
MOZ_FUZZING_INTERFACE_RAW(FuzzingInitNetworkWebsocketPlain,
                          FuzzingRunNetworkWebsocket, NetworkWebsocketPlain);

}  // namespace net
}  // namespace mozilla
