/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_InputTaskManager_h
#define mozilla_InputTaskManager_h

#include "TaskController.h"

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

 private:
  InputTaskManager() : mInputQueueState(STATE_DISABLED) {}

  TimeStamp mInputHandlingStartTime;
  Atomic<InputEventQueueState> mInputQueueState;
  AutoTArray<TimeStamp, 4> mStartTimes;

  static StaticRefPtr<InputTaskManager> gInputTaskManager;
};

}  // namespace mozilla

#endif  // mozilla_InputTaskManager_h
