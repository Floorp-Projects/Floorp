/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfilerParent_h
#define ProfilerParent_h

#include "mozilla/PProfilerParent.h"
#include "mozilla/RefPtr.h"

class nsIProfilerStartParams;

namespace mozilla {

class ProfileBufferGlobalController;
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
class ProfilerParent final : public PProfilerParent {
 public:
  NS_INLINE_DECL_REFCOUNTING(ProfilerParent, final)

  static mozilla::ipc::Endpoint<PProfilerChild> CreateForProcess(
      base::ProcessId aOtherPid);

#ifdef MOZ_GECKO_PROFILER
  using SingleProcessProfilePromise =
      MozPromise<IPCProfileAndAdditionalInformation, ResponseRejectReason,
                 true>;

  struct SingleProcessProfilePromiseAndChildPid {
    RefPtr<SingleProcessProfilePromise> profilePromise;
    base::ProcessId childPid;
  };

  using SingleProcessProgressPromise =
      MozPromise<GatherProfileProgress, ResponseRejectReason, true>;

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

  // Returns the profiles to expect, as promises and child pids.
  static nsTArray<SingleProcessProfilePromiseAndChildPid> GatherProfiles();

  // Send a request to get the GatherProfiles() progress update from one child
  // process, returns a promise to be resolved with that progress.
  // The promise RefPtr may be null if the child process is unknown.
  // Progress may be invalid, if the request arrived after the child process
  // had already responded to the main GatherProfile() IPC, or something went
  // very wrong in that process.
  static RefPtr<SingleProcessProgressPromise> RequestGatherProfileProgress(
      base::ProcessId aChildPid);

  // This will start the profiler in all child processes. The returned promise
  // will be resolved when all child have completed their operation
  // (successfully or not.)
  [[nodiscard]] static RefPtr<GenericPromise> ProfilerStarted(
      nsIProfilerStartParams* aParams);
  static void ProfilerWillStopIfStarted();
  [[nodiscard]] static RefPtr<GenericPromise> ProfilerStopped();
  [[nodiscard]] static RefPtr<GenericPromise> ProfilerPaused();
  [[nodiscard]] static RefPtr<GenericPromise> ProfilerResumed();
  [[nodiscard]] static RefPtr<GenericPromise> ProfilerPausedSampling();
  [[nodiscard]] static RefPtr<GenericPromise> ProfilerResumedSampling();
  static void ClearAllPages();

  [[nodiscard]] static RefPtr<GenericPromise> WaitOnePeriodicSampling();

  // Create a "Final" update that the Child can return to its Parent.
  static ProfileBufferChunkManagerUpdate MakeFinalUpdate();

  // True if the ProfilerParent holds a lock on this thread.
  static bool IsLockedOnCurrentThread();

 private:
  friend class ProfileBufferGlobalController;
  friend class ProfilerParentTracker;

  explicit ProfilerParent(base::ProcessId aChildPid);

  void Init();
  void ActorDestroy(ActorDestroyReason aActorDestroyReason) override;

  void RequestChunkManagerUpdate();

  base::ProcessId mChildPid;
  nsTArray<MozPromiseHolder<SingleProcessProfilePromise>>
      mPendingRequestedProfiles;
  bool mDestroyed;
#endif  // MOZ_GECKO_PROFILER

 private:
  virtual ~ProfilerParent();
};

}  // namespace mozilla

#endif  // ProfilerParent_h
