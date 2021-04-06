/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputTaskManager.h"
#include "VsyncTaskManager.h"

namespace mozilla {

StaticRefPtr<InputTaskManager> InputTaskManager::gInputTaskManager;

void InputTaskManager::EnableInputEventPrioritization() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mInputQueueState == STATE_DISABLED);
  mInputQueueState = STATE_ENABLED;
  mInputHandlingStartTime = TimeStamp();
}

void InputTaskManager::FlushInputEventPrioritization() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mInputQueueState == STATE_ENABLED ||
             mInputQueueState == STATE_SUSPEND);
  mInputQueueState =
      mInputQueueState == STATE_ENABLED ? STATE_FLUSHING : STATE_SUSPEND;
}

void InputTaskManager::SuspendInputEventPrioritization() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mInputQueueState == STATE_ENABLED ||
             mInputQueueState == STATE_FLUSHING);
  mInputQueueState = STATE_SUSPEND;
}

void InputTaskManager::ResumeInputEventPrioritization() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mInputQueueState == STATE_SUSPEND);
  mInputQueueState = STATE_ENABLED;
}

int32_t InputTaskManager::GetPriorityModifierForEventLoopTurn(
    const MutexAutoLock& aProofOfLock) {
  if (StaticPrefs::dom_input_events_strict_input_vsync_alignment()) {
    return GetPriorityModifierForEventLoopTurnForStrictVsyncAlignment();
  }

  size_t inputCount = PendingTaskCount();
  if (State() == STATE_ENABLED && InputHandlingStartTime().IsNull() &&
      inputCount > 0) {
    SetInputHandlingStartTime(
        InputEventStatistics::Get().GetInputHandlingStartTime(inputCount));
  }

  if (inputCount > 0 && (State() == InputTaskManager::STATE_FLUSHING ||
                         (State() == InputTaskManager::STATE_ENABLED &&
                          !InputHandlingStartTime().IsNull() &&
                          TimeStamp::Now() > InputHandlingStartTime()))) {
    return 0;
  }

  int32_t modifier = static_cast<int32_t>(EventQueuePriority::InputLow) -
                     static_cast<int32_t>(EventQueuePriority::MediumHigh);
  return modifier;
}

void InputTaskManager::WillRunTask() {
  TaskManager::WillRunTask();
  mStartTimes.AppendElement(TimeStamp::Now());
}

void InputTaskManager::DidRunTask() {
  TaskManager::DidRunTask();
  MOZ_ASSERT(!mStartTimes.IsEmpty());
  TimeStamp start = mStartTimes.PopLastElement();
  InputEventStatistics::Get().UpdateInputDuration(TimeStamp::Now() - start);

  if (StaticPrefs::dom_input_events_strict_input_vsync_alignment()) {
    mInputPriorityController.DidRunTask();
  }
}

int32_t
InputTaskManager::GetPriorityModifierForEventLoopTurnForStrictVsyncAlignment() {
  MOZ_ASSERT(StaticPrefs::dom_input_events_strict_input_vsync_alignment());
  MOZ_ASSERT(!IsSuspended());

  size_t inputCount = PendingTaskCount();
  if (inputCount > 0 &&
      mInputPriorityController.ShouldUseHighestPriority(this)) {
    return static_cast<int32_t>(EventQueuePriority::InputHighest) -
           static_cast<int32_t>(EventQueuePriority::InputHigh);
  }

  if (State() == STATE_FLUSHING ||
      nsRefreshDriver::GetNextTickHint().isNothing()) {
    return 0;
  }

  return static_cast<int32_t>(EventQueuePriority::InputLow) -
         static_cast<int32_t>(EventQueuePriority::InputHigh);
}

InputTaskManager::InputPriorityController::InputPriorityController()
    : mIsInitialized(false),
      mInputVsyncState(InputVsyncState::NoPendingVsync) {}

bool InputTaskManager::InputPriorityController::ShouldUseHighestPriority(
    InputTaskManager* aInputTaskManager) {
  if (!mIsInitialized) {
    // Have to initialize mMaxInputHandlingDuration lazily because
    // Preference may not be ready upon the construction of
    // InputTaskManager.
    mMaxInputHandlingDuration =
        InputEventStatistics::Get().GetMaxInputHandlingDuration();
    mIsInitialized = true;
  }

  if (mInputVsyncState == InputVsyncState::HasPendingVsync) {
    return true;
  }

  if (mInputVsyncState == InputVsyncState::RunVsync) {
    return false;
  }

  if (mInputVsyncState == InputVsyncState::NoPendingVsync &&
      VsyncTaskManager::Get()->PendingTaskCount()) {
    EnterPendingVsyncState(aInputTaskManager->PendingTaskCount());
    return true;
  }

  return false;
}

void InputTaskManager::InputPriorityController::EnterPendingVsyncState(
    uint32_t aNumPendingTasks) {
  MOZ_ASSERT(StaticPrefs::dom_input_events_strict_input_vsync_alignment());
  MOZ_ASSERT(mInputVsyncState == InputVsyncState::NoPendingVsync);

  mInputVsyncState = InputVsyncState::HasPendingVsync;
  mMaxInputTasksToRun = aNumPendingTasks;
  mRunInputStartTime = TimeStamp::Now();
}

void InputTaskManager::InputPriorityController::DidVsync() {
  MOZ_ASSERT(StaticPrefs::dom_input_events_strict_input_vsync_alignment());

  if (mInputVsyncState == InputVsyncState::RunVsync) {
    LeavePendingVsyncState(false);
  }
}

void InputTaskManager::InputPriorityController::LeavePendingVsyncState(
    bool aRunVsync) {
  MOZ_ASSERT(StaticPrefs::dom_input_events_strict_input_vsync_alignment());

  if (aRunVsync) {
    MOZ_ASSERT(mInputVsyncState == InputVsyncState::HasPendingVsync);
    mInputVsyncState = InputVsyncState::RunVsync;
  } else {
    MOZ_ASSERT(mInputVsyncState == InputVsyncState::RunVsync);
    mInputVsyncState = InputVsyncState::NoPendingVsync;
  }

  mMaxInputTasksToRun = 0;
}

void InputTaskManager::InputPriorityController::DidRunTask() {
  MOZ_ASSERT(StaticPrefs::dom_input_events_strict_input_vsync_alignment());

  switch (mInputVsyncState) {
    case InputVsyncState::NoPendingVsync:
      MOZ_ASSERT(mMaxInputTasksToRun == 0);
      return;
    case InputVsyncState::HasPendingVsync:
      MOZ_ASSERT(mMaxInputTasksToRun > 0);
      --mMaxInputTasksToRun;
      if (!mMaxInputTasksToRun ||
          TimeStamp::Now() - mRunInputStartTime >= mMaxInputHandlingDuration) {
        LeavePendingVsyncState(true);
      }
      return;
    default:
      MOZ_CRASH("Shouldn't run this input task when we suppose to run vsync");
      return;
  }
}

// static
void InputTaskManager::Init() { gInputTaskManager = new InputTaskManager(); }

}  // namespace mozilla
