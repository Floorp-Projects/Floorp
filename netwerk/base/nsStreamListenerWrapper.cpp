/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsStreamListenerWrapper.h"
#ifdef DEBUG
#include "MainThreadUtils.h"
#endif

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(nsStreamListenerWrapper,
                  nsIStreamListener,
                  nsIRequestObserver,
                  nsIThreadRetargetableStreamListener)

NS_IMETHODIMP
nsStreamListenerWrapper::CheckListenerChain()
{
    NS_ASSERTION(NS_IsMainThread(), "Should be on main thread!");
    nsresult rv = NS_OK;
    nsCOMPtr<nsIThreadRetargetableStreamListener> retargetableListener =
        do_QueryInterface(mListener, &rv);
    if (retargetableListener) {
        rv = retargetableListener->CheckListenerChain();
    }
    return rv;
}

} // namespace net
} // namespace mozilla
