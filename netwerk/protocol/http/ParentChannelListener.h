/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_ParentChannelListener_h
#define mozilla_net_ParentChannelListener_h

#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "nsIAuthPromptProvider.h"
#include "nsIInterfaceRequestor.h"
#include "nsIMultiPartChannel.h"
#include "nsINetworkInterceptController.h"
#include "nsIStreamListener.h"
#include "nsIThreadRetargetableStreamListener.h"

namespace mozilla {
namespace net {

#define PARENT_CHANNEL_LISTENER                      \
  {                                                  \
    0xa4e2c10c, 0xceba, 0x457f, {                    \
      0xa8, 0x0d, 0x78, 0x2b, 0x23, 0xba, 0xbd, 0x16 \
    }                                                \
  }

class ParentChannelListener final : public nsIInterfaceRequestor,
                                    public nsIStreamListener,
                                    public nsIMultiPartChannelListener,
                                    public nsINetworkInterceptController,
                                    public nsIThreadRetargetableStreamListener,
                                    private nsIAuthPromptProvider {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIMULTIPARTCHANNELLISTENER
  NS_DECL_NSINETWORKINTERCEPTCONTROLLER
  NS_DECL_NSIAUTHPROMPTPROVIDER
  NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER

  NS_DECLARE_STATIC_IID_ACCESSOR(PARENT_CHANNEL_LISTENER)

  explicit ParentChannelListener(
      nsIStreamListener* aListener,
      dom::CanonicalBrowsingContext* aBrowsingContext,
      bool aUsePrivateBrowsing);

  // Called to set a new listener which replaces the old one after a redirect.
  void SetListenerAfterRedirect(nsIStreamListener* aListener);

  dom::CanonicalBrowsingContext* GetBrowsingContext() const {
    return mBrowsingContext;
  }

 private:
  virtual ~ParentChannelListener();

  // Can be the original HttpChannelParent that created this object (normal
  // case), a different {HTTP|FTP}ChannelParent that we've been redirected to,
  // or some other listener that we have been diverted to via
  // nsIDivertableChannel.
  nsCOMPtr<nsIStreamListener> mNextListener;

  // This will be populated with a real network controller if parent-side
  // interception is enabled.
  nsCOMPtr<nsINetworkInterceptController> mInterceptController;

  RefPtr<dom::CanonicalBrowsingContext> mBrowsingContext;

  // True if we received OnStartRequest for a nsIMultiPartChannel, and are
  // expected AllPartsStopped to be called when complete.
  bool mIsMultiPart = false;
};

NS_DEFINE_STATIC_IID_ACCESSOR(ParentChannelListener, PARENT_CHANNEL_LISTENER)

inline nsISupports* ToSupports(ParentChannelListener* aDoc) {
  return static_cast<nsIInterfaceRequestor*>(aDoc);
}

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_ParentChannelListener_h
