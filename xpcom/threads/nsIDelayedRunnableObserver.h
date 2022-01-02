/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XPCOM_THREADS_NSIDELAYEDRUNNABLEOBSERVER_H_
#define XPCOM_THREADS_NSIDELAYEDRUNNABLEOBSERVER_H_

#include "nsISupports.h"

namespace mozilla {
class DelayedRunnable;
}

#define NS_IDELAYEDRUNNABLEOBSERVER_IID              \
  {                                                  \
    0xd226bade, 0xac13, 0x46fe, {                    \
      0x9f, 0xcc, 0xde, 0xe7, 0x48, 0x35, 0xcd, 0x82 \
    }                                                \
  }

class NS_NO_VTABLE nsIDelayedRunnableObserver : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDELAYEDRUNNABLEOBSERVER_IID)

  /**
   * Called by the DelayedRunnable after being created, on the dispatching
   * thread. This allows for various lifetime checks and gives assertions a
   * chance to provide useful stack traces.
   */
  virtual void OnDelayedRunnableCreated(
      mozilla::DelayedRunnable* aRunnable) = 0;
  /**
   * Called by the DelayedRunnable on its target thread when delegating the
   * responsibility for being run to its underlying timer.
   */
  virtual void OnDelayedRunnableScheduled(
      mozilla::DelayedRunnable* aRunnable) = 0;
  /**
   * Called by the DelayedRunnable on its target thread after having been run by
   * its underlying timer.
   */
  virtual void OnDelayedRunnableRan(mozilla::DelayedRunnable* aRunnable) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDelayedRunnableObserver,
                              NS_IDELAYEDRUNNABLEOBSERVER_IID)

#endif
