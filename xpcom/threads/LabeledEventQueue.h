/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_LabeledEventQueue_h
#define mozilla_LabeledEventQueue_h

#include <stdint.h>
#include "mozilla/AbstractEventQueue.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Queue.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"

namespace mozilla {

class SchedulerGroup;

// LabeledEventQueue is actually a set of queues. There is one queue for each
// SchedulerGroup, as well as one queue for unlabeled events (those with no
// associated SchedulerGroup). When an event is added to a LabeledEventQueue, we
// query its SchedulerGroup and then add it to the appropriate queue. When an
// event is fetched, we heuristically pick a SchedulerGroup and return an event
// from its queue. Ideally the heuristic should give precedence to
// SchedulerGroups corresponding to the foreground tabs. The correctness of this
// data structure relies on the invariant that events from different
// SchedulerGroups cannot affect each other.
class LabeledEventQueue final : public AbstractEventQueue
{
public:
  LabeledEventQueue();
  ~LabeledEventQueue();

  void PutEvent(already_AddRefed<nsIRunnable>&& aEvent,
                EventPriority aPriority,
                const MutexAutoLock& aProofOfLock) final;
  already_AddRefed<nsIRunnable> GetEvent(EventPriority* aPriority,
                                         const MutexAutoLock& aProofOfLock) final;

  bool IsEmpty(const MutexAutoLock& aProofOfLock) final;
  size_t Count(const MutexAutoLock& aProofOfLock) const final;
  bool HasReadyEvent(const MutexAutoLock& aProofOfLock) final;

  void EnableInputEventPrioritization(const MutexAutoLock& aProofOfLock) final {}
  void FlushInputEventPrioritization(const MutexAutoLock& aProofOfLock) final {}
  void SuspendInputEventPrioritization(const MutexAutoLock& aProofOfLock) final {}
  void ResumeInputEventPrioritization(const MutexAutoLock& aProofOfLock) final {}

private:

  // The basic problem here is to keep track of the ordering relationships
  // between events. As long as there are only labeled events, there can be one
  // queue per SchedulerGroup. However, if an unlabeled event is pushed, we must
  // remember that it should run after all the labeled events currently in the
  // queue. To do this, the queues are arranged in "epochs". Each time the tail
  // of the queue transitions from labeled to unlabeled (or from unlabeled to
  // labeled) a new epoch starts. Events within different epochs are ordered
  // according to which epoch happened first. Within a labeled epoch, there is
  // one queue per SchedulerGroup. So events from different SchedulerGroups
  // within the same epoch are unordered with respect to each other. Within an
  // unlabeled epoch, there is a single queue that orders all the unlabeled
  // events.
  //
  // The data structures we use are:
  // 1. A queue of epochs. For each epoch, we store its epoch number. This number
  // is odd for labeled epochs and even for unlabeled epochs. We also store the
  // number of events in the epoch.
  // 2. A single queue for all unlabeled events. For each event in the queue, we
  // store the runnable as well as the epoch number.
  // 3. For labeled events, one queue for each SchedulerGroup. Each element in
  // these queues also keeps track of the epoch it belongs to.
  //
  // To push an event, we see if we can remain in the same epoch or if we have
  // to start a new one. If we have to start a new one, we push onto the epoch
  // queue. Then, based on whether the event is labeled or not, we push the
  // runnable and the epoch number into the appopriate queue.
  //
  // To pop an event, we look at the epoch at the front of the epoch queue. If
  // it is unlabeled, then we pop the first event in the unlabeled queue. If it
  // is labeled, we can pop from any of the SchedulerGroup queues. Then we
  // decrement the number of events in the current epoch. If this number reaches
  // zero, we pop from the epoch queue.

  struct QueueEntry
  {
    nsCOMPtr<nsIRunnable> mRunnable;
    uintptr_t mEpochNumber;

    QueueEntry(already_AddRefed<nsIRunnable> aRunnable, uintptr_t aEpoch)
      : mRunnable(aRunnable)
      , mEpochNumber(aEpoch)
    {}
  };

  struct Epoch
  {
    static Epoch First(bool aIsLabeled)
    {
      // Odd numbers are labeled, even are unlabeled.
      uintptr_t number = aIsLabeled ? 1 : 0;
      return Epoch(number, aIsLabeled);
    }

    static bool EpochNumberIsLabeled(uintptr_t aEpochNumber)
    {
      // Odd numbers are labeled, even are unlabeled.
      return (aEpochNumber & 1) ? true : false;
    }

    uintptr_t mEpochNumber;
    size_t mNumEvents;

    Epoch(uintptr_t aEpochNumber, bool aIsLabeled)
      : mEpochNumber(aEpochNumber)
      , mNumEvents(0)
    {
      MOZ_ASSERT(aIsLabeled == EpochNumberIsLabeled(aEpochNumber));
    }

    bool IsLabeled() const { return EpochNumberIsLabeled(mEpochNumber); }

    Epoch NextEpoch(bool aIsLabeled) const
    {
      MOZ_ASSERT(aIsLabeled == !IsLabeled());
      return Epoch(mEpochNumber + 1, aIsLabeled);
    }
  };

  void PopEpoch();
  static SchedulerGroup* NextSchedulerGroup(SchedulerGroup* aGroup);

  using RunnableEpochQueue = Queue<QueueEntry, 32>;
  using LabeledMap = nsClassHashtable<nsRefPtrHashKey<SchedulerGroup>, RunnableEpochQueue>;
  using EpochQueue = Queue<Epoch, 8>;

  // List of SchedulerGroups that might have events. This is static, so it
  // covers all LabeledEventQueues. If a SchedulerGroup is in this list, it may
  // not have an event in *this* LabeledEventQueue (although it will have an
  // event in *some* LabeledEventQueue). sCurrentSchedulerGroup cycles through
  // the elements of sSchedulerGroups in order.
  static LinkedList<SchedulerGroup>* sSchedulerGroups;
  static size_t sLabeledEventQueueCount;
  static SchedulerGroup* sCurrentSchedulerGroup;

  LabeledMap mLabeled;
  RunnableEpochQueue mUnlabeled;
  EpochQueue mEpochs;
  size_t mNumEvents = 0;

  // Number of SchedulerGroups that must be processed before we prioritize a
  // visible tab. This field is designed to guarantee a 1:1 interleaving between
  // foreground and background SchedulerGroups. For details, see its usage in
  // LabeledEventQueue.cpp.
  int64_t mAvoidVisibleTabCount = 0;
};

} // namespace mozilla

#endif // mozilla_LabeledEventQueue_h
