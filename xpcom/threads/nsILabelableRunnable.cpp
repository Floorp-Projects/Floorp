/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsILabelableRunnable.h"

#include "mozilla/Scheduler.h"
#include "mozilla/SchedulerGroup.h"

bool
nsILabelableRunnable::IsReadyToRun()
{
  MOZ_ASSERT(mozilla::Scheduler::AnyEventRunning());
  MOZ_ASSERT(!mozilla::Scheduler::UnlabeledEventRunning());
  SchedulerGroupSet groups;
  if (!GetAffectedSchedulerGroups(groups)) {
    // it can not be labeled right now.
    return false;
  }

  if (groups.mSingle) {
    MOZ_ASSERT(groups.mMulti.isNothing());
    return !groups.mSingle->IsRunning();
  }

  if (groups.mMulti.isSome()) {
    MOZ_ASSERT(!groups.mSingle);
    for (auto iter = groups.mMulti.ref().ConstIter(); !iter.Done(); iter.Next()) {
      if (iter.Get()->GetKey()->IsRunning()) {
        return false;
      }
    }
    return true;
  }

  // No affected groups if we are here. Then, it's ready to run.
  return true;
}

void
nsILabelableRunnable::SchedulerGroupSet::Put(mozilla::SchedulerGroup* aGroup)
{
  if (mSingle) {
    MOZ_ASSERT(mMulti.isNothing());
    mMulti.emplace();
    auto& multi = mMulti.ref();
    multi.PutEntry(mSingle);
    multi.PutEntry(aGroup);
    mSingle = nullptr;
    return;
  }

  if (mMulti.isSome()) {
    MOZ_ASSERT(!mSingle);
    mMulti.ref().PutEntry(aGroup);
    return;
  }

  mSingle = aGroup;
}

void
nsILabelableRunnable::SchedulerGroupSet::Clear()
{
  mSingle = nullptr;
  mMulti.reset();
}

void
nsILabelableRunnable::SchedulerGroupSet::SetIsRunning(bool aIsRunning)
{
  if (mSingle) {
    MOZ_ASSERT(mMulti.isNothing());
    mSingle->SetIsRunning(aIsRunning);
    return;
  }

  if (mMulti.isSome()) {
    MOZ_ASSERT(!mSingle);
    for (auto iter = mMulti.ref().ConstIter(); !iter.Done(); iter.Next()) {
      MOZ_ASSERT(iter.Get()->GetKey()->IsRunning() != aIsRunning);
      iter.Get()->GetKey()->SetIsRunning(aIsRunning);
    }
    return;
  }
}
