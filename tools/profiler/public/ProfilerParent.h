/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfilerParent_h
#define ProfilerParent_h

#include "mozilla/RefPtr.h"
#include "mozilla/PProfilerParent.h"

class nsIProfilerStartParams;

namespace mozilla {

class ProfilerParentTracker;

// This is the main process side of the PProfiler protocol.
// ProfilerParent instances only exist on the main thread of the main process.
// The other side (ProfilerChild) lives on a background thread in the other
// process.
// The creation of PProfiler actors is initiated from the main process, after
// the other process has been launched.
// ProfilerParent instances are destroyed once the message channel closes,
// which can be triggered by either process, depending on which one shuts down
// first.
// All ProfilerParent instances are registered with a manager class called
// ProfilerParentTracker, which has the list of living ProfilerParent instances
// and handles shutdown.
class ProfilerParent final : public PProfilerParent
{
public:
  NS_INLINE_DECL_REFCOUNTING(ProfilerParent)

  static mozilla::ipc::Endpoint<PProfilerChild> CreateForProcess(base::ProcessId aOtherPid);

  typedef MozPromise<Shmem, ResponseRejectReason, true> SingleProcessProfilePromise;

  // The following static methods can be called on any thread, but they are
  // no-ops on anything other than the main thread.
  // If called on the main thread, the call will be broadcast to all
  // registered processes (all processes for which we have a ProfilerParent
  // object).
  // At the moment, the main process always calls these methods on the main
  // thread, and that's the only process in which we need to forward these
  // calls to other processes. The other processes will call these methods on
  // the ProfilerChild background thread, but those processes don't need to
  // forward these calls any further.

  // Returns the number of profiles to expect. The gathered profiles will be
  // provided asynchronously with a call to ProfileGatherer::ReceiveGatheredProfile.
  static nsTArray<RefPtr<SingleProcessProfilePromise>> GatherProfiles();

  static void ProfilerStarted(nsIProfilerStartParams* aParams);
  static void ProfilerStopped();
  static void ProfilerPaused();
  static void ProfilerResumed();

private:
  friend class ProfilerParentTracker;

  ProfilerParent();
  virtual ~ProfilerParent();

  void Init();
  void ActorDestroy(ActorDestroyReason aActorDestroyReason) override;
  void DeallocPProfilerParent() override;

  RefPtr<ProfilerParent> mSelfRef;
  nsTArray<MozPromiseHolder<SingleProcessProfilePromise>> mPendingRequestedProfiles;
  bool mDestroyed;
};

} // namespace mozilla

#endif  // ProfilerParent_h
