/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChildProfilerController.h"

#include "ProfilerChild.h"

#include "mozilla/ProfilerState.h"
#include "mozilla/ipc/Endpoint.h"
#include "nsExceptionHandler.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"

using namespace mozilla::ipc;

namespace mozilla {

/* static */
already_AddRefed<ChildProfilerController> ChildProfilerController::Create(
    mozilla::ipc::Endpoint<PProfilerChild>&& aEndpoint) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  RefPtr<ChildProfilerController> cpc = new ChildProfilerController();
  cpc->Init(std::move(aEndpoint));
  return cpc.forget();
}

ChildProfilerController::ChildProfilerController()
    : mThread(nullptr, "ChildProfilerController::mThread") {
  MOZ_COUNT_CTOR(ChildProfilerController);
}

void ChildProfilerController::Init(Endpoint<PProfilerChild>&& aEndpoint) {
  RefPtr<nsIThread> newProfilerChildThread;
  if (NS_SUCCEEDED(NS_NewNamedThread("ProfilerChild",
                                     getter_AddRefs(newProfilerChildThread)))) {
    {
      auto lock = mThread.Lock();
      RefPtr<nsIThread>& lockedmThread = lock.ref();
      MOZ_ASSERT(!lockedmThread, "There is already a ProfilerChild thread");
      // Copy ref'd ptr into mThread. Don't move/swap, so that
      // newProfilerChildThread can be used below.
      lockedmThread = newProfilerChildThread;
    }
    // Now that mThread has been set, run SetupProfilerChild on the thread.
    newProfilerChildThread->Dispatch(
        NewRunnableMethod<Endpoint<PProfilerChild>&&>(
            "ChildProfilerController::SetupProfilerChild", this,
            &ChildProfilerController::SetupProfilerChild, std::move(aEndpoint)),
        NS_DISPATCH_NORMAL);
  }
}

nsCString ChildProfilerController::GrabShutdownProfileAndShutdown() {
  nsCString shutdownProfile;
  ShutdownAndMaybeGrabShutdownProfileFirst(&shutdownProfile);
  return shutdownProfile;
}

void ChildProfilerController::Shutdown() {
  ShutdownAndMaybeGrabShutdownProfileFirst(nullptr);
}

void ChildProfilerController::ShutdownAndMaybeGrabShutdownProfileFirst(
    nsCString* aOutShutdownProfile) {
  // First, get the owning reference out of mThread, so it cannot be used in
  // ChildProfilerController after this (including re-entrantly during the
  // profilerChildThread->Shutdown() inner event loop below).
  RefPtr<nsIThread> profilerChildThread;
  {
    auto lock = mThread.Lock();
    RefPtr<nsIThread>& lockedmThread = lock.ref();
    lockedmThread.swap(profilerChildThread);
  }
  if (profilerChildThread) {
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::ProfilerChildShutdownPhase,
        profiler_is_active()
            ? "Profiling - Dispatching ShutdownProfilerChild"_ns
            : "Not profiling - Dispatching ShutdownProfilerChild"_ns);
    profilerChildThread->Dispatch(
        NewRunnableMethod<nsCString*>(
            "ChildProfilerController::ShutdownProfilerChild", this,
            &ChildProfilerController::ShutdownProfilerChild,
            aOutShutdownProfile),
        NS_DISPATCH_NORMAL);
    // Shut down the thread. This call will spin until all runnables (including
    // the ShutdownProfilerChild runnable) have been processed.
    profilerChildThread->Shutdown();
    // At this point, `profilerChildThread` should be the last reference to the
    // thread, so it will now get destroyed.
  }
}

ChildProfilerController::~ChildProfilerController() {
  MOZ_COUNT_DTOR(ChildProfilerController);

#ifdef DEBUG
  {
    auto lock = mThread.Lock();
    RefPtr<nsIThread>& lockedmThread = lock.ref();
    MOZ_ASSERT(
        !lockedmThread,
        "Please call Shutdown before destroying ChildProfilerController");
  }
#endif
  MOZ_ASSERT(!mProfilerChild);
}

void ChildProfilerController::SetupProfilerChild(
    Endpoint<PProfilerChild>&& aEndpoint) {
  {
    auto lock = mThread.Lock();
    RefPtr<nsIThread>& lockedmThread = lock.ref();
    // We should be on the ProfilerChild thread. In rare cases, we could already
    // be in shutdown, in which case mThread is null; we still need to continue,
    // so that ShutdownProfilerChild can work on a valid mProfilerChild.
    MOZ_RELEASE_ASSERT(!lockedmThread ||
                       lockedmThread == NS_GetCurrentThread());
  }
  MOZ_ASSERT(aEndpoint.IsValid());

  mProfilerChild = new ProfilerChild();
  Endpoint<PProfilerChild> endpoint = std::move(aEndpoint);

  if (!endpoint.Bind(mProfilerChild)) {
    MOZ_CRASH("Failed to bind ProfilerChild!");
  }
}

void ChildProfilerController::ShutdownProfilerChild(
    nsCString* aOutShutdownProfile) {
  const bool isProfiling = profiler_is_active();
  if (aOutShutdownProfile) {
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::ProfilerChildShutdownPhase,
        isProfiling ? "Profiling - GrabShutdownProfile"_ns
                    : "Not profiling - GrabShutdownProfile"_ns);
    *aOutShutdownProfile = mProfilerChild->GrabShutdownProfile();
  }
  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::ProfilerChildShutdownPhase,
      isProfiling ? "Profiling - Destroying ProfilerChild"_ns
                  : "Not profiling - Destroying ProfilerChild"_ns);
  mProfilerChild->Destroy();
  mProfilerChild = nullptr;
  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::ProfilerChildShutdownPhase,
      isProfiling
          ? "Profiling - ShutdownProfilerChild complete, waiting for thread shutdown"_ns
          : "Not Profiling - ShutdownProfilerChild complete, waiting for thread shutdown"_ns);
}

}  // namespace mozilla
