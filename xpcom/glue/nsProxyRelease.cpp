/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsProxyRelease.h"
#include "nsThreadUtils.h"
#include "nsAutoPtr.h"

class nsProxyReleaseEvent : public nsRunnable
{
public:
  nsProxyReleaseEvent(nsISupports* aDoomed) : mDoomed(aDoomed) {}

  NS_IMETHOD Run()
  {
    mDoomed->Release();
    return NS_OK;
  }

private:
  nsISupports* mDoomed;
};

nsresult
NS_ProxyRelease(nsIEventTarget* aTarget, nsISupports* aDoomed,
                bool aAlwaysProxy)
{
  nsresult rv;

  if (!aDoomed) {
    // nothing to do
    return NS_OK;
  }

  if (!aTarget) {
    NS_RELEASE(aDoomed);
    return NS_OK;
  }

  if (!aAlwaysProxy) {
    bool onCurrentThread = false;
    rv = aTarget->IsOnCurrentThread(&onCurrentThread);
    if (NS_SUCCEEDED(rv) && onCurrentThread) {
      NS_RELEASE(aDoomed);
      return NS_OK;
    }
  }

  nsRefPtr<nsIRunnable> ev = new nsProxyReleaseEvent(aDoomed);
  if (!ev) {
    // we do not release aDoomed here since it may cause a delete on the
    // wrong thread.  better to leak than crash.
    return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = aTarget->Dispatch(ev, NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    NS_WARNING("failed to post proxy release event");
    // again, it is better to leak the aDoomed object than risk crashing as
    // a result of deleting it on the wrong thread.
  }
  return rv;
}
