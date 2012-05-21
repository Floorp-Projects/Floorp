/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_BaseWebSocketChannel_h
#define mozilla_net_BaseWebSocketChannel_h

#include "nsIWebSocketChannel.h"
#include "nsIWebSocketListener.h"
#include "nsIProtocolHandler.h"
#include "nsCOMPtr.h"
#include "nsString.h"

namespace mozilla {
namespace net {

const static PRInt32 kDefaultWSPort     = 80;
const static PRInt32 kDefaultWSSPort    = 443;

class BaseWebSocketChannel : public nsIWebSocketChannel,
                             public nsIProtocolHandler
{
 public:
  BaseWebSocketChannel();

  NS_DECL_NSIPROTOCOLHANDLER

  NS_IMETHOD QueryInterface(const nsIID & uuid, void **result NS_OUTPARAM) = 0;
  NS_IMETHOD_(nsrefcnt ) AddRef(void) = 0;
  NS_IMETHOD_(nsrefcnt ) Release(void) = 0;

  // Partial implementation of nsIWebSocketChannel
  //
  NS_IMETHOD GetOriginalURI(nsIURI **aOriginalURI);
  NS_IMETHOD GetURI(nsIURI **aURI);
  NS_IMETHOD GetNotificationCallbacks(nsIInterfaceRequestor **aNotificationCallbacks);
  NS_IMETHOD SetNotificationCallbacks(nsIInterfaceRequestor *aNotificationCallbacks);
  NS_IMETHOD GetLoadGroup(nsILoadGroup **aLoadGroup);
  NS_IMETHOD SetLoadGroup(nsILoadGroup *aLoadGroup);
  NS_IMETHOD GetExtensions(nsACString &aExtensions);
  NS_IMETHOD GetProtocol(nsACString &aProtocol);
  NS_IMETHOD SetProtocol(const nsACString &aProtocol);

 protected:
  nsCOMPtr<nsIURI>                mOriginalURI;
  nsCOMPtr<nsIURI>                mURI;
  nsCOMPtr<nsIWebSocketListener>  mListener;
  nsCOMPtr<nsISupports>           mContext;
  nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
  nsCOMPtr<nsILoadGroup>          mLoadGroup;

  nsCString                       mProtocol;
  nsCString                       mOrigin;

  bool                            mEncrypted;
  nsCString                       mNegotiatedExtensions;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_BaseWebSocketChannel_h
