/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LabeledEventQueue.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/TabGroup.h"
#include "mozilla/Scheduler.h"
#include "mozilla/SchedulerGroup.h"
#include "nsQueryObject.h"

using namespace mozilla::dom;

LabeledEventQueue::LabeledEventQueue()
{
}

static SchedulerGroup*
GetSchedulerGroup(nsIRunnable* aEvent)
{
  RefPtr<SchedulerGroup::Runnable> groupRunnable = do_QueryObject(aEvent);
  if (!groupRunnable) {
    // It's not labeled.
    return nullptr;
  }

  return groupRunnable->Group();
}

static bool
IsReadyToRun(nsIRunnable* aEvent, SchedulerGroup* aEventGroup)
{
  if (!Scheduler::AnyEventRunning()) {
    return true;
  }

  if (Scheduler::UnlabeledEventRunning()) {
    return false;
  }

  if (aEventGroup) {
    return !aEventGroup->IsRunning();
  }

  nsCOMPtr<nsILabelableRunnable> labelable = do_QueryInterface(aEvent);
  if (!labelable) {
    return false;
  }

  AutoTArray<RefPtr<SchedulerGroup>, 1> groups;
  bool labeled = labelable->GetAffectedSchedulerGroups(groups);
  if (!labeled) {
    return false;
  }

  for (SchedulerGroup* group : groups) {
    if (group->IsRunning()) {
      return false;
    }
  }
  return true;
}

void
LabeledEventQueue::PutEvent(already_AddRefed<nsIRunnable>&& aEvent,
                            EventPriority aPriority,
                            const MutexAutoLock& aProofOfLock)
{
  nsCOMPtr<nsIRunnable> event(aEvent);

  MOZ_ASSERT(event.get());

  SchedulerGroup* group = GetSchedulerGroup(event);
  bool isLabeled = !!group;

  // Create a new epoch if necessary.
  Epoch* epoch;
  if (mEpochs.IsEmpty()) {
    epoch = &mEpochs.Push(Epoch::First(isLabeled));
  } else {
    Epoch& lastEpoch = mEpochs.LastElement();
    if (lastEpoch.IsLabeled() != isLabeled) {
      epoch = &mEpochs.Push(lastEpoch.NextEpoch(isLabeled));
    } else {
      epoch = &lastEpoch;
    }
  }

  mNumEvents++;
  epoch->mNumEvents++;

  RunnableEpochQueue* queue = isLabeled ? mLabeled.LookupOrAdd(group) : &mUnlabeled;
  queue->Push(QueueEntry(event.forget(), epoch->mEpochNumber));

  if (group && !group->isInList()) {
    mSchedulerGroups.insertBack(group);
  }
}

void
LabeledEventQueue::PopEpoch()
{
  Epoch& epoch = mEpochs.FirstElement();
  MOZ_ASSERT(epoch.mNumEvents > 0);
  if (epoch.mNumEvents == 1) {
    mEpochs.Pop();
  } else {
    epoch.mNumEvents--;
  }

  mNumEvents--;
}

already_AddRefed<nsIRunnable>
LabeledEventQueue::GetEvent(EventPriority* aPriority,
                            const MutexAutoLock& aProofOfLock)
{
  if (mEpochs.IsEmpty()) {
    return nullptr;
  }

  Epoch epoch = mEpochs.FirstElement();
  if (!epoch.IsLabeled()) {
    QueueEntry entry = mUnlabeled.FirstElement();
    if (IsReadyToRun(entry.mRunnable, nullptr)) {
      PopEpoch();
      mUnlabeled.Pop();
      MOZ_ASSERT(entry.mEpochNumber == epoch.mEpochNumber);
      MOZ_ASSERT(entry.mRunnable.get());
      return entry.mRunnable.forget();
    }
  }

  // Move active tabs to the front of the queue. The mAvoidActiveTabCount field
  // prevents us from preferentially processing events from active tabs twice in
  // a row. This scheme is designed to prevent starvation.
  if (TabChild::HasActiveTabs() && mAvoidActiveTabCount <= 0) {
    for (TabChild* tabChild : TabChild::GetActiveTabs()) {
      SchedulerGroup* group = tabChild->TabGroup();
      if (!group->isInList() || group == mSchedulerGroups.getFirst()) {
        continue;
      }

      // For each active tab we move to the front of the queue, we have to
      // process two SchedulerGroups (the active tab and another one, presumably
      // a background group) before we prioritize active tabs again.
      mAvoidActiveTabCount += 2;

      group->removeFrom(mSchedulerGroups);
      mSchedulerGroups.insertFront(group);
    }
  }

  // Iterate over SchedulerGroups in order. Each time we pass by a
  // SchedulerGroup, we move it to the back of the list. This ensures that we
  // process SchedulerGroups in a round-robin order (ignoring active tab
  // prioritization).
  SchedulerGroup* firstGroup = mSchedulerGroups.getFirst();
  SchedulerGroup* group = firstGroup;
  do {
    RunnableEpochQueue* queue = mLabeled.Get(group);
    MOZ_ASSERT(queue);
    MOZ_ASSERT(!queue->IsEmpty());

    mAvoidActiveTabCount--;
    SchedulerGroup* next = group->removeAndGetNext();
    mSchedulerGroups.insertBack(group);

    QueueEntry entry = queue->FirstElement();
    if (entry.mEpochNumber == epoch.mEpochNumber &&
        IsReadyToRun(entry.mRunnable, group)) {
      PopEpoch();

      queue->Pop();
      if (queue->IsEmpty()) {
        mLabeled.Remove(group);
        group->removeFrom(mSchedulerGroups);
      }
      return entry.mRunnable.forget();
    }

    group = next;
  } while (group != firstGroup);

  return nullptr;
}

bool
LabeledEventQueue::IsEmpty(const MutexAutoLock& aProofOfLock)
{
  return mEpochs.IsEmpty();
}

size_t
LabeledEventQueue::Count(const MutexAutoLock& aProofOfLock) const
{
  return mNumEvents;
}

bool
LabeledEventQueue::HasReadyEvent(const MutexAutoLock& aProofOfLock)
{
  if (mEpochs.IsEmpty()) {
    return false;
  }

  Epoch& frontEpoch = mEpochs.FirstElement();

  if (!frontEpoch.IsLabeled()) {
    QueueEntry entry = mUnlabeled.FirstElement();
    return IsReadyToRun(entry.mRunnable, nullptr);
  }

  // Go through the labeled queues and look for one whose head is from the
  // current epoch and is allowed to run.
  uintptr_t currentEpoch = frontEpoch.mEpochNumber;
  for (auto iter = mLabeled.Iter(); !iter.Done(); iter.Next()) {
    SchedulerGroup* key = iter.Key();
    RunnableEpochQueue* queue = iter.Data();
    MOZ_ASSERT(!queue->IsEmpty());

    QueueEntry entry = queue->FirstElement();
    if (entry.mEpochNumber != currentEpoch) {
      continue;
    }

    if (IsReadyToRun(entry.mRunnable, key)) {
      return true;
    }
  }

  return false;
}
