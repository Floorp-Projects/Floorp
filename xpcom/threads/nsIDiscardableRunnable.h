/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XPCOM_THREADS_NSIDISCARDABLERUNNABLE_H_
#define XPCOM_THREADS_NSIDISCARDABLERUNNABLE_H_

#include "nsISupports.h"

/**
 * An interface implemented by nsIRunnable tasks for which nsIRunnable::Run()
 * might not be called.
 */
#define NS_IDISCARDABLERUNNABLE_IID                  \
  {                                                  \
    0xde93dc4c, 0x755c, 0x4cdc, {                    \
      0x96, 0x76, 0x35, 0xc6, 0x48, 0x81, 0x59, 0x78 \
    }                                                \
  }

class NS_NO_VTABLE nsIDiscardableRunnable : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDISCARDABLERUNNABLE_IID)

  /**
   * Called exactly once on a queued task only if nsIRunnable::Run() is not
   * called.
   */
  virtual void OnDiscard() = 0;

 protected:
  nsIDiscardableRunnable() = default;
  virtual ~nsIDiscardableRunnable() = default;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDiscardableRunnable,
                              NS_IDISCARDABLERUNNABLE_IID)

#endif  // XPCOM_THREADS_NSIDISCARDABLERUNNABLE_H_
