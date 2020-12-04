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

// A general purpose repeating callback runner (it can be configured to a
// one-time runner, too.) If it is running repeatedly, one has to either
// explicitly Cancel() the runner or have MayStopProcessing() callback return
// true to completely remove the runner.
class IdleTaskRunner final : public CancelableIdleRunnable {
 public:
  // Return true if some meaningful work was done.
  using CallbackType = std::function<bool(TimeStamp aDeadline)>;

  // A callback for "stop processing" decision. Return true to
  // stop processing. This can be an alternative to Cancel() or
  // work together in different way.
  using MayStopProcessingCallbackType = std::function<bool()>;

 public:
  // An IdleTaskRunner will attempt to run in idle time, with a budget computed
  // based on a (capped) estimate for how much idle time is available. If there
  // is no idle time within `aMaxDelay` ms, it will fall back to running using
  // a specified `aNonIdleBudget`.
  static already_AddRefed<IdleTaskRunner> Create(
      const CallbackType& aCallback, const char* aRunnableName,
      uint32_t aMaxDelay, int64_t aNonIdleBudget, bool aRepeating,
      const MayStopProcessingCallbackType& aMayStopProcessing);

  NS_IMETHOD Run() override;

  // (Used by the task triggering code.) Record the end of the current idle
  // period, or null if not running during idle time.
  void SetDeadline(mozilla::TimeStamp aDeadline) override;

  void SetTimer(uint32_t aDelay, nsIEventTarget* aTarget) override;

  // Update the non-idle time budgeted for this callback. This really only
  // makes sense for a repeating runner.
  void SetBudget(int64_t aBudget);

  nsresult Cancel() override;
  void Schedule(bool aAllowIdleDispatch);

 private:
  explicit IdleTaskRunner(
      const CallbackType& aCallback, const char* aRunnableName,
      uint32_t aMaxDelay, int64_t aNonIdleBudget, bool aRepeating,
      const MayStopProcessingCallbackType& aMayStopProcessing);
  ~IdleTaskRunner();
  void CancelTimer();
  void SetTimerInternal(uint32_t aDelay);

  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<nsITimer> mScheduleTimer;
  CallbackType mCallback;

  // Wait this long for idle time before giving up and running a non-idle
  // callback.
  uint32_t mDelay;

  // If running during idle time, the expected end of the current idle period.
  // The null timestamp when the run is triggered by aMaxDelay instead of idle.
  TimeStamp mDeadline;

  // The expected amount of time the callback will run for, when not running
  // during idle time.
  TimeDuration mBudget;

  bool mRepeating;
  bool mTimerActive;
  MayStopProcessingCallbackType mMayStopProcessing;
  const char* mName;
};

}  // end of namespace mozilla.

#endif
