/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XPCOM_THREADS_DELAYEDRUNNABLE_H_
#define XPCOM_THREADS_DELAYEDRUNNABLE_H_

#include "mozilla/TimeStamp.h"
#include "nsCOMPtr.h"
#include "nsIDelayedRunnableObserver.h"
#include "nsIRunnable.h"
#include "nsITimer.h"
#include "nsThreadUtils.h"

namespace mozilla {

class DelayedRunnable : public Runnable, public nsITimerCallback {
 public:
  DelayedRunnable(already_AddRefed<nsIEventTarget> aTarget,
                  already_AddRefed<nsIRunnable> aRunnable, uint32_t aDelay);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSITIMERCALLBACK

  nsresult Init();

  /**
   * Cancels the underlying timer. Called when the target is going away, so the
   * runnable can be released safely on the target thread.
   */
  void CancelTimer();

 private:
  ~DelayedRunnable() = default;
  nsresult DoRun();

  const nsCOMPtr<nsIEventTarget> mTarget;
  const nsCOMPtr<nsIDelayedRunnableObserver> mObserver;
  nsCOMPtr<nsIRunnable> mWrappedRunnable;
  nsCOMPtr<nsITimer> mTimer;
  const TimeStamp mDelayedFrom;
  uint32_t mDelay;
};

}  // namespace mozilla

#endif
