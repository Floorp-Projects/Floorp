/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWebRequestListener_h__
#define nsWebRequestListener_h__

#include "nsCOMPtr.h"
#include "nsIWebRequestListener.h"
#include "nsIRequestObserver.h"
#include "nsIStreamListener.h"
#include "nsITraceableChannel.h"
#include "nsIThreadRetargetableStreamListener.h"
#include "mozilla/Attributes.h"

class nsWebRequestListener final : public nsIWebRequestListener
                                 , public nsIStreamListener
                                 , public nsIThreadRetargetableStreamListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBREQUESTLISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER

  nsWebRequestListener() {}

private:
  ~nsWebRequestListener() {}
  nsCOMPtr<nsIStreamListener> mOrigStreamListener;
  nsCOMPtr<nsIStreamListener> mTargetStreamListener;
};

#endif // nsWebRequestListener_h__

