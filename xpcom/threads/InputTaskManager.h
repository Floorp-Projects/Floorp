/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_InputTaskManager_h
#define mozilla_InputTaskManager_h

#include "TaskController.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/StaticPrefs_dom.h"
#include "nsXULAppAPI.h"

namespace mozilla {

class InputTaskManager : public TaskManager {
 public:
  int32_t GetPriorityModifierForEventLoopTurn(
      const MutexAutoLock& aProofOfLock) final;
  void WillRunTask() final;
  void DidRunTask() final;

  enum InputEventQueueState {
    STATE_DISABLED,
    STATE_FLUSHING,
    STATE_SUSPEND,
    STATE_ENABLED
  };

  void EnableInputEventPrioritization();
  void FlushInputEventPrioritization();
  void SuspendInputEventPrioritization();
  void ResumeInputEventPrioritization();

  InputEventQueueState State() { return mInputQueueState; }

  void SetState(InputEventQueueState aState) { mInputQueueState = aState; }

  TimeStamp InputHandlingStartTime() { return mInputHandlingStartTime; }

  void SetInputHandlingStartTime(TimeStamp aStartTime) {
    mInputHandlingStartTime = aStartTime;
  }

  static InputTaskManager* Get() { return gInputTaskManager.get(); }
  static void Cleanup() { gInputTaskManager = nullptr; }
  static void Init();

  bool IsSuspended(const MutexAutoLock& aProofOfLock) override {
    MOZ_ASSERT(NS_IsMainThread());
    return mSuspensionLevel > 0;
  }

  bool IsSuspended() {
    MOZ_ASSERT(NS_IsMainThread());
    return mSuspensionLevel > 0;
  }

  void IncSuspensionLevel() {
    MOZ_ASSERT(NS_IsMainThread());
    ++mSuspensionLevel;
  }

  void DecSuspensionLevel() {
    MOZ_ASSERT(NS_IsMainThread());
    --mSuspensionLevel;
  }

  static bool CanSuspendInputEvent() {
    // Ensure it's content process because InputTaskManager only
    // works in e10s.
    //
    // Input tasks will have nullptr as their task manager when the
    // event queue state is STATE_DISABLED, so we can't suspend
    // input events.
    return XRE_IsContentProcess() &&
           StaticPrefs::dom_input_events_canSuspendInBCG_enabled() &&
           InputTaskManager::Get()->State() !=
               InputEventQueueState::STATE_DISABLED;
  }

 private:
  InputTaskManager() : mInputQueueState(STATE_DISABLED) {}

  TimeStamp mInputHandlingStartTime;
  Atomic<InputEventQueueState> mInputQueueState;
  AutoTArray<TimeStamp, 4> mStartTimes;

  static StaticRefPtr<InputTaskManager> gInputTaskManager;

  // Number of BCGs have asked InputTaskManager to suspend input events
  uint32_t mSuspensionLevel = 0;
};

}  // namespace mozilla

#endif  // mozilla_InputTaskManager_h
