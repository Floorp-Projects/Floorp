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

  HttpChannelParentListener(HttpChannelParent* aInitialChannel);
  virtual ~HttpChannelParentListener();

private:
  nsCOMPtr<nsIParentChannel> mActiveChannel;
  uint32_t mRedirectChannelId;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_HttpChannelParent_h
