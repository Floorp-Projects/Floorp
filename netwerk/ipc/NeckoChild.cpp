
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
#include "mozilla/net/DNSRequestChild.h"
#include "mozilla/net/RemoteOpenFileChild.h"
#include "mozilla/net/ChannelDiverterChild.h"
#include "mozilla/dom/network/TCPSocketChild.h"
#include "mozilla/dom/network/TCPServerSocketChild.h"
#include "mozilla/dom/network/UDPSocketChild.h"
#ifdef NECKO_PROTOCOL_rtsp
#include "mozilla/net/RtspControllerChild.h"
#include "mozilla/net/RtspChannelChild.h"
#endif
#include "SerializedLoadContext.h"
#include "nsIOService.h"

using mozilla::dom::TCPSocketChild;
using mozilla::dom::TCPServerSocketChild;
using mozilla::dom::UDPSocketChild;

namespace mozilla {
namespace net {

PNeckoChild *gNeckoChild = nullptr;

// C++ file contents
NeckoChild::NeckoChild()
{
}

NeckoChild::~NeckoChild()
{
}

void NeckoChild::InitNeckoChild()
{
  MOZ_ASSERT(IsNeckoChild(), "InitNeckoChild called by non-child!");

  if (!gNeckoChild) {
    mozilla::dom::ContentChild * cpc = 
      mozilla::dom::ContentChild::GetSingleton();
    NS_ASSERTION(cpc, "Content Protocol is NULL!");
    gNeckoChild = cpc->SendPNeckoConstructor(); 
    NS_ASSERTION(gNeckoChild, "PNecko Protocol init failed!");
  }
}

// Note: not actually called; has some lifespan as child process, so
// automatically destroyed at exit.  
void NeckoChild::DestroyNeckoChild()
{
  MOZ_ASSERT(IsNeckoChild(), "DestroyNeckoChild called by non-child!");
  static bool alreadyDestroyed = false;
  MOZ_ASSERT(!alreadyDestroyed, "DestroyNeckoChild already called!");

  if (!alreadyDestroyed) {
    Send__delete__(gNeckoChild); 
    gNeckoChild = nullptr;
    alreadyDestroyed = true;
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

PFTPChannelChild*
NeckoChild::AllocPFTPChannelChild(const PBrowserOrId& aBrowser,
                                  const SerializedLoadContext& aSerialized,
                                  const FTPChannelCreationArgs& aOpenArgs)
{
  // We don't allocate here: see FTPChannelChild::AsyncOpen()
  NS_RUNTIMEABORT("AllocPFTPChannelChild should not be called");
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
  WyciwygChannelChild *p = new WyciwygChannelChild();
  p->AddIPDLReference();
  return p;
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
                                 const SerializedLoadContext& aSerialized)
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

PRtspControllerChild*
NeckoChild::AllocPRtspControllerChild()
{
  NS_NOTREACHED("AllocPRtspController should not be called");
  return nullptr;
}

bool
NeckoChild::DeallocPRtspControllerChild(PRtspControllerChild* child)
{
#ifdef NECKO_PROTOCOL_rtsp
  RtspControllerChild* p = static_cast<RtspControllerChild*>(child);
  p->ReleaseIPDLReference();
#endif
  return true;
}

PRtspChannelChild*
NeckoChild::AllocPRtspChannelChild(const RtspChannelConnectArgs& aArgs)
{
  NS_NOTREACHED("AllocPRtspController should not be called");
  return nullptr;
}

bool
NeckoChild::DeallocPRtspChannelChild(PRtspChannelChild* child)
{
#ifdef NECKO_PROTOCOL_rtsp
  RtspChannelChild* p = static_cast<RtspChannelChild*>(child);
  p->ReleaseIPDLReference();
#endif
  return true;
}

PTCPSocketChild*
NeckoChild::AllocPTCPSocketChild(const nsString& host,
                                 const uint16_t& port)
{
  TCPSocketChild* p = new TCPSocketChild();
  p->Init(host, port);
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
                                  const nsString& aBinaryType)
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
                                  const uint32_t& aFlags,
                                  const nsCString& aNetworkInterface)
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

PRemoteOpenFileChild*
NeckoChild::AllocPRemoteOpenFileChild(const SerializedLoadContext& aSerialized,
                                      const URIParams&,
                                      const OptionalURIParams&)
{
  // We don't allocate here: instead we always use IPDL constructor that takes
  // an existing RemoteOpenFileChild
  NS_NOTREACHED("AllocPRemoteOpenFileChild should not be called on child");
  return nullptr;
}

bool
NeckoChild::DeallocPRemoteOpenFileChild(PRemoteOpenFileChild* aChild)
{
  RemoteOpenFileChild *p = static_cast<RemoteOpenFileChild*>(aChild);
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

bool
NeckoChild::RecvAsyncAuthPromptForNestedFrame(const TabId& aNestedFrameId,
                                              const nsCString& aUri,
                                              const nsString& aRealm,
                                              const uint64_t& aCallbackId)
{
  nsRefPtr<dom::TabChild> tabChild = dom::TabChild::FindTabChild(aNestedFrameId);
  if (!tabChild) {
    MOZ_CRASH();
    return false;
  }
  tabChild->SendAsyncAuthPrompt(aUri, aRealm, aCallbackId);
  return true;
}

bool
NeckoChild::RecvAppOfflineStatus(const uint32_t& aId, const bool& aOffline)
{
  // Instantiate the service to make sure gIOService is initialized
  nsCOMPtr<nsIIOService> ioService = do_GetIOService();
  if (gIOService) {
    gIOService->SetAppOfflineInternal(aId, aOffline ?
      nsIAppOfflineInfo::OFFLINE : nsIAppOfflineInfo::ONLINE);
  }
  return true;
}

}} // mozilla::net

