/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/SchedulerGroup.h"

#include <utility>

#include "jsfriendapi.h"
#include "mozilla/Atomics.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsINamed.h"
#include "nsQueryObject.h"
#include "nsThreadUtils.h"

using namespace mozilla;

namespace {

static Atomic<uint64_t> gEarliestUnprocessedVsync(0);

}  // namespace

/* static */
nsresult SchedulerGroup::UnlabeledDispatch(
    TaskCategory aCategory, already_AddRefed<nsIRunnable>&& aRunnable) {
  if (NS_IsMainThread()) {
    return NS_DispatchToCurrentThread(std::move(aRunnable));
  } else {
    return NS_DispatchToMainThread(std::move(aRunnable));
  }
}

/* static */
void SchedulerGroup::MarkVsyncReceived() {
  if (gEarliestUnprocessedVsync) {
    // If we've seen a vsync already, but haven't handled it, keep the
    // older one.
    return;
  }

  MOZ_ASSERT(!NS_IsMainThread());
  bool inconsistent = false;
  TimeStamp creation = TimeStamp::ProcessCreation(&inconsistent);
  if (inconsistent) {
    return;
  }

  gEarliestUnprocessedVsync = (TimeStamp::Now() - creation).ToMicroseconds();
}

/* static */
void SchedulerGroup::MarkVsyncRan() { gEarliestUnprocessedVsync = 0; }

SchedulerGroup::SchedulerGroup() : mIsRunning(false) {}

/* static */
nsresult SchedulerGroup::DispatchWithDocGroup(
    TaskCategory aCategory, already_AddRefed<nsIRunnable>&& aRunnable,
    dom::DocGroup* aDocGroup) {
  return LabeledDispatch(aCategory, std::move(aRunnable), aDocGroup);
}

/* static */
nsresult SchedulerGroup::Dispatch(TaskCategory aCategory,
                                  already_AddRefed<nsIRunnable>&& aRunnable) {
  return LabeledDispatch(aCategory, std::move(aRunnable), nullptr);
}

/* static */
nsresult SchedulerGroup::LabeledDispatch(
    TaskCategory aCategory, already_AddRefed<nsIRunnable>&& aRunnable,
    dom::DocGroup* aDocGroup) {
  nsCOMPtr<nsIRunnable> runnable(aRunnable);
  if (XRE_IsContentProcess()) {
    RefPtr<Runnable> internalRunnable =
        new Runnable(runnable.forget(), aDocGroup);
    return InternalUnlabeledDispatch(aCategory, internalRunnable.forget());
  }
  return UnlabeledDispatch(aCategory, runnable.forget());
}

/*static*/
nsresult SchedulerGroup::InternalUnlabeledDispatch(
    TaskCategory aCategory, already_AddRefed<Runnable>&& aRunnable) {
  if (NS_IsMainThread()) {
    // NS_DispatchToCurrentThread will not leak the passed in runnable
    // when it fails, so we don't need to do anything special.
    return NS_DispatchToCurrentThread(std::move(aRunnable));
  }

  RefPtr<Runnable> runnable(aRunnable);
  nsresult rv = NS_DispatchToMainThread(do_AddRef(runnable));
  if (NS_FAILED(rv)) {
    // Dispatch failed.  This is a situation where we would have used
    // NS_DispatchToMainThread rather than calling into the SchedulerGroup
    // machinery, and the caller would be expecting to leak the nsIRunnable
    // originally passed in.  But because we've had to wrap things up
    // internally, we were going to leak the nsIRunnable *and* our Runnable
    // wrapper.  But there's no reason that we have to leak our Runnable
    // wrapper; we can just leak the wrapped nsIRunnable, and let the caller
    // take care of unleaking it if they need to.
    Unused << runnable->mRunnable.forget().take();
    nsrefcnt refcnt = runnable.get()->Release();
    MOZ_RELEASE_ASSERT(refcnt == 1, "still holding an unexpected reference!");
  }

  return rv;
}

SchedulerGroup::Runnable::Runnable(already_AddRefed<nsIRunnable>&& aRunnable,
                                   dom::DocGroup* aDocGroup)
    : mozilla::Runnable("SchedulerGroup::Runnable"),
      mRunnable(std::move(aRunnable)),
      mDocGroup(aDocGroup) {}

dom::DocGroup* SchedulerGroup::Runnable::DocGroup() const { return mDocGroup; }

#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
NS_IMETHODIMP
SchedulerGroup::Runnable::GetName(nsACString& aName) {
  // Try to get a name from the underlying runnable.
  nsCOMPtr<nsINamed> named = do_QueryInterface(mRunnable);
  if (named) {
    named->GetName(aName);
  }
  if (aName.IsEmpty()) {
    aName.AssignLiteral("anonymous");
  }

  return NS_OK;
}
#endif

NS_IMETHODIMP
SchedulerGroup::Runnable::Run() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  // The runnable's destructor can have side effects, so try to execute it in
  // the scope of the SchedulerGroup.
  nsCOMPtr<nsIRunnable> runnable(std::move(mRunnable));
  return runnable->Run();
}

NS_IMETHODIMP
SchedulerGroup::Runnable::GetPriority(uint32_t* aPriority) {
  *aPriority = nsIRunnablePriority::PRIORITY_NORMAL;
  nsCOMPtr<nsIRunnablePriority> runnablePrio = do_QueryInterface(mRunnable);
  return runnablePrio ? runnablePrio->GetPriority(aPriority) : NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED(SchedulerGroup::Runnable, mozilla::Runnable,
                            nsIRunnablePriority, SchedulerGroup::Runnable)
