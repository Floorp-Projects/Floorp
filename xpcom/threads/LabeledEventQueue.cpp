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

using EpochQueueEntry = SchedulerGroup::EpochQueueEntry;

LinkedList<SchedulerGroup>* LabeledEventQueue::sSchedulerGroups;
size_t LabeledEventQueue::sLabeledEventQueueCount;
SchedulerGroup* LabeledEventQueue::sCurrentSchedulerGroup;

LabeledEventQueue::LabeledEventQueue(EventPriority aPriority)
  : mPriority(aPriority)
{
  // LabeledEventQueue should only be used by one consumer since it uses a
  // single static sSchedulerGroups field. It's hard to assert this, though, so
  // we assert NS_IsMainThread(), which is a reasonable proxy.
  MOZ_ASSERT(NS_IsMainThread());

  if (sLabeledEventQueueCount++ == 0) {
    sSchedulerGroups = new LinkedList<SchedulerGroup>();
  }
}

LabeledEventQueue::~LabeledEventQueue()
{
  if (--sLabeledEventQueueCount == 0) {
    delete sSchedulerGroups;
    sSchedulerGroups = nullptr;
  }
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

  return labelable->IsReadyToRun();
}

void
LabeledEventQueue::PutEvent(already_AddRefed<nsIRunnable>&& aEvent,
                            EventPriority aPriority,
                            const MutexAutoLock& aProofOfLock)
{
  MOZ_ASSERT(aPriority == mPriority);

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

  RunnableEpochQueue& queue = isLabeled ? group->GetQueue(aPriority) : mUnlabeled;
  queue.Push(EpochQueueEntry(event.forget(), epoch->mEpochNumber));

  if (group && group->EnqueueEvent() == SchedulerGroup::NewlyQueued) {
    // This group didn't have any events before. Add it to the
    // sSchedulerGroups list.
    MOZ_ASSERT(!group->isInList());
    sSchedulerGroups->insertBack(group);
    if (!sCurrentSchedulerGroup) {
      sCurrentSchedulerGroup = group;
    }
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

// Returns the next SchedulerGroup after |aGroup| in sSchedulerGroups. Wraps
// around to the beginning of the list when we hit the end.
/* static */ SchedulerGroup*
LabeledEventQueue::NextSchedulerGroup(SchedulerGroup* aGroup)
{
  SchedulerGroup* result = aGroup->getNext();
  if (!result) {
    result = sSchedulerGroups->getFirst();
  }
  return result;
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
    EpochQueueEntry& first = mUnlabeled.FirstElement();
    if (!IsReadyToRun(first.mRunnable, nullptr)) {
      return nullptr;
    }

    PopEpoch();
    EpochQueueEntry entry = mUnlabeled.Pop();
    MOZ_ASSERT(entry.mEpochNumber == epoch.mEpochNumber);
    MOZ_ASSERT(entry.mRunnable.get());
    return entry.mRunnable.forget();
  }

  if (!sCurrentSchedulerGroup) {
    return nullptr;
  }

  // Move visible tabs to the front of the queue. The mAvoidVisibleTabCount field
  // prevents us from preferentially processing events from visible tabs twice in
  // a row. This scheme is designed to prevent starvation.
  if (TabChild::HasVisibleTabs() && mAvoidVisibleTabCount <= 0) {
    for (auto iter = TabChild::GetVisibleTabs().ConstIter();
         !iter.Done(); iter.Next()) {
      SchedulerGroup* group = iter.Get()->GetKey()->TabGroup();
      if (!group->isInList() || group == sCurrentSchedulerGroup) {
        continue;
      }

      // For each visible tab we move to the front of the queue, we have to
      // process two SchedulerGroups (the visible tab and another one, presumably
      // a background group) before we prioritize visible tabs again.
      mAvoidVisibleTabCount += 2;

      // We move |group| right before sCurrentSchedulerGroup and then set
      // sCurrentSchedulerGroup to group.
      MOZ_ASSERT(group != sCurrentSchedulerGroup);
      group->removeFrom(*sSchedulerGroups);
      sCurrentSchedulerGroup->setPrevious(group);
      sCurrentSchedulerGroup = group;
    }
  }

  // Iterate over each SchedulerGroup once, starting at sCurrentSchedulerGroup.
  SchedulerGroup* firstGroup = sCurrentSchedulerGroup;
  SchedulerGroup* group = firstGroup;
  do {
    mAvoidVisibleTabCount--;

    RunnableEpochQueue& queue = group->GetQueue(mPriority);

    if (queue.IsEmpty()) {
      // This can happen if |group| is in a different LabeledEventQueue than |this|.
      group = NextSchedulerGroup(group);
      continue;
    }

    EpochQueueEntry& first = queue.FirstElement();
    if (first.mEpochNumber == epoch.mEpochNumber &&
        IsReadyToRun(first.mRunnable, group)) {
      sCurrentSchedulerGroup = NextSchedulerGroup(group);

      PopEpoch();

      if (group->DequeueEvent() == SchedulerGroup::NoLongerQueued) {
        // Now we can take group out of sSchedulerGroups.
        if (sCurrentSchedulerGroup == group) {
          // Since we changed sCurrentSchedulerGroup above, we'll only get here
          // if |group| was the only element in sSchedulerGroups. In that case
          // set sCurrentSchedulerGroup to null.
          MOZ_ASSERT(group->getNext() == nullptr);
          MOZ_ASSERT(group->getPrevious() == nullptr);
          sCurrentSchedulerGroup = nullptr;
        }
        group->removeFrom(*sSchedulerGroups);
      }
      EpochQueueEntry entry = queue.Pop();
      return entry.mRunnable.forget();
    }

    group = NextSchedulerGroup(group);
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
    EpochQueueEntry& entry = mUnlabeled.FirstElement();
    return IsReadyToRun(entry.mRunnable, nullptr);
  }

  // Go through the scheduler groups and look for one that has events
  // for the priority of this labeled queue that is in the current
  // epoch and is allowed to run.
  uintptr_t currentEpoch = frontEpoch.mEpochNumber;
  for (SchedulerGroup* group : *sSchedulerGroups) {
    RunnableEpochQueue& queue = group->GetQueue(mPriority);
    if (queue.IsEmpty()) {
      continue;
    }

    EpochQueueEntry& entry = queue.FirstElement();
    if (entry.mEpochNumber != currentEpoch) {
      continue;
    }

    if (IsReadyToRun(entry.mRunnable, group)) {
      return true;
    }
  }

  return false;
}
