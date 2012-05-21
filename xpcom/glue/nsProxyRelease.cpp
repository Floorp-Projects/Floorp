/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsProxyRelease.h"
#include "nsThreadUtils.h"
#include "nsAutoPtr.h"

class nsProxyReleaseEvent : public nsRunnable
{
public:
    nsProxyReleaseEvent(nsISupports *doomed)
        : mDoomed(doomed) {
    }

    NS_IMETHOD Run()
    {
        mDoomed->Release();
        return NS_OK;
    }

private:
    nsISupports *mDoomed;
};

nsresult
NS_ProxyRelease(nsIEventTarget *target, nsISupports *doomed,
                bool alwaysProxy)
{
    nsresult rv;

    if (!target) {
        NS_RELEASE(doomed);
        return NS_OK;
    }

    if (!alwaysProxy) {
        bool onCurrentThread = false;
        rv = target->IsOnCurrentThread(&onCurrentThread);
        if (NS_SUCCEEDED(rv) && onCurrentThread) {
            NS_RELEASE(doomed);
            return NS_OK;
        }
    }

    nsRefPtr<nsIRunnable> ev = new nsProxyReleaseEvent(doomed);
    if (!ev) {
        // we do not release doomed here since it may cause a delete on the
        // wrong thread.  better to leak than crash. 
        return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = target->Dispatch(ev, NS_DISPATCH_NORMAL);
    if (NS_FAILED(rv)) {
        NS_WARNING("failed to post proxy release event");
        // again, it is better to leak the doomed object than risk crashing as
        // a result of deleting it on the wrong thread.
    }
    return rv;
}
