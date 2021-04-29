/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_InputTaskManager_h
#define mozilla_InputTaskManager_h

#include "nsXULAppAPI.h"
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
    return mInputQueueState == STATE_DISABLED ||
           mInputQueueState == STATE_SUSPEND || mSuspensionLevel > 0;
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

  void NotifyVsync() {
    MOZ_ASSERT(StaticPrefs::dom_input_events_strict_input_vsync_alignment());
    mInputPriorityController.WillRunVsync();
  }

 private:
  InputTaskManager() : mInputQueueState(STATE_DISABLED) {}

  class InputPriorityController {
   public:
    InputPriorityController();
    // Determines whether we should use the highest input priority for input
    // tasks
    bool ShouldUseHighestPriority(InputTaskManager*);

    void WillRunVsync();

    // Gets called when a input task is finished to run; If the current
    // input vsync state is `HasPendingVsync`, determines whether we
    // should continue running input tasks or leave the `HasPendingVsync` state
    // based on
    //    1. Whether we still have time to process input tasks
    //    2. Whether we have processed the max number of tasks that
    //    we should process.
    void DidRunTask();

   private:
    // Used to represents the relationship between Input and Vsync.
    //
    // HasPendingVsync: There are pending vsync tasks and we are using
    // InputHighest priority for inputs.
    // NoPendingVsync: No pending vsync tasks and no need to use InputHighest
    // priority.
    // RunVsync: Finished running input tasks and the vsync task
    // should be run.
    enum class InputVsyncState {
      HasPendingVsync,
      NoPendingVsync,
      RunVsync,
    };

    void EnterPendingVsyncState(uint32_t aNumPendingTasks);
    void LeavePendingVsyncState(bool aRunVsync);

    // Stores the number of pending input tasks when we enter the
    // InputVsyncState::HasPendingVsync state.
    uint32_t mMaxInputTasksToRun = 0;

    bool mIsInitialized;
    InputVsyncState mInputVsyncState;

    TimeStamp mRunInputStartTime;
    TimeDuration mMaxInputHandlingDuration;
  };

  int32_t GetPriorityModifierForEventLoopTurnForStrictVsyncAlignment();

  TimeStamp mInputHandlingStartTime;
  Atomic<InputEventQueueState> mInputQueueState;
  AutoTArray<TimeStamp, 4> mStartTimes;

  static StaticRefPtr<InputTaskManager> gInputTaskManager;

  // Number of BCGs have asked InputTaskManager to suspend input events
  uint32_t mSuspensionLevel = 0;

  InputPriorityController mInputPriorityController;
};

}  // namespace mozilla

#endif  // mozilla_InputTaskManager_h
