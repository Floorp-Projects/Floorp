/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStreamListenerWrapper_h__
#define nsStreamListenerWrapper_h__

#include "nsCOMPtr.h"
#include "nsIRequest.h"
#include "nsIStreamListener.h"
#include "nsIThreadRetargetableStreamListener.h"
#include "nsIMultiPartChannel.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace net {

// Wrapper class to make replacement of nsHttpChannel's listener
// from JavaScript possible. It is workaround for bug 433711 and 682305.
class nsStreamListenerWrapper final
    : public nsIMultiPartChannelListener,
      public nsIThreadRetargetableStreamListener {
 public:
  explicit nsStreamListenerWrapper(nsIStreamListener* listener)
      : mListener(listener) {
    MOZ_ASSERT(mListener, "no stream listener specified");
  }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_FORWARD_SAFE_NSISTREAMLISTENER(mListener)
  NS_DECL_NSIMULTIPARTCHANNELLISTENER
  NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER

  //  Don't use NS_FORWARD_NSIREQUESTOBSERVER(mListener->) here, because we need
  //  to release mListener in OnStopRequest, and IDL-generated function doesn't.
  NS_IMETHOD OnStartRequest(nsIRequest* aRequest) override {
    // OnStartRequest can come after OnStopRequest in certain cases (multipart
    // listeners)
    nsCOMPtr<nsIMultiPartChannel> multiPartChannel =
        do_QueryInterface(aRequest);
    if (multiPartChannel) {
      mIsMulti = true;
    }
    return mListener->OnStartRequest(aRequest);
  }
  NS_IMETHOD OnStopRequest(nsIRequest* aRequest,
                           nsresult aStatusCode) override {
    nsresult rv = mListener->OnStopRequest(aRequest, aStatusCode);
    if (!mIsMulti) {
      // Multipart channels can call OnStartRequest again
      mListener = nullptr;
    }
    return rv;
  }

 private:
  bool mIsMulti{false};
  ~nsStreamListenerWrapper() = default;
  nsCOMPtr<nsIStreamListener> mListener;
};

}  // namespace net
}  // namespace mozilla

#endif  // nsStreamListenerWrapper_h__
