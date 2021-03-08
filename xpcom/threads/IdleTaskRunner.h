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
  // An IdleTaskRunner will attempt to run in any idle time period large enough
  // to fit `aMinimumUsefulBudget`. If no such window occurs within `aMaxDelay`
  // ms, it will stop waiting for idle time and run from a TYPE_ONE_SHOT timer.
  static already_AddRefed<IdleTaskRunner> Create(
      const CallbackType& aCallback, const char* aRunnableName,
      uint32_t aMaxDelay, int64_t aMinimumUsefulBudget, bool aRepeating,
      const MayStopProcessingCallbackType& aMayStopProcessing);

  NS_IMETHOD Run() override;

  // (Used by the task triggering code.) Record the end of the current idle
  // period, or null if not running during idle time.
  void SetDeadline(mozilla::TimeStamp aDeadline) override;

  void SetTimer(uint32_t aDelay, nsIEventTarget* aTarget) override;

  // Update the minimum idle time that this callback would be invoked for.
  void SetMinimumUsefulBudget(int64_t aMinimumUsefulBudget);

  nsresult Cancel() override;
  void Schedule(bool aAllowIdleDispatch);

 private:
  explicit IdleTaskRunner(
      const CallbackType& aCallback, const char* aRunnableName,
      uint32_t aMaxDelay, int64_t aMinimumUsefulBudget, bool aRepeating,
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

  // The least duration worth calling the callback for during idle time.
  TimeDuration mMinimumUsefulBudget;

  bool mRepeating;
  bool mTimerActive;
  MayStopProcessingCallbackType mMayStopProcessing;
  const char* mName;
};

}  // end of namespace mozilla.

#endif
