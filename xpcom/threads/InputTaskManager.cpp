/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputTaskManager.h"
#include "VsyncTaskManager.h"
#include "nsRefreshDriver.h"

namespace mozilla {

StaticRefPtr<InputTaskManager> InputTaskManager::gInputTaskManager;

void InputTaskManager::EnableInputEventPrioritization() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mInputQueueState == STATE_DISABLED);
  mInputQueueState = STATE_ENABLED;
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
  // When the state is disabled, the input task that we have is
  // very likely SuspendInputEventQueue, so here we also use
  // normal priority as ResumeInputEventQueue, FlushInputEventQueue and
  // SetInputEventQueueEnabled all uses normal priority, to
  // ensure the ordering is correct.
  if (State() == InputTaskManager::STATE_DISABLED) {
    return static_cast<int32_t>(EventQueuePriority::Normal) -
           static_cast<int32_t>(EventQueuePriority::InputHigh);
  }

  return GetPriorityModifierForEventLoopTurnForStrictVsyncAlignment();
}

void InputTaskManager::WillRunTask() {
  TaskManager::WillRunTask();
  mInputPriorityController.WillRunTask();
}

int32_t
InputTaskManager::GetPriorityModifierForEventLoopTurnForStrictVsyncAlignment() {
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
    : mInputVsyncState(InputVsyncState::NoPendingVsync) {}

bool InputTaskManager::InputPriorityController::ShouldUseHighestPriority(
    InputTaskManager* aInputTaskManager) {
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
  MOZ_ASSERT(mInputVsyncState == InputVsyncState::NoPendingVsync);

  mInputVsyncState = InputVsyncState::HasPendingVsync;
  mMaxInputTasksToRun = aNumPendingTasks;
  mRunInputStartTime = TimeStamp::Now();
}

void InputTaskManager::InputPriorityController::WillRunVsync() {
  if (mInputVsyncState == InputVsyncState::RunVsync ||
      mInputVsyncState == InputVsyncState::HasPendingVsync) {
    LeavePendingVsyncState(false);
  }
}

void InputTaskManager::InputPriorityController::LeavePendingVsyncState(
    bool aRunVsync) {
  if (aRunVsync) {
    MOZ_ASSERT(mInputVsyncState == InputVsyncState::HasPendingVsync);
    mInputVsyncState = InputVsyncState::RunVsync;
  } else {
    mInputVsyncState = InputVsyncState::NoPendingVsync;
  }

  mMaxInputTasksToRun = 0;
}

void InputTaskManager::InputPriorityController::WillRunTask() {
  switch (mInputVsyncState) {
    case InputVsyncState::NoPendingVsync:
      return;
    case InputVsyncState::HasPendingVsync:
      MOZ_ASSERT(mMaxInputTasksToRun > 0);
      --mMaxInputTasksToRun;
      if (!mMaxInputTasksToRun ||
          TimeStamp::Now() - mRunInputStartTime >=
              TimeDuration::FromMilliseconds(
                  StaticPrefs::dom_input_event_queue_duration_max())) {
        LeavePendingVsyncState(true);
      }
      return;
    default:
      MOZ_DIAGNOSTIC_ASSERT(
          false, "Shouldn't run this input task when we suppose to run vsync");
      return;
  }
}

// static
void InputTaskManager::Init() { gInputTaskManager = new InputTaskManager(); }

}  // namespace mozilla
