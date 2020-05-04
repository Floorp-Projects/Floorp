/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_UntrustedModulesProcessor_h
#define mozilla_UntrustedModulesProcessor_h

#include "mozilla/Atomics.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/glue/WindowsDllServices.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UntrustedModulesData.h"
#include "mozilla/Vector.h"
#include "mozilla/WinHeaderOnlyUtils.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsIRunnable.h"
#include "nsISupportsImpl.h"
#include "nsString.h"

namespace mozilla {

class ModuleEvaluator;

using UntrustedModulesPromise =
    MozPromise<Maybe<UntrustedModulesData>, nsresult, true>;

using ModulesTrustPromise = MozPromise<ModulesMapResult, nsresult, true>;

using GetModulesTrustIpcPromise =
    MozPromise<Maybe<ModulesMapResult>, ipc::ResponseRejectReason, true>;

class UntrustedModulesProcessor final : public nsIObserver {
 public:
  static RefPtr<UntrustedModulesProcessor> Create();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  // Called by DLL Services to explicitly begin shutting down
  void Disable();

  // Called by DLL Services to submit module load data to the processor
  void Enqueue(glue::EnhancedModuleLoadInfo&& aModLoadInfo);
  void Enqueue(ModuleLoadInfoVec&& aEvents);

  // Called by telemetry to retrieve the processed data
  RefPtr<UntrustedModulesPromise> GetProcessedData();

  // Called by IPC actors in the parent process to evaluate module trust
  // on behalf of child processes
  RefPtr<ModulesTrustPromise> GetModulesTrust(ModulePaths&& aModPaths,
                                              bool aRunAtNormalPriority);

  UntrustedModulesProcessor(const UntrustedModulesProcessor&) = delete;
  UntrustedModulesProcessor(UntrustedModulesProcessor&&) = delete;
  UntrustedModulesProcessor& operator=(const UntrustedModulesProcessor&) =
      delete;
  UntrustedModulesProcessor& operator=(UntrustedModulesProcessor&&) = delete;

 private:
  ~UntrustedModulesProcessor() = default;
  UntrustedModulesProcessor();

  static bool IsSupportedProcessType();

  void AddObservers();
  void RemoveObservers();

  void ScheduleNonEmptyQueueProcessing(const MutexAutoLock& aProofOfLock);
  void CancelScheduledProcessing(const MutexAutoLock& aProofOfLock);
  void DispatchBackgroundProcessing();

  void BackgroundProcessModuleLoadQueue();
  void ProcessModuleLoadQueue();

  using LoadsVec = Vector<glue::EnhancedModuleLoadInfo>;

  class ModulesMapResultWithLoads final {
   public:
    ModulesMapResultWithLoads(Maybe<ModulesMapResult>&& aModMapResult,
                              LoadsVec&& aLoads)
        : mModMapResult(std::move(aModMapResult)), mLoads(std::move(aLoads)) {}
    Maybe<ModulesMapResult> mModMapResult;
    LoadsVec mLoads;
  };

  using GetModulesTrustPromise =
      MozPromise<Maybe<ModulesMapResultWithLoads>, nsresult, true>;

  enum class Priority { Default, Background };

  RefPtr<GetModulesTrustPromise> ProcessModuleLoadQueueChildProcess(
      Priority aPriority);
  void BackgroundProcessModuleLoadQueueChildProcess();

  void AssertRunningOnLazyIdleThread();

  RefPtr<UntrustedModulesPromise> GetProcessedDataInternal();
  RefPtr<UntrustedModulesPromise> GetProcessedDataInternalChildProcess();

  RefPtr<ModulesTrustPromise> GetModulesTrustInternal(
      ModulePaths&& aModPaths, bool aRunAtNormalPriority);
  RefPtr<ModulesTrustPromise> GetModulesTrustInternal(ModulePaths&& aModPaths);

  // These two functions are only called by the parent process
  RefPtr<ModuleRecord> GetOrAddModuleRecord(
      ModulesMap& aModules, const ModuleEvaluator& aModEval,
      const glue::EnhancedModuleLoadInfo& aModLoadInfo);
  RefPtr<ModuleRecord> GetOrAddModuleRecord(ModulesMap& aModules,
                                            const ModuleEvaluator& aModEval,
                                            const nsAString& aResolvedNtPath);

  // Only called by child processes
  RefPtr<ModuleRecord> GetModuleRecord(
      const ModulesMap& aModules,
      const glue::EnhancedModuleLoadInfo& aModuleLoadInfo);

  RefPtr<GetModulesTrustIpcPromise> SendGetModulesTrust(ModulePaths&& aModules,
                                                        Priority aPriority);

  void CompleteProcessing(ModulesMapResultWithLoads&& aModulesAndLoads);
  RefPtr<UntrustedModulesPromise> GetAllProcessedData(const char* aSource);

 private:
  RefPtr<LazyIdleThread> mThread;

  Mutex mUnprocessedMutex;

  // The members in this group are protected by mUnprocessedMutex
  Vector<glue::EnhancedModuleLoadInfo> mUnprocessedModuleLoads;
  nsCOMPtr<nsIRunnable> mIdleRunnable;

  // This member must only be touched on mThread
  UntrustedModulesData mProcessedModuleLoads;

  // This member may be touched by any thread
  Atomic<bool> mAllowProcessing;

  // This member must only be touched on mThread, making sure a hash table of
  // dependent modules is initialized once in a process when the first batch
  // of queued events is processed.  We don't need the hash table to process
  // subsequent queues because we're interested in modules imported via the
  // executable's Import Table which are all expected to be in the first queue.
  // A boolean flag is enough to control initialization because tasks of
  // processing queues are serialized.
  bool mIsFirstBatchProcessed;
};

}  // namespace mozilla

#endif  // mozilla_UntrustedModulesProcessor_h
