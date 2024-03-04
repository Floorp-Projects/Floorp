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

ProfileAndAdditionalInformation
ChildProfilerController::GrabShutdownProfileAndShutdown() {
  ProfileAndAdditionalInformation profileAndAdditionalInformation;
  ShutdownAndMaybeGrabShutdownProfileFirst(&profileAndAdditionalInformation);
  return profileAndAdditionalInformation;
}

void ChildProfilerController::Shutdown() {
  ShutdownAndMaybeGrabShutdownProfileFirst(nullptr);
}

void ChildProfilerController::ShutdownAndMaybeGrabShutdownProfileFirst(
    ProfileAndAdditionalInformation* aOutShutdownProfileInformation) {
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
    if (profiler_is_active()) {
      CrashReporter::RecordAnnotationCString(
          CrashReporter::Annotation::ProfilerChildShutdownPhase,
          "Profiling - Dispatching ShutdownProfilerChild");
      profilerChildThread->Dispatch(
          NewRunnableMethod<ProfileAndAdditionalInformation*>(
              "ChildProfilerController::ShutdownProfilerChild", this,
              &ChildProfilerController::ShutdownProfilerChild,
              aOutShutdownProfileInformation),
          NS_DISPATCH_NORMAL);
      // Shut down the thread. This call will spin until all runnables
      // (including the ShutdownProfilerChild runnable) have been processed.
      profilerChildThread->Shutdown();
    } else {
      CrashReporter::RecordAnnotationCString(
          CrashReporter::Annotation::ProfilerChildShutdownPhase,
          "Not profiling - Running ShutdownProfilerChild");
      // If we're not profiling, this operation will be very quick, so it can be
      // done synchronously. This avoids having to manually shutdown the thread,
      // which runs a risky inner event loop, see bug 1613798.
      NS_DispatchAndSpinEventLoopUntilComplete(
          "ChildProfilerController::ShutdownProfilerChild SYNC"_ns,
          profilerChildThread,
          NewRunnableMethod<ProfileAndAdditionalInformation*>(
              "ChildProfilerController::ShutdownProfilerChild SYNC", this,
              &ChildProfilerController::ShutdownProfilerChild, nullptr));
    }
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
    ProfileAndAdditionalInformation* aOutShutdownProfileInformation) {
  const bool isProfiling = profiler_is_active();
  if (aOutShutdownProfileInformation) {
    CrashReporter::RecordAnnotationCString(
        CrashReporter::Annotation::ProfilerChildShutdownPhase,
        isProfiling ? "Profiling - GrabShutdownProfile"
                    : "Not profiling - GrabShutdownProfile");
    *aOutShutdownProfileInformation = mProfilerChild->GrabShutdownProfile();
  }
  CrashReporter::RecordAnnotationCString(
      CrashReporter::Annotation::ProfilerChildShutdownPhase,
      isProfiling ? "Profiling - Destroying ProfilerChild"
                  : "Not profiling - Destroying ProfilerChild");
  mProfilerChild->Destroy();
  mProfilerChild = nullptr;
  CrashReporter::RecordAnnotationCString(
      CrashReporter::Annotation::ProfilerChildShutdownPhase,
      isProfiling ? "Profiling - ShutdownProfilerChild complete, waiting for "
                    "thread shutdown"
                  : "Not Profiling - ShutdownProfilerChild complete, waiting "
                    "for thread shutdown");
}

}  // namespace mozilla
