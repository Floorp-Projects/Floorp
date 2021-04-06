/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputTaskManager.h"

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
}

// static
void InputTaskManager::Init() { gInputTaskManager = new InputTaskManager(); }

}  // namespace mozilla
