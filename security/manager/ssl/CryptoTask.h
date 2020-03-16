/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla__CryptoTask_h
#define mozilla__CryptoTask_h

#include "mozilla/Attributes.h"
#include "nsThreadUtils.h"

namespace mozilla {

/**
 * Frequently we need to run a task on a background thread without blocking
 * the main thread, and then call a callback on the main thread with the
 * result. This class provides the framework for that. Subclasses must:
 *
 *   (1) Override CalculateResult for the off-the-main-thread computation.
 *   (2) Override CallCallback() for the on-the-main-thread call of the
 *       callback.
 */
class CryptoTask : public Runnable {
 public:
  nsresult Dispatch();

 protected:
  CryptoTask() : Runnable("CryptoTask"), mRv(NS_ERROR_NOT_INITIALIZED) {}

  virtual ~CryptoTask() = default;

  /**
   * Called on a background thread (never the main thread). Its result will be
   * passed to CallCallback on the main thread.
   */
  virtual nsresult CalculateResult() = 0;

  /**
   * Called on the main thread with the result from CalculateResult().
   */
  virtual void CallCallback(nsresult rv) = 0;

 private:
  NS_IMETHOD Run() final;

  nsresult mRv;
};

}  // namespace mozilla

#endif  // mozilla__CryptoTask_h
