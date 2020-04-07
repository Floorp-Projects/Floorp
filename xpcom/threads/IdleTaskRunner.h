/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IdleTaskRunner_h
#define IdleTaskRunner_h

#include "mozilla/TimeStamp.h"
#include "mozilla/TaskCategory.h"
#include "nsThreadUtils.h"
#include <functional>

namespace mozilla {

// A general purpose repeating callback runner (it can be configured
// to a one-time runner, too.) If it is running repeatedly,
// one has to either explicitly Cancel() the runner or have
// MayContinueProcessing() callback return false to completely remove
// the runner.
class IdleTaskRunner final : public IdleRunnable {
 public:
  // Return true if some meaningful work was done.
  using CallbackType = std::function<bool(TimeStamp aDeadline)>;

  // A callback for "stop processing" decision. Return true to
  // stop processing. This can be an alternative to Cancel() or
  // work together in different way.
  using MayStopProcessingCallbackType = std::function<bool()>;

 public:
  static already_AddRefed<IdleTaskRunner> Create(
      const CallbackType& aCallback, const char* aRunnableName, uint32_t aDelay,
      int64_t aBudget, bool aRepeating,
      const MayStopProcessingCallbackType& aMayStopProcessing);

  NS_IMETHOD Run() override;

  void SetDeadline(mozilla::TimeStamp aDeadline) override;
  void SetTimer(uint32_t aDelay, nsIEventTarget* aTarget) override;

  nsresult Cancel() override;
  void Schedule(bool aAllowIdleDispatch);

 private:
  explicit IdleTaskRunner(
      const CallbackType& aCallback, const char* aRunnableName, uint32_t aDelay,
      int64_t aBudget, bool aRepeating,
      const MayStopProcessingCallbackType& aMayStopProcessing);
  ~IdleTaskRunner();
  void CancelTimer();
  void SetTimerInternal(uint32_t aDelay);

  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<nsITimer> mScheduleTimer;
  CallbackType mCallback;
  uint32_t mDelay;
  TimeStamp mDeadline;
  TimeDuration mBudget;
  bool mRepeating;
  bool mTimerActive;
  MayStopProcessingCallbackType mMayStopProcessing;
  const char* mName;
};

}  // end of namespace mozilla.

#endif
