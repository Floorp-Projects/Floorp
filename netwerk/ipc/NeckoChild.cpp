
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHttp.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/net/HttpChannelChild.h"
#include "mozilla/net/ChildDNSService.h"
#include "mozilla/net/CookieServiceChild.h"
#include "mozilla/net/DataChannelChild.h"
#ifdef MOZ_WIDGET_GTK
#  include "mozilla/net/GIOChannelChild.h"
#endif
#include "mozilla/net/FileChannelChild.h"
#include "mozilla/net/WebSocketChannelChild.h"
#include "mozilla/net/WebSocketEventListenerChild.h"
#include "mozilla/net/DNSRequestChild.h"
#include "mozilla/net/IPCTransportProvider.h"
#include "mozilla/dom/network/TCPSocketChild.h"
#include "mozilla/dom/network/TCPServerSocketChild.h"
#include "mozilla/dom/network/UDPSocketChild.h"
#include "mozilla/net/AltDataOutputStreamChild.h"
#include "mozilla/net/SocketProcessBridgeChild.h"
#ifdef MOZ_WEBRTC
#  include "mozilla/net/StunAddrsRequestChild.h"
#  include "mozilla/net/WebrtcTCPSocketChild.h"
#endif

#include "SerializedLoadContext.h"
#include "nsGlobalWindowInner.h"
#include "nsIOService.h"
#include "nsINetworkPredictor.h"
#include "nsINetworkPredictorVerifier.h"
#include "nsINetworkLinkService.h"
#include "nsQueryObject.h"
#include "mozilla/ipc/URIUtils.h"
#include "nsNetUtil.h"
#include "SimpleChannel.h"

using mozilla::dom::TCPServerSocketChild;
using mozilla::dom::TCPSocketChild;
using mozilla::dom::UDPSocketChild;

namespace mozilla {
namespace net {

PNeckoChild* gNeckoChild = nullptr;

// C++ file contents

NeckoChild::~NeckoChild() {
  // Send__delete__(gNeckoChild);
  gNeckoChild = nullptr;
}

void NeckoChild::InitNeckoChild() {
  if (!IsNeckoChild()) {
    MOZ_ASSERT(false, "InitNeckoChild called by non-child!");
    return;
  }

  if (!gNeckoChild) {
    mozilla::dom::ContentChild* cpc =
        mozilla::dom::ContentChild::GetSingleton();
    NS_ASSERTION(cpc, "Content Protocol is NULL!");
    if (NS_WARN_IF(cpc->IsShuttingDown())) {
      return;
    }
    RefPtr<NeckoChild> child = new NeckoChild();
    gNeckoChild = cpc->SendPNeckoConstructor(child);
    NS_ASSERTION(gNeckoChild, "PNecko Protocol init failed!");
  }
}

PStunAddrsRequestChild* NeckoChild::AllocPStunAddrsRequestChild() {
  // We don't allocate here: instead we always use IPDL constructor that takes
  // an existing object
  MOZ_ASSERT_UNREACHABLE(
      "AllocPStunAddrsRequestChild should not be called "
      "on child");
  return nullptr;
}

bool NeckoChild::DeallocPStunAddrsRequestChild(PStunAddrsRequestChild* aActor) {
#ifdef MOZ_WEBRTC
  StunAddrsRequestChild* p = static_cast<StunAddrsRequestChild*>(aActor);
  p->ReleaseIPDLReference();
#endif
  return true;
}

PWebrtcTCPSocketChild* NeckoChild::AllocPWebrtcTCPSocketChild(
    const Maybe<TabId>& tabId) {
  // We don't allocate here: instead we always use IPDL constructor that takes
  // an existing object
  MOZ_ASSERT_UNREACHABLE(
      "AllocPWebrtcTCPSocketChild should not be called on"
      " child");
  return nullptr;
}

bool NeckoChild::DeallocPWebrtcTCPSocketChild(PWebrtcTCPSocketChild* aActor) {
#ifdef MOZ_WEBRTC
  WebrtcTCPSocketChild* child = static_cast<WebrtcTCPSocketChild*>(aActor);
  child->ReleaseIPDLReference();
#endif
  return true;
}

PAltDataOutputStreamChild* NeckoChild::AllocPAltDataOutputStreamChild(
    const nsACString& type, const int64_t& predictedSize,
    PHttpChannelChild* channel) {
  // We don't allocate here: see HttpChannelChild::OpenAlternativeOutputStream()
  MOZ_ASSERT_UNREACHABLE("AllocPAltDataOutputStreamChild should not be called");
  return nullptr;
}

bool NeckoChild::DeallocPAltDataOutputStreamChild(
    PAltDataOutputStreamChild* aActor) {
  AltDataOutputStreamChild* child =
      static_cast<AltDataOutputStreamChild*>(aActor);
  child->ReleaseIPDLReference();
  return true;
}

#ifdef MOZ_WIDGET_GTK
PGIOChannelChild* NeckoChild::AllocPGIOChannelChild(
    PBrowserChild* aBrowser, const SerializedLoadContext& aSerialized,
    const GIOChannelCreationArgs& aOpenArgs) {
  // We don't allocate here: see GIOChannelChild::AsyncOpen()
  MOZ_CRASH("AllocPGIOChannelChild should not be called");
  return nullptr;
}

bool NeckoChild::DeallocPGIOChannelChild(PGIOChannelChild* channel) {
  MOZ_ASSERT(IsNeckoChild(), "DeallocPGIOChannelChild called by non-child!");

  GIOChannelChild* child = static_cast<GIOChannelChild*>(channel);
  child->ReleaseIPDLReference();
  return true;
}
#endif

PCookieServiceChild* NeckoChild::AllocPCookieServiceChild() {
  // We don't allocate here: see CookieService::GetSingleton()
  MOZ_ASSERT_UNREACHABLE("AllocPCookieServiceChild should not be called");
  return nullptr;
}

bool NeckoChild::DeallocPCookieServiceChild(PCookieServiceChild* cs) {
  NS_ASSERTION(IsNeckoChild(),
               "DeallocPCookieServiceChild called by non-child!");

  CookieServiceChild* p = static_cast<CookieServiceChild*>(cs);
  p->Release();
  return true;
}

PWebSocketChild* NeckoChild::AllocPWebSocketChild(
    PBrowserChild* browser, const SerializedLoadContext& aSerialized,
    const uint32_t& aSerial) {
  MOZ_ASSERT_UNREACHABLE("AllocPWebSocketChild should not be called");
  return nullptr;
}

bool NeckoChild::DeallocPWebSocketChild(PWebSocketChild* child) {
  WebSocketChannelChild* p = static_cast<WebSocketChannelChild*>(child);
  p->ReleaseIPDLReference();
  return true;
}

PWebSocketEventListenerChild* NeckoChild::AllocPWebSocketEventListenerChild(
    const uint64_t& aInnerWindowID) {
  RefPtr<WebSocketEventListenerChild> c = new WebSocketEventListenerChild(
      aInnerWindowID, GetMainThreadSerialEventTarget());
  return c.forget().take();
}

bool NeckoChild::DeallocPWebSocketEventListenerChild(
    PWebSocketEventListenerChild* aActor) {
  RefPtr<WebSocketEventListenerChild> c =
      dont_AddRef(static_cast<WebSocketEventListenerChild*>(aActor));
  MOZ_ASSERT(c);
  return true;
}

PSimpleChannelChild* NeckoChild::AllocPSimpleChannelChild(
    const uint32_t& channelId) {
  MOZ_ASSERT_UNREACHABLE("Should never get here");
  return nullptr;
}

bool NeckoChild::DeallocPSimpleChannelChild(PSimpleChannelChild* child) {
  static_cast<SimpleChannelChild*>(child)->Release();
  return true;
}

PTCPSocketChild* NeckoChild::AllocPTCPSocketChild(const nsAString& host,
                                                  const uint16_t& port) {
  TCPSocketChild* p = new TCPSocketChild(host, port, nullptr);
  p->AddIPDLReference();
  return p;
}

bool NeckoChild::DeallocPTCPSocketChild(PTCPSocketChild* child) {
  TCPSocketChild* p = static_cast<TCPSocketChild*>(child);
  p->ReleaseIPDLReference();
  return true;
}

PTCPServerSocketChild* NeckoChild::AllocPTCPServerSocketChild(
    const uint16_t& aLocalPort, const uint16_t& aBacklog,
    const bool& aUseArrayBuffers) {
  MOZ_ASSERT_UNREACHABLE("AllocPTCPServerSocket should not be called");
  return nullptr;
}

bool NeckoChild::DeallocPTCPServerSocketChild(PTCPServerSocketChild* child) {
  TCPServerSocketChild* p = static_cast<TCPServerSocketChild*>(child);
  p->ReleaseIPDLReference();
  return true;
}

PUDPSocketChild* NeckoChild::AllocPUDPSocketChild(nsIPrincipal* aPrincipal,
                                                  const nsACString& aFilter) {
  MOZ_ASSERT_UNREACHABLE("AllocPUDPSocket should not be called");
  return nullptr;
}

bool NeckoChild::DeallocPUDPSocketChild(PUDPSocketChild* child) {
  UDPSocketChild* p = static_cast<UDPSocketChild*>(child);
  p->ReleaseIPDLReference();
  return true;
}

PTransportProviderChild* NeckoChild::AllocPTransportProviderChild() {
  // This refcount is transferred to the receiver of the message that
  // includes the PTransportProviderChild actor.
  RefPtr<TransportProviderChild> res = new TransportProviderChild();

  return res.forget().take();
}

bool NeckoChild::DeallocPTransportProviderChild(
    PTransportProviderChild* aActor) {
  return true;
}

/* Predictor Messages */
mozilla::ipc::IPCResult NeckoChild::RecvPredOnPredictPrefetch(
    nsIURI* aURI, const uint32_t& aHttpStatus) {
  MOZ_ASSERT(NS_IsMainThread(),
             "PredictorChild::RecvOnPredictPrefetch "
             "off main thread.");
  if (!aURI) {
    return IPC_FAIL(this, "aURI is null");
  }

  // Get the current predictor
  nsresult rv = NS_OK;
  nsCOMPtr<nsINetworkPredictorVerifier> predictor =
      do_GetService("@mozilla.org/network/predictor;1", &rv);
  NS_ENSURE_SUCCESS(rv, IPC_FAIL_NO_REASON(this));

  predictor->OnPredictPrefetch(aURI, aHttpStatus);
  return IPC_OK();
}

mozilla::ipc::IPCResult NeckoChild::RecvPredOnPredictPreconnect(nsIURI* aURI) {
  MOZ_ASSERT(NS_IsMainThread(),
             "PredictorChild::RecvOnPredictPreconnect "
             "off main thread.");
  if (!aURI) {
    return IPC_FAIL(this, "aURI is null");
  }
  // Get the current predictor
  nsresult rv = NS_OK;
  nsCOMPtr<nsINetworkPredictorVerifier> predictor =
      do_GetService("@mozilla.org/network/predictor;1", &rv);
  NS_ENSURE_SUCCESS(rv, IPC_FAIL_NO_REASON(this));

  predictor->OnPredictPreconnect(aURI);
  return IPC_OK();
}

mozilla::ipc::IPCResult NeckoChild::RecvPredOnPredictDNS(nsIURI* aURI) {
  MOZ_ASSERT(NS_IsMainThread(),
             "PredictorChild::RecvOnPredictDNS off "
             "main thread.");
  if (!aURI) {
    return IPC_FAIL(this, "aURI is null");
  }
  // Get the current predictor
  nsresult rv = NS_OK;
  nsCOMPtr<nsINetworkPredictorVerifier> predictor =
      do_GetService("@mozilla.org/network/predictor;1", &rv);
  NS_ENSURE_SUCCESS(rv, IPC_FAIL_NO_REASON(this));

  predictor->OnPredictDNS(aURI);
  return IPC_OK();
}

mozilla::ipc::IPCResult NeckoChild::RecvSpeculativeConnectRequest() {
  nsCOMPtr<nsIObserverService> obsService = services::GetObserverService();
  if (obsService) {
    obsService->NotifyObservers(nullptr, "speculative-connect-request",
                                nullptr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult NeckoChild::RecvNetworkChangeNotification(
    nsCString const& type) {
  nsCOMPtr<nsIObserverService> obsService = services::GetObserverService();
  if (obsService) {
    obsService->NotifyObservers(nullptr, NS_NETWORK_LINK_TOPIC,
                                NS_ConvertUTF8toUTF16(type).get());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult NeckoChild::RecvSetTRRDomain(const nsCString& domain) {
  RefPtr<net::ChildDNSService> dnsServiceChild =
      dont_AddRef(net::ChildDNSService::GetSingleton());
  if (dnsServiceChild) {
    dnsServiceChild->SetTRRDomain(domain);
  }
  return IPC_OK();
}

}  // namespace net
}  // namespace mozilla
