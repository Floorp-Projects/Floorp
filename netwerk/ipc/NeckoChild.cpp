/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHttp.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/net/HttpChannelChild.h"
#include "mozilla/net/CookieServiceChild.h"
#include "mozilla/net/WyciwygChannelChild.h"
#include "mozilla/net/FTPChannelChild.h"
#include "mozilla/net/WebSocketChannelChild.h"
#include "mozilla/net/RemoteOpenFileChild.h"
#include "mozilla/dom/network/TCPSocketChild.h"
#include "mozilla/dom/network/TCPServerSocketChild.h"

using mozilla::dom::TCPSocketChild;
using mozilla::dom::TCPServerSocketChild;

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
  NS_ABORT_IF_FALSE(IsNeckoChild(), "InitNeckoChild called by non-child!");

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
  NS_ABORT_IF_FALSE(IsNeckoChild(), "DestroyNeckoChild called by non-child!");
  static bool alreadyDestroyed = false;
  NS_ABORT_IF_FALSE(!alreadyDestroyed, "DestroyNeckoChild already called!");

  if (!alreadyDestroyed) {
    Send__delete__(gNeckoChild); 
    gNeckoChild = nullptr;
    alreadyDestroyed = true;
  }
}

PHttpChannelChild*
NeckoChild::AllocPHttpChannelChild(PBrowserChild* browser,
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
  NS_ABORT_IF_FALSE(IsNeckoChild(), "DeallocPHttpChannelChild called by non-child!");

  HttpChannelChild* child = static_cast<HttpChannelChild*>(channel);
  child->ReleaseIPDLReference();
  return true;
}

PFTPChannelChild*
NeckoChild::AllocPFTPChannelChild(PBrowserChild* aBrowser,
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
  NS_ABORT_IF_FALSE(IsNeckoChild(), "DeallocPFTPChannelChild called by non-child!");

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
  NS_ABORT_IF_FALSE(IsNeckoChild(), "DeallocPWyciwygChannelChild called by non-child!");

  WyciwygChannelChild *p = static_cast<WyciwygChannelChild*>(channel);
  p->ReleaseIPDLReference();
  return true;
}

PWebSocketChild*
NeckoChild::AllocPWebSocketChild(PBrowserChild* browser,
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

PTCPSocketChild*
NeckoChild::AllocPTCPSocketChild()
{
  TCPSocketChild* p = new TCPSocketChild();
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

PRemoteOpenFileChild*
NeckoChild::AllocPRemoteOpenFileChild(const URIParams&)
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

}} // mozilla::net

