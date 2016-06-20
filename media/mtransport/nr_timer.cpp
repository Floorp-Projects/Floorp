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

// Some code here copied from nrappkit. The license was.

/**
   Copyright (C) 2004, Network Resonance, Inc.
   Copyright (C) 2006, Network Resonance, Inc.
   All Rights Reserved

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of Network Resonance, Inc. nor the name of any
      contributors to this software may be used to endorse or promote
      products derived from this software without specific prior written
      permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.


   ekr@rtfm.com  Sun Feb 22 19:35:24 2004
 */

#include <string>

#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsIEventTarget.h"
#include "nsITimer.h"
#include "nsNetCID.h"
#include "runnable_utils.h"

extern "C" {
#include "nr_api.h"
#include "async_timer.h"
}


namespace mozilla {

class nrappkitCallback  {
 public:
  nrappkitCallback(NR_async_cb cb, void *cb_arg,
                   const char *function, int line)
    : cb_(cb), cb_arg_(cb_arg), function_(function), line_(line) {
  }
  virtual ~nrappkitCallback() {}

  virtual void Cancel() = 0;

protected:
  /* additional members */
  NR_async_cb cb_;
  void *cb_arg_;
  std::string function_;
  int line_;
};

class nrappkitTimerCallback : public nrappkitCallback,
                              public nsITimerCallback {
 public:
  // We're going to release ourself in the callback, so we need to be threadsafe
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

  nrappkitTimerCallback(NR_async_cb cb, void *cb_arg,
                        const char *function, int line)
      : nrappkitCallback(cb, cb_arg, function, line),
      timer_(nullptr) {}

  void SetTimer(already_AddRefed<nsITimer>&& timer) {
    timer_ = timer;
  }

  virtual void Cancel() override {
    AddRef();  // Cancelling the timer causes the callback it holds to
               // be released. AddRef() keeps us alive.
    timer_->Cancel();
    timer_ = nullptr;
    Release(); // Will cause deletion of this object.
  }

 private:
  nsCOMPtr<nsITimer> timer_;
  virtual ~nrappkitTimerCallback() {}
};

NS_IMPL_ISUPPORTS(nrappkitTimerCallback, nsITimerCallback)

NS_IMETHODIMP nrappkitTimerCallback::Notify(nsITimer *timer) {
  r_log(LOG_GENERIC, LOG_DEBUG, "Timer callback fired (set in %s:%d)",
        function_.c_str(), line_);
  MOZ_RELEASE_ASSERT(timer == timer_);
  cb_(0, 0, cb_arg_);

  // Allow the timer to go away.
  timer_ = nullptr;
  return NS_OK;
}

class nrappkitScheduledCallback : public nrappkitCallback {
 public:

  nrappkitScheduledCallback(NR_async_cb cb, void *cb_arg,
                            const char *function, int line)
      : nrappkitCallback(cb, cb_arg, function, line) {}

  void Run() {
    if (cb_) {
      cb_(0, 0, cb_arg_);
    }
  }

  virtual void Cancel() override {
    cb_ = nullptr;
  }

  ~nrappkitScheduledCallback() {}
};

}  // close namespace


using namespace mozilla;

static nsCOMPtr<nsIEventTarget> GetSTSThread() {
  nsresult rv;

  nsCOMPtr<nsIEventTarget> sts_thread;

  sts_thread = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  return sts_thread;
}

// These timers must only be used from the STS thread.
// This function is a helper that enforces that.
static void CheckSTSThread() {
  nsCOMPtr<nsIEventTarget> sts_thread = GetSTSThread();

  ASSERT_ON_THREAD(sts_thread);
}

static int nr_async_timer_set_zero(NR_async_cb cb, void *arg,
                                   char *func, int l,
                                   nrappkitCallback **handle) {
  nrappkitScheduledCallback* callback(new nrappkitScheduledCallback(
      cb, arg, func, l));

  nsresult rv = GetSTSThread()->Dispatch(WrapRunnable(
      nsAutoPtr<nrappkitScheduledCallback>(callback),
      &nrappkitScheduledCallback::Run),
                        NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv))
    return R_FAILED;

  *handle = callback;

  // On exit to this function, the only strong reference to callback is in
  // the Runnable. Because we are redispatching to the same thread,
  // this is always safe.
  return 0;
}

static int nr_async_timer_set_nonzero(int timeout, NR_async_cb cb, void *arg,
                                      char *func, int l,
                                      nrappkitCallback **handle) {
  nsresult rv;
  CheckSTSThread();

  nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return(R_FAILED);
  }

  nrappkitTimerCallback* callback =
      new nrappkitTimerCallback(cb, arg, func, l);
  rv = timer->InitWithCallback(callback, timeout, nsITimer::TYPE_ONE_SHOT);
  if (NS_FAILED(rv)) {
    return R_FAILED;
  }

  // Move the ownership of the timer to the callback object, which holds the
  // timer alive per spec.
  callback->SetTimer(timer.forget());

  *handle = callback;

  return 0;
}

int NR_async_timer_set(int timeout, NR_async_cb cb, void *arg,
                       char *func, int l, void **handle) {
  CheckSTSThread();

  nrappkitCallback *callback;
  int r;

  if (!timeout) {
    r = nr_async_timer_set_zero(cb, arg, func, l, &callback);
  } else {
    r = nr_async_timer_set_nonzero(timeout, cb, arg, func, l, &callback);
  }

  if (r)
    return r;

  if (handle)
    *handle = callback;

  return 0;
}

int NR_async_schedule(NR_async_cb cb, void *arg, char *func, int l) {
  // No need to check the thread because we check it next in the
  // timer set.
  return NR_async_timer_set(0, cb, arg, func, l, nullptr);
}

int NR_async_timer_cancel(void *handle) {
  // Check for the handle being nonzero because sometimes we get
  // no-op cancels that aren't on the STS thread. This can be
  // non-racy as long as the upper-level code is careful.
  if (!handle)
    return 0;

  CheckSTSThread();

  nrappkitCallback* callback = static_cast<nrappkitCallback *>(handle);
  callback->Cancel();

  return 0;
}

