/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IdleTaskRunner_h
#define IdleTaskRunner_h

#include "nsThreadUtils.h"

namespace mozilla {

// A general purpose repeating callback runner (it can be configured
// to a one-time runner, too.) If it is running repeatedly,
// one has to either explicitly Cancel() the runner or have
// MayContinueProcessing() callback return false to completely remove
// the runner.
class IdleTaskRunner final : public IdleRunnable
{
public:
  // Return true if some meaningful work was done.
  using CallbackType = std::function<bool(TimeStamp aDeadline)>;

  // A callback for "continue process" decision. Return false to
  // stop processing. This can be an alternative to Cancel() or
  // work together in different way.
  using MayContinueProcessingCallbackType = std::function<bool()>;

public:
  static already_AddRefed<IdleTaskRunner>
  Create(const CallbackType& aCallback, uint32_t aDelay,
         int64_t aBudget, bool aRepeating,
         const MayContinueProcessingCallbackType& aMaybeContinueProcessing);

  NS_IMETHOD Run() override;

  void SetDeadline(mozilla::TimeStamp aDeadline) override;
  void SetTimer(uint32_t aDelay, nsIEventTarget* aTarget) override;

  nsresult Cancel() override;
  void Schedule(bool aAllowIdleDispatch);

private:
  explicit IdleTaskRunner(const CallbackType& aCallback,
                          uint32_t aDelay, int64_t aBudget,
                          bool aRepeating,
                          const MayContinueProcessingCallbackType& aMaybeContinueProcessing);
  ~IdleTaskRunner();
  void CancelTimer();

  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<nsITimer> mScheduleTimer;
  nsCOMPtr<nsIEventTarget> mTarget;
  CallbackType mCallback;
  uint32_t mDelay;
  TimeStamp mDeadline;
  TimeDuration mBudget;
  bool mRepeating;
  bool mTimerActive;
  MayContinueProcessingCallbackType mMaybeContinueProcessing;
};

} // end of unnamed namespace.

#endif
