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

namespace mozilla {

// Return true if some meaningful work was done.
typedef bool (*IdleTaskRunnerCallback) (TimeStamp aDeadline, void* aData);

class IdleTaskRunner final : public IdleRunnable
{
public:
  static already_AddRefed<IdleTaskRunner>
  Create(IdleTaskRunnerCallback aCallback, uint32_t aDelay,
         int64_t aBudget, bool aRepeating, void* aData = nullptr);

  NS_IMETHOD Run() override;

  void SetDeadline(mozilla::TimeStamp aDeadline) override;
  void SetTimer(uint32_t aDelay, nsIEventTarget* aTarget) override;

  nsresult Cancel() override;
  void Schedule(bool aAllowIdleDispatch);

private:
  explicit IdleTaskRunner(IdleTaskRunnerCallback aCallback,
                          uint32_t aDelay, int64_t aBudget,
                          bool aRepeating, void* aData);
  ~IdleTaskRunner();
  void CancelTimer();
  void SetTimerInternal(uint32_t aDelay);

  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<nsITimer> mScheduleTimer;
  IdleTaskRunnerCallback mCallback;
  uint32_t mDelay;
  TimeStamp mDeadline;
  TimeDuration mBudget;
  bool mRepeating;
  bool mTimerActive;
  void* mData;
};

} // end of namespace mozilla.

#endif
