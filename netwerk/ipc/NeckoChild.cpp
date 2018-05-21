
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "necko-config.h"
#include "nsHttp.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/net/HttpChannelChild.h"
#include "mozilla/net/CookieServiceChild.h"
#include "mozilla/net/WyciwygChannelChild.h"
#include "mozilla/net/FTPChannelChild.h"
#include "mozilla/net/WebSocketChannelChild.h"
#include "mozilla/net/WebSocketEventListenerChild.h"
#include "mozilla/net/DNSRequestChild.h"
#include "mozilla/net/ChannelDiverterChild.h"
#include "mozilla/net/IPCTransportProvider.h"
#include "mozilla/dom/network/TCPSocketChild.h"
#include "mozilla/dom/network/TCPServerSocketChild.h"
#include "mozilla/dom/network/UDPSocketChild.h"
#include "mozilla/net/AltDataOutputStreamChild.h"
#ifdef MOZ_WEBRTC
#include "mozilla/net/StunAddrsRequestChild.h"
#endif

#include "SerializedLoadContext.h"
#include "nsGlobalWindow.h"
#include "nsIOService.h"
#include "nsINetworkPredictor.h"
#include "nsINetworkPredictorVerifier.h"
#include "mozilla/ipc/URIUtils.h"
#include "nsNetUtil.h"

using mozilla::dom::TCPSocketChild;
using mozilla::dom::TCPServerSocketChild;
using mozilla::dom::UDPSocketChild;

namespace mozilla {
namespace net {

PNeckoChild *gNeckoChild = nullptr;

// C++ file contents

NeckoChild::~NeckoChild()
{
  //Send__delete__(gNeckoChild);
  gNeckoChild = nullptr;
}

void NeckoChild::InitNeckoChild()
{
  MOZ_ASSERT(IsNeckoChild(), "InitNeckoChild called by non-child!");

  if (!gNeckoChild) {
    mozilla::dom::ContentChild * cpc =
      mozilla::dom::ContentChild::GetSingleton();
    NS_ASSERTION(cpc, "Content Protocol is NULL!");
    if (NS_WARN_IF(cpc->IsShuttingDown())) {
      return;
    }
    gNeckoChild = cpc->SendPNeckoConstructor();
    NS_ASSERTION(gNeckoChild, "PNecko Protocol init failed!");
  }
}

PHttpChannelChild*
NeckoChild::AllocPHttpChannelChild(const PBrowserOrId& browser,
                                   const SerializedLoadContext& loadContext,
                                   const HttpChannelCreationArgs& aOpenArgs)
{
  // We don't allocate here: instead we always use IPDL constructor that takes
  // an existing HttpChildChannel
  NS_NOTREACHED("AllocPHttpChannelChild should not be called on child");
  return nullptr;
}

bool
NeckoChild::DeallocPHttpChannelChild(PHttpChannelChild* channel)
{
  MOZ_ASSERT(IsNeckoChild(), "DeallocPHttpChannelChild called by non-child!");

  HttpChannelChild* child = static_cast<HttpChannelChild*>(channel);
  child->ReleaseIPDLReference();
  return true;
}

PStunAddrsRequestChild*
NeckoChild::AllocPStunAddrsRequestChild()
{
  // We don't allocate here: instead we always use IPDL constructor that takes
  // an existing object
  NS_NOTREACHED("AllocPStunAddrsRequestChild should not be called on child");
  return nullptr;
}

bool
NeckoChild::DeallocPStunAddrsRequestChild(PStunAddrsRequestChild* aActor)
{
#ifdef MOZ_WEBRTC
  StunAddrsRequestChild* p = static_cast<StunAddrsRequestChild*>(aActor);
  p->ReleaseIPDLReference();
#endif
  return true;
}

PAltDataOutputStreamChild*
NeckoChild::AllocPAltDataOutputStreamChild(
        const nsCString& type,
        const int64_t& predictedSize,
        PHttpChannelChild* channel)
{
  // We don't allocate here: see HttpChannelChild::OpenAlternativeOutputStream()
  NS_NOTREACHED("AllocPAltDataOutputStreamChild should not be called");
  return nullptr;
}

bool
NeckoChild::DeallocPAltDataOutputStreamChild(PAltDataOutputStreamChild* aActor)
{
  AltDataOutputStreamChild* child = static_cast<AltDataOutputStreamChild*>(aActor);
  child->ReleaseIPDLReference();
  return true;
}

PFTPChannelChild*
NeckoChild::AllocPFTPChannelChild(const PBrowserOrId& aBrowser,
                                  const SerializedLoadContext& aSerialized,
                                  const FTPChannelCreationArgs& aOpenArgs)
{
  // We don't allocate here: see FTPChannelChild::AsyncOpen()
  MOZ_CRASH("AllocPFTPChannelChild should not be called");
  return nullptr;
}

bool
NeckoChild::DeallocPFTPChannelChild(PFTPChannelChild* channel)
{
  MOZ_ASSERT(IsNeckoChild(), "DeallocPFTPChannelChild called by non-child!");

  FTPChannelChild* child = static_cast<FTPChannelChild*>(channel);
  child->ReleaseIPDLReference();
  return true;
}

PCookieServiceChild*
NeckoChild::AllocPCookieServiceChild()
{
  // We don't allocate here: see nsCookieService::GetSingleton()
  NS_NOTREACHED("AllocPCookieServiceChild should not be called");
  return nullptr;
}

bool
NeckoChild::DeallocPCookieServiceChild(PCookieServiceChild* cs)
{
  NS_ASSERTION(IsNeckoChild(), "DeallocPCookieServiceChild called by non-child!");

  CookieServiceChild *p = static_cast<CookieServiceChild*>(cs);
  p->Release();
  return true;
}

PWyciwygChannelChild*
NeckoChild::AllocPWyciwygChannelChild()
{
  // We don't allocate here: see nsWyciwygProtocolHandler::NewChannel2()
  NS_NOTREACHED("AllocPWyciwygChannelChild should not be called");
  return nullptr;
}

bool
NeckoChild::DeallocPWyciwygChannelChild(PWyciwygChannelChild* channel)
{
  MOZ_ASSERT(IsNeckoChild(), "DeallocPWyciwygChannelChild called by non-child!");

  WyciwygChannelChild *p = static_cast<WyciwygChannelChild*>(channel);
  p->ReleaseIPDLReference();
  return true;
}

PWebSocketChild*
NeckoChild::AllocPWebSocketChild(const PBrowserOrId& browser,
                                 const SerializedLoadContext& aSerialized,
                                 const uint32_t& aSerial)
{
  NS_NOTREACHED("AllocPWebSocketChild should not be called");
  return nullptr;
}

bool
NeckoChild::DeallocPWebSocketChild(PWebSocketChild* child)
{
  WebSocketChannelChild* p = static_cast<WebSocketChannelChild*>(child);
  p->ReleaseIPDLReference();
  return true;
}

PWebSocketEventListenerChild*
NeckoChild::AllocPWebSocketEventListenerChild(const uint64_t& aInnerWindowID)
{
  nsCOMPtr<nsIEventTarget> target;
  if (nsGlobalWindowInner* win = nsGlobalWindowInner::GetInnerWindowWithId(aInnerWindowID)) {
    target = win->EventTargetFor(TaskCategory::Other);
  }

  RefPtr<WebSocketEventListenerChild> c =
    new WebSocketEventListenerChild(aInnerWindowID, target);

  if (target) {
    gNeckoChild->SetEventTargetForActor(c, target);
  }

  return c.forget().take();
}

bool
NeckoChild::DeallocPWebSocketEventListenerChild(PWebSocketEventListenerChild* aActor)
{
  RefPtr<WebSocketEventListenerChild> c =
    dont_AddRef(static_cast<WebSocketEventListenerChild*>(aActor));
  MOZ_ASSERT(c);
  return true;
}

PDataChannelChild*
NeckoChild::AllocPDataChannelChild(const uint32_t& channelId)
{
  MOZ_ASSERT_UNREACHABLE("Should never get here");
  return nullptr;
}

bool
NeckoChild::DeallocPDataChannelChild(PDataChannelChild* child)
{
  // NB: See DataChannelChild::ActorDestroy.
  return true;
}

PFileChannelChild*
NeckoChild::AllocPFileChannelChild(const uint32_t& channelId)
{
  MOZ_ASSERT_UNREACHABLE("Should never get here");
  return nullptr;
}

bool
NeckoChild::DeallocPFileChannelChild(PFileChannelChild* child)
{
  // NB: See FileChannelChild::ActorDestroy.
  return true;
}

PSimpleChannelChild*
NeckoChild::AllocPSimpleChannelChild(const uint32_t& channelId)
{
  MOZ_ASSERT_UNREACHABLE("Should never get here");
  return nullptr;
}

bool
NeckoChild::DeallocPSimpleChannelChild(PSimpleChannelChild* child)
{
  // NB: See SimpleChannelChild::ActorDestroy.
  return true;
}

PTCPSocketChild*
NeckoChild::AllocPTCPSocketChild(const nsString& host,
                                 const uint16_t& port)
{
  TCPSocketChild* p = new TCPSocketChild(host, port, nullptr);
  p->AddIPDLReference();
  return p;
}

bool
NeckoChild::DeallocPTCPSocketChild(PTCPSocketChild* child)
{
  TCPSocketChild* p = static_cast<TCPSocketChild*>(child);
  p->ReleaseIPDLReference();
  return true;
}

PTCPServerSocketChild*
NeckoChild::AllocPTCPServerSocketChild(const uint16_t& aLocalPort,
                                  const uint16_t& aBacklog,
                                  const bool& aUseArrayBuffers)
{
  NS_NOTREACHED("AllocPTCPServerSocket should not be called");
  return nullptr;
}

bool
NeckoChild::DeallocPTCPServerSocketChild(PTCPServerSocketChild* child)
{
  TCPServerSocketChild* p = static_cast<TCPServerSocketChild*>(child);
  p->ReleaseIPDLReference();
  return true;
}

PUDPSocketChild*
NeckoChild::AllocPUDPSocketChild(const Principal& aPrincipal,
                                 const nsCString& aFilter)
{
  NS_NOTREACHED("AllocPUDPSocket should not be called");
  return nullptr;
}

bool
NeckoChild::DeallocPUDPSocketChild(PUDPSocketChild* child)
{

  UDPSocketChild* p = static_cast<UDPSocketChild*>(child);
  p->ReleaseIPDLReference();
  return true;
}

PDNSRequestChild*
NeckoChild::AllocPDNSRequestChild(const nsCString& aHost,
                                  const OriginAttributes& aOriginAttributes,
                                  const uint32_t& aFlags)
{
  // We don't allocate here: instead we always use IPDL constructor that takes
  // an existing object
  NS_NOTREACHED("AllocPDNSRequestChild should not be called on child");
  return nullptr;
}

bool
NeckoChild::DeallocPDNSRequestChild(PDNSRequestChild* aChild)
{
  DNSRequestChild *p = static_cast<DNSRequestChild*>(aChild);
  p->ReleaseIPDLReference();
  return true;
}

PChannelDiverterChild*
NeckoChild::AllocPChannelDiverterChild(const ChannelDiverterArgs& channel)
{
  return new ChannelDiverterChild();;
}

bool
NeckoChild::DeallocPChannelDiverterChild(PChannelDiverterChild* child)
{
  delete static_cast<ChannelDiverterChild*>(child);
  return true;
}

PTransportProviderChild*
NeckoChild::AllocPTransportProviderChild()
{
  // This refcount is transferred to the receiver of the message that
  // includes the PTransportProviderChild actor.
  RefPtr<TransportProviderChild> res = new TransportProviderChild();

  return res.forget().take();
}

bool
NeckoChild::DeallocPTransportProviderChild(PTransportProviderChild* aActor)
{
  return true;
}

mozilla::ipc::IPCResult
NeckoChild::RecvAsyncAuthPromptForNestedFrame(const TabId& aNestedFrameId,
                                              const nsCString& aUri,
                                              const nsString& aRealm,
                                              const uint64_t& aCallbackId)
{
  RefPtr<dom::TabChild> tabChild = dom::TabChild::FindTabChild(aNestedFrameId);
  if (!tabChild) {
    MOZ_CRASH();
    return IPC_FAIL_NO_REASON(this);
  }
  tabChild->SendAsyncAuthPrompt(aUri, aRealm, aCallbackId);
  return IPC_OK();
}

/* Predictor Messages */
mozilla::ipc::IPCResult
NeckoChild::RecvPredOnPredictPrefetch(const URIParams& aURI,
                                      const uint32_t& aHttpStatus)
{
  MOZ_ASSERT(NS_IsMainThread(), "PredictorChild::RecvOnPredictPrefetch "
                                "off main thread.");

  nsCOMPtr<nsIURI> uri = DeserializeURI(aURI);

  // Get the current predictor
  nsresult rv = NS_OK;
  nsCOMPtr<nsINetworkPredictorVerifier> predictor =
    do_GetService("@mozilla.org/network/predictor;1", &rv);
  NS_ENSURE_SUCCESS(rv, IPC_FAIL_NO_REASON(this));

  predictor->OnPredictPrefetch(uri, aHttpStatus);
  return IPC_OK();
}

mozilla::ipc::IPCResult
NeckoChild::RecvPredOnPredictPreconnect(const URIParams& aURI)
{
  MOZ_ASSERT(NS_IsMainThread(), "PredictorChild::RecvOnPredictPreconnect "
                                "off main thread.");

  nsCOMPtr<nsIURI> uri = DeserializeURI(aURI);

  // Get the current predictor
  nsresult rv = NS_OK;
  nsCOMPtr<nsINetworkPredictorVerifier> predictor =
    do_GetService("@mozilla.org/network/predictor;1", &rv);
  NS_ENSURE_SUCCESS(rv, IPC_FAIL_NO_REASON(this));

  predictor->OnPredictPreconnect(uri);
  return IPC_OK();
}

mozilla::ipc::IPCResult
NeckoChild::RecvPredOnPredictDNS(const URIParams& aURI)
{
  MOZ_ASSERT(NS_IsMainThread(), "PredictorChild::RecvOnPredictDNS off "
                                "main thread.");

  nsCOMPtr<nsIURI> uri = DeserializeURI(aURI);

  // Get the current predictor
  nsresult rv = NS_OK;
  nsCOMPtr<nsINetworkPredictorVerifier> predictor =
    do_GetService("@mozilla.org/network/predictor;1", &rv);
  NS_ENSURE_SUCCESS(rv, IPC_FAIL_NO_REASON(this));

  predictor->OnPredictDNS(uri);
  return IPC_OK();
}

mozilla::ipc::IPCResult
NeckoChild::RecvSpeculativeConnectRequest()
{
  nsCOMPtr<nsIObserverService> obsService = services::GetObserverService();
  if (obsService) {
    obsService->NotifyObservers(nullptr, "speculative-connect-request",
                                nullptr);
  }
  return IPC_OK();
}

} // namespace net
} // namespace mozilla

