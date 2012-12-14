/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original code by: ekr@rtfm.com

// Implementation of the NR timer interface

#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsIEventTarget.h"
#include "nsITimer.h"
#include "nsNetCID.h"

extern "C" {
#include "nr_api.h"
#include "async_timer.h"
}


namespace mozilla {

class nrappkitTimerCallback : public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

  nrappkitTimerCallback(NR_async_cb cb, void *cb_arg) : cb_(cb), cb_arg_(cb_arg) {}

private:
  virtual ~nrappkitTimerCallback() {}

protected:
  /* additional members */
  NR_async_cb cb_;
  void *cb_arg_;
};

// We're going to release ourself in the callback, so we need to be threadsafe
NS_IMPL_THREADSAFE_ISUPPORTS1(nrappkitTimerCallback, nsITimerCallback)

NS_IMETHODIMP nrappkitTimerCallback::Notify(nsITimer *timer) {
  r_log(LOG_GENERIC, LOG_DEBUG, "Timer callback fired");
  cb_(0, 0, cb_arg_);

  // Allow the timer to go away.
  timer->Release();
  return NS_OK;
}
}  // close namespace


using namespace mozilla;

int NR_async_timer_set(int timeout, NR_async_cb cb, void *arg, char *func,
                       int l, void **handle) {
  nsresult rv;

  nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);

  if (NS_FAILED(rv)) {
    return(R_FAILED);
  }

  rv = timer->InitWithCallback(new nrappkitTimerCallback(cb, arg),
                                        timeout, nsITimer::TYPE_ONE_SHOT);
  if (NS_FAILED(rv)) {
    return R_FAILED;
  }

  // We need an AddRef here to keep the timer alive, per the spec.
  timer->AddRef();

  if (handle)
    *handle = timer.get();
  // Bug 818806: if we have no handle to the timer, we have no way to avoid
  // it leaking (though not the callback object) if it never fires (or if
  // we exit before it fires).

  return 0;
}

int NR_async_schedule(NR_async_cb cb, void *arg, char *func, int l) {
  return NR_async_timer_set(0, cb, arg, func, l, nullptr);
}

int NR_async_timer_cancel(void *handle) {
  nsITimer *timer = static_cast<nsITimer *>(handle);

  timer->Cancel();
  // Allow the timer to go away.
  timer->Release();

  return 0;
}

