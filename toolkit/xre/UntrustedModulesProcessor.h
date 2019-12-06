/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_UntrustedModulesProcessor_h
#define mozilla_UntrustedModulesProcessor_h

#include "mozilla/Atomics.h"
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
#include "nsIThread.h"
#include "nsString.h"

namespace mozilla {

class ModuleEvaluator;

using UntrustedModulesPromise =
    MozPromise<Maybe<UntrustedModulesData>, nsresult, true>;

class UntrustedModulesProcessor final : public nsIObserver {
 public:
  static RefPtr<UntrustedModulesProcessor> Create();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  void Enqueue(glue::EnhancedModuleLoadInfo&& aModLoadInfo);
  void Enqueue(ModuleLoadInfoVec&& aEvents);

  RefPtr<UntrustedModulesPromise> GetProcessedData();

  UntrustedModulesProcessor(const UntrustedModulesProcessor&) = delete;
  UntrustedModulesProcessor(UntrustedModulesProcessor&&) = delete;
  UntrustedModulesProcessor& operator=(const UntrustedModulesProcessor&) =
      delete;
  UntrustedModulesProcessor& operator=(UntrustedModulesProcessor&&) = delete;

 private:
  ~UntrustedModulesProcessor() = default;
  UntrustedModulesProcessor();
  void AddObservers();
  void RemoveObservers();
  void ScheduleNonEmptyQueueProcessing(const char* aSource,
                                       const MutexAutoLock& aProofOfLock);
  void CancelScheduledProcessing(const MutexAutoLock& aProofOfLock);
  void DispatchBackgroundProcessing(const char* aSource);
  void BackgroundProcessModuleLoadQueue(const char* aSource);
  void ProcessModuleLoadQueue(const char* aSource);
  void AssertRunningOnLazyIdleThread();
  RefPtr<UntrustedModulesPromise> GetProcessedDataInternal();
  RefPtr<ModuleRecord> GetModuleRecord(
      UntrustedModulesData::ModulesMap& aModules,
      const ModuleEvaluator& aModEval, const nsAString& aResolvedNtPath);

  RefPtr<LazyIdleThread> mThread;

  Mutex mUnprocessedMutex;

  // The members in this group are protected by mUnprocessedMutex
  Vector<glue::EnhancedModuleLoadInfo> mUnprocessedModuleLoads;
  nsCOMPtr<nsIRunnable> mIdleRunnable;

  // This member must only be touched on mThread
  UntrustedModulesData mProcessedModuleLoads;

  // This member may be touched by any thread
  Atomic<bool> mAllowProcessing;
};

}  // namespace mozilla

#endif  // mozilla_UntrustedModulesProcessor_h
