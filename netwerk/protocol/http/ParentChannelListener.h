/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_ParentChannelListener_h
#define mozilla_net_ParentChannelListener_h

#include "nsIAuthPromptProvider.h"
#include "nsIInterfaceRequestor.h"
#include "nsINetworkInterceptController.h"
#include "nsIStreamListener.h"
#include "nsIMultiPartChannel.h"
#include "nsIRemoteWindowContext.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"

namespace mozilla {
namespace net {

#define PARENT_CHANNEL_LISTENER                      \
  {                                                  \
    0xa4e2c10c, 0xceba, 0x457f, {                    \
      0xa8, 0x0d, 0x78, 0x2b, 0x23, 0xba, 0xbd, 0x16 \
    }                                                \
  }

// Note: nsIInterfaceRequestor must be the first base so that do_QueryObject()
// works correctly on this object, as it's needed to compute a void* pointing to
// the beginning of this object.

class ParentChannelListener final : public nsIInterfaceRequestor,
                                    public nsIStreamListener,
                                    public nsIMultiPartChannelListener,
                                    public nsINetworkInterceptController,
                                    private nsIAuthPromptProvider,
                                    private nsIRemoteWindowContext {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIMULTIPARTCHANNELLISTENER
  NS_DECL_NSINETWORKINTERCEPTCONTROLLER
  NS_DECL_NSIAUTHPROMPTPROVIDER
  NS_DECL_NSIREMOTEWINDOWCONTEXT

  NS_DECLARE_STATIC_IID_ACCESSOR(PARENT_CHANNEL_LISTENER)

  explicit ParentChannelListener(
      nsIStreamListener* aListener,
      dom::CanonicalBrowsingContext* aBrowsingContext,
      bool aUsePrivateBrowsing);

  // For channel diversion from child to parent.
  void DivertTo(nsIStreamListener* aListener);
  [[nodiscard]] nsresult SuspendForDiversion();

  void SetupInterception(const nsHttpResponseHead& aResponseHead);
  void SetupInterceptionAfterRedirect(bool aShouldIntercept);
  void ClearInterceptedChannel(nsIStreamListener* aListener);

  // Called to set a new listener which replaces the old one after a redirect.
  void SetListenerAfterRedirect(nsIStreamListener* aListener);

  dom::CanonicalBrowsingContext* GetBrowsingContext() {
    return mBrowsingContext;
  }

 private:
  virtual ~ParentChannelListener();

  // Private partner function to SuspendForDiversion.
  void ResumeForDiversion();

  // Can be the original HttpChannelParent that created this object (normal
  // case), a different {HTTP|FTP}ChannelParent that we've been redirected to,
  // or some other listener that we have been diverted to via
  // nsIDivertableChannel.
  nsCOMPtr<nsIStreamListener> mNextListener;
  // When set, no OnStart/OnData/OnStop calls should be received.
  bool mSuspendedForDiversion;

  // Set if this channel should be intercepted before it sets up the HTTP
  // transaction.
  bool mShouldIntercept;
  // Set if this channel should suspend on interception.
  bool mShouldSuspendIntercept;
  // Set if the channel interception has been canceled.  Can be set before
  // interception first occurs.  In this case cancelation is deferred until
  // the interception takes place.
  bool mInterceptCanceled;

  UniquePtr<nsHttpResponseHead> mSynthesizedResponseHead;

  // Handle to the channel wrapper if this channel has been intercepted.
  nsCOMPtr<nsIInterceptedChannel> mInterceptedChannel;

  // This will be populated with a real network controller if parent-side
  // interception is enabled.
  nsCOMPtr<nsINetworkInterceptController> mInterceptController;

  RefPtr<dom::CanonicalBrowsingContext> mBrowsingContext;

  // True if we received OnStartRequest for a nsIMultiPartChannel, and are
  // expected AllPartsStopped to be called when complete.
  bool mIsMultiPart = false;

  // True if the nsILoadContext for this channel has private browsing enabled.
  bool mUsePrivateBrowsing = false;
};

NS_DEFINE_STATIC_IID_ACCESSOR(ParentChannelListener, PARENT_CHANNEL_LISTENER)

inline nsISupports* ToSupports(ParentChannelListener* aDoc) {
  return static_cast<nsIInterfaceRequestor*>(aDoc);
}

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_ParentChannelListener_h
