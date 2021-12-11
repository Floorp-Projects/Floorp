/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStreamListenerTee_h__
#define nsStreamListenerTee_h__

#include "nsIStreamListenerTee.h"
#include "nsIThreadRetargetableStreamListener.h"
#include "nsIInputStreamTee.h"
#include "nsIOutputStream.h"
#include "nsCOMPtr.h"
#include "nsIEventTarget.h"
#include "nsIMultiPartChannel.h"

namespace mozilla {
namespace net {

class nsStreamListenerTee : public nsIStreamListenerTee,
                            public nsIThreadRetargetableStreamListener,
                            public nsIMultiPartChannelListener {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER
  NS_DECL_NSISTREAMLISTENERTEE
  NS_DECL_NSIMULTIPARTCHANNELLISTENER

  nsStreamListenerTee() = default;

 private:
  virtual ~nsStreamListenerTee() = default;

  nsCOMPtr<nsIInputStreamTee> mInputTee;
  nsCOMPtr<nsIOutputStream> mSink;
  nsCOMPtr<nsIStreamListener> mListener;
  nsCOMPtr<nsIRequestObserver> mObserver;
  nsCOMPtr<nsIEventTarget> mEventTarget;
  bool mIsMultiPart = false;
};

}  // namespace net
}  // namespace mozilla

#endif
