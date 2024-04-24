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

struct UnprocessedModuleLoadInfoContainer final
    : public LinkedListElement<UnprocessedModuleLoadInfoContainer> {
  glue::EnhancedModuleLoadInfo mInfo;

  template <typename T>
  explicit UnprocessedModuleLoadInfoContainer(T&& aInfo)
      : mInfo(std::move(aInfo)) {}

  UnprocessedModuleLoadInfoContainer(
      const UnprocessedModuleLoadInfoContainer&) = delete;
  UnprocessedModuleLoadInfoContainer& operator=(
      const UnprocessedModuleLoadInfoContainer&) = delete;
};
using UnprocessedModuleLoads =
    AutoCleanLinkedList<UnprocessedModuleLoadInfoContainer>;

class UntrustedModulesProcessor final : public nsIObserver,
                                        public nsIThreadPoolListener {
 public:
  static RefPtr<UntrustedModulesProcessor> Create(
      bool aIsReadyForBackgroundProcessing);

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITHREADPOOLLISTENER

  // Called to check if the parent process is ready when a child process
  // is spanwed
  bool IsReadyForBackgroundProcessing() const;

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
  explicit UntrustedModulesProcessor(bool aIsReadyForBackgroundProcessing);

  static bool IsSupportedProcessType();

  void AddObservers();
  void RemoveObservers();

  void ScheduleNonEmptyQueueProcessing(const MutexAutoLock& aProofOfLock)
      MOZ_REQUIRES(mUnprocessedMutex);
  void CancelScheduledProcessing(const MutexAutoLock& aProofOfLock)
      MOZ_REQUIRES(mUnprocessedMutex);
  void DispatchBackgroundProcessing();

  void BackgroundProcessModuleLoadQueue();
  void ProcessModuleLoadQueue();

  // Extract the loading events from mUnprocessedModuleLoads to process and
  // move to mProcessedModuleLoads.  It's guaranteed that the total length of
  // mProcessedModuleLoads will not exceed |aMaxLength|.
  UnprocessedModuleLoads ExtractLoadingEventsToProcess(size_t aMaxLength);

  class ModulesMapResultWithLoads final {
   public:
    ModulesMapResultWithLoads(Maybe<ModulesMapResult>&& aModMapResult,
                              UnprocessedModuleLoads&& aLoads)
        : mModMapResult(std::move(aModMapResult)), mLoads(std::move(aLoads)) {}
    Maybe<ModulesMapResult> mModMapResult;
    UnprocessedModuleLoads mLoads;
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

  // This function is only called by the parent process
  RefPtr<ModuleRecord> GetOrAddModuleRecord(const ModuleEvaluator& aModEval,
                                            const nsAString& aResolvedNtPath);

  // Only called by child processes
  RefPtr<ModuleRecord> GetModuleRecord(
      const ModulesMap& aModules,
      const glue::EnhancedModuleLoadInfo& aModuleLoadInfo);

  RefPtr<GetModulesTrustIpcPromise> SendGetModulesTrust(ModulePaths&& aModules,
                                                        Priority aPriority);

  void CompleteProcessing(ModulesMapResultWithLoads&& aModulesAndLoads);
  RefPtr<UntrustedModulesPromise> GetAllProcessedData(StaticString aSource);

 private:
  RefPtr<LazyIdleThread> mThread;

  Mutex mThreadHandleMutex;
  Mutex mUnprocessedMutex;
  Mutex mModuleCacheMutex;

  // Windows HANDLE for the currently active mThread, if active.
  nsAutoHandle mThreadHandle MOZ_GUARDED_BY(mThreadHandleMutex);

  // The members in this group are protected by mUnprocessedMutex
  UnprocessedModuleLoads mUnprocessedModuleLoads
      MOZ_GUARDED_BY(mUnprocessedMutex);
  nsCOMPtr<nsIRunnable> mIdleRunnable MOZ_GUARDED_BY(mUnprocessedMutex);

  // This member must only be touched on mThread
  UntrustedModulesData mProcessedModuleLoads;

  enum class Status { StartingUp, Allowed, ShuttingDown };

  // This member may be touched by any thread
  Atomic<Status> mStatus;

  // Cache all module records, including ones trusted and ones loaded in
  // child processes, in the browser process to avoid evaluating the same
  // module multiple times
  ModulesMap mGlobalModuleCache MOZ_GUARDED_BY(mModuleCacheMutex);
};

}  // namespace mozilla

#endif  // mozilla_UntrustedModulesProcessor_h
