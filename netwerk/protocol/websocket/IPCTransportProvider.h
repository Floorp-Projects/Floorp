/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_IPCTransportProvider_h
#define mozilla_net_IPCTransportProvider_h

#include "nsISupportsImpl.h"
#include "mozilla/net/PTransportProviderParent.h"
#include "mozilla/net/PTransportProviderChild.h"
#include "nsIHttpChannelInternal.h"
#include "nsITransportProvider.h"

/*
 * No, the ownership model for TransportProvider is that the child object is
 * refcounted "normally". I.e. ipdl code doesn't hold a strong reference to
 * TransportProviderChild.
 *
 * When TransportProviderChild goes away, it sends a __delete__ message to the
 * parent.
 *
 * On the parent side, ipdl holds a strong reference to TransportProviderParent.
 * When the actor is deallocatde it releases the reference to the
 * TransportProviderParent.
 *
 * So effectively the child holds a strong reference to the parent, and are
 * otherwise normally refcounted and have their lifetime determined by that
 * refcount.
 *
 * The only other caveat is that the creation happens from the parent.
 * So to create a TransportProvider, a constructor is sent from the parent to
 * the child. At this time the child gets its first addref.
 *
 * A reference to the TransportProvider is then sent as part of some other
 * message from the parent to the child.
 *
 * The receiver of that message can then grab the TransportProviderChild and
 * without addreffing it, effectively using the refcount that the
 * TransportProviderChild got on creation.
 */

class nsISocketTransport;
class nsIAsyncInputStream;
class nsIAsyncOutputStream;

namespace mozilla {
namespace net {

class TransportProviderParent final : public PTransportProviderParent,
                                      public nsITransportProvider,
                                      public nsIHttpUpgradeListener {
 public:
  TransportProviderParent() = default;

  NS_DECL_ISUPPORTS
  NS_DECL_NSITRANSPORTPROVIDER
  NS_DECL_NSIHTTPUPGRADELISTENER

  void ActorDestroy(ActorDestroyReason aWhy) override {}

 private:
  ~TransportProviderParent() = default;

  void MaybeNotify();

  nsCOMPtr<nsIHttpUpgradeListener> mListener;
  nsCOMPtr<nsISocketTransport> mTransport;
  nsCOMPtr<nsIAsyncInputStream> mSocketIn;
  nsCOMPtr<nsIAsyncOutputStream> mSocketOut;
};

class TransportProviderChild final : public PTransportProviderChild,
                                     public nsITransportProvider {
 public:
  TransportProviderChild() = default;

  NS_DECL_ISUPPORTS
  NS_DECL_NSITRANSPORTPROVIDER

 private:
  ~TransportProviderChild();
};

}  // namespace net
}  // namespace mozilla

#endif
