/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStreamListenerWrapper_h__
#define nsStreamListenerWrapper_h__

#include "nsCOMPtr.h"
#include "nsIStreamListener.h"
#include "nsIRequestObserver.h"
#include "nsIThreadRetargetableStreamListener.h"
#include "mozilla/Attributes.h"

// Wrapper class to make replacement of nsHttpChannel's listener
// from JavaScript possible. It is workaround for bug 433711 and 682305.
class nsStreamListenerWrapper MOZ_FINAL : public nsIStreamListener
                                        , public nsIThreadRetargetableStreamListener
{
public:
  nsStreamListenerWrapper(nsIStreamListener *listener)
    : mListener(listener)
  {
    NS_ASSERTION(mListener, "no stream listener specified");
  }

  NS_DECL_ISUPPORTS
  NS_FORWARD_NSIREQUESTOBSERVER(mListener->)
  NS_FORWARD_NSISTREAMLISTENER(mListener->)
  NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER

private:
  ~nsStreamListenerWrapper() {}
  nsCOMPtr<nsIStreamListener> mListener;
};

#endif // nsStreamListenerWrapper_h__

