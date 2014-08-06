/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_HttpChannelCallbackWrapper_h
#define mozilla_net_HttpChannelCallbackWrapper_h

#include "nsIInterfaceRequestor.h"
#include "nsIChannelEventSink.h"
#include "nsIRedirectResultListener.h"

class nsIParentChannel;

namespace mozilla {
namespace net {

class HttpChannelParent;

class HttpChannelParentListener : public nsIInterfaceRequestor
                                 , public nsIChannelEventSink
                                 , public nsIRedirectResultListener
                                 , public nsIStreamListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIREDIRECTRESULTLISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  explicit HttpChannelParentListener(HttpChannelParent* aInitialChannel);

  // For channel diversion from child to parent.
  nsresult DivertTo(nsIStreamListener *aListener);
  nsresult SuspendForDiversion();

private:
  virtual ~HttpChannelParentListener();

  // Private partner function to SuspendForDiversion.
  nsresult ResumeForDiversion();

  // Can be the original HttpChannelParent that created this object (normal
  // case), a different {HTTP|FTP}ChannelParent that we've been redirected to,
  // or some other listener that we have been diverted to via
  // nsIDivertableChannel.
  nsCOMPtr<nsIStreamListener> mNextListener;
  uint32_t mRedirectChannelId;
  // When set, no OnStart/OnData/OnStop calls should be received.
  bool mSuspendedForDiversion;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_HttpChannelParent_h
