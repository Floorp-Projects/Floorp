/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChildProfilerController.h"
#include "nsThreadUtils.h"
#include "ProfilerChild.h"

using namespace mozilla::ipc;

namespace mozilla {

ChildProfilerController::ChildProfilerController()
{
  MOZ_COUNT_CTOR(ChildProfilerController);
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
}

Endpoint<PProfilerParent>
ChildProfilerController::SetUpEndpoints(base::ProcessId aOtherPid)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  Endpoint<PProfilerParent> parent;
  Endpoint<PProfilerChild> child;
  nsresult rv = PProfiler::CreateEndpoints(aOtherPid,
                                           base::GetCurrentProcId(),
                                           &parent, &child);

  if (NS_FAILED(rv)) {
    MOZ_CRASH("Failed to create top level actor for PProfiler!");
  }

  if (NS_SUCCEEDED(NS_NewNamedThread("ProfilerChild", getter_AddRefs(mThread)))) {
    // Now that mThread has been set, run SetupProfilerChild on the thread.
    mThread->Dispatch(NewRunnableMethod<Endpoint<PProfilerChild>&&>(
      this, &ChildProfilerController::SetupProfilerChild, Move(child)),
      NS_DISPATCH_NORMAL);
  }

  return parent;
}

nsCString
ChildProfilerController::GrabShutdownProfileAndShutdown()
{
  nsCString shutdownProfile;
  ShutdownAndMaybeGrabShutdownProfileFirst(&shutdownProfile);
  return shutdownProfile;
}

void
ChildProfilerController::Shutdown()
{
  ShutdownAndMaybeGrabShutdownProfileFirst(nullptr);
}

void
ChildProfilerController::ShutdownAndMaybeGrabShutdownProfileFirst(nsCString* aOutShutdownProfile)
{
  if (mThread) {
    mThread->Dispatch(NewRunnableMethod<nsCString*>(
      this, &ChildProfilerController::ShutdownProfilerChild, aOutShutdownProfile),
      NS_DISPATCH_NORMAL);
    // Shut down the thread. This call will spin until all runnables (including
    // the ShutdownProfilerChild runnable) have been processed.
    mThread->Shutdown();
    mThread = nullptr;
  }
}

ChildProfilerController::~ChildProfilerController()
{
  MOZ_COUNT_DTOR(ChildProfilerController);

  MOZ_ASSERT(!mThread, "Please call Shutdown before destroying ChildProfilerController");
  MOZ_ASSERT(!mProfilerChild);
}

void
ChildProfilerController::SetupProfilerChild(Endpoint<PProfilerChild>&& aEndpoint)
{
  MOZ_RELEASE_ASSERT(mThread == NS_GetCurrentThread());
  MOZ_ASSERT(aEndpoint.IsValid());

  mProfilerChild = new ProfilerChild();
  Endpoint<PProfilerChild> endpoint = Move(aEndpoint);

  if (!endpoint.Bind(mProfilerChild)) {
    MOZ_CRASH("Failed to bind ProfilerChild!");
  }
}

void
ChildProfilerController::ShutdownProfilerChild(nsCString* aOutShutdownProfile)
{
  MOZ_RELEASE_ASSERT(mThread == NS_GetCurrentThread());

  if (aOutShutdownProfile) {
    *aOutShutdownProfile = mProfilerChild->GrabShutdownProfile();
  }
  mProfilerChild->Destroy();
  mProfilerChild = nullptr;
}

} // namespace mozilla
