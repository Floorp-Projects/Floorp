/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "UntrustedModulesProcessor.h"

#include <windows.h>

#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/Likely.h"
#include "mozilla/Services.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "ModuleEvaluator.h"
#include "nsCOMPtr.h"
#include "nsIObserverService.h"
#include "nsXULAppAPI.h"

namespace mozilla {

/* static */
RefPtr<UntrustedModulesProcessor> UntrustedModulesProcessor::Create() {
#if defined(NIGHTLY_BUILD)
  if (!XRE_IsParentProcess()) {
    // Not currently supported outside the parent process
    return nullptr;
  }

  if (!Telemetry::CanRecordReleaseData()) {
    return nullptr;
  }

  RefPtr<UntrustedModulesProcessor> result(new UntrustedModulesProcessor());
  return result.forget();
#else
  return nullptr;
#endif  // defined(NIGHTLY_BUILD)
}

NS_IMPL_ISUPPORTS(UntrustedModulesProcessor, nsIObserver)

static const uint32_t kThreadTimeoutMS = 120000;  // 2 minutes

UntrustedModulesProcessor::UntrustedModulesProcessor()
    : mThread(new LazyIdleThread(kThreadTimeoutMS,
                                 NS_LITERAL_CSTRING("Untrusted Modules"),
                                 LazyIdleThread::ManualShutdown)),
      mUnprocessedMutex(
          "mozilla::UntrustedModulesProcessor::mUnprocessedMutex"),
      mAllowProcessing(true) {
  AddObservers();
}

void UntrustedModulesProcessor::AddObservers() {
  nsCOMPtr<nsIObserverService> obsServ(services::GetObserverService());
  obsServ->AddObserver(this, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID, false);
  obsServ->AddObserver(this, "xpcom-shutdown-threads", false);
}

NS_IMETHODIMP UntrustedModulesProcessor::Observe(nsISupports* aSubject,
                                                 const char* aTopic,
                                                 const char16_t* aData) {
  if (!strcmp(aTopic, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID)) {
    // No more background processing allowed beyond this point
    mAllowProcessing = false;
    MutexAutoLock lock(mUnprocessedMutex);
    CancelScheduledProcessing(lock);
    return NS_OK;
  }

  if (!strcmp(aTopic, "xpcom-shutdown-threads")) {
    mThread->Shutdown();

    RemoveObservers();

    mThread = nullptr;
    return NS_OK;
  }

  MOZ_ASSERT_UNREACHABLE("Not reachable");

  return NS_OK;
}

void UntrustedModulesProcessor::RemoveObservers() {
  nsCOMPtr<nsIObserverService> obsServ(services::GetObserverService());
  obsServ->RemoveObserver(this, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID);
  obsServ->RemoveObserver(this, "xpcom-shutdown-threads");
}

void UntrustedModulesProcessor::ScheduleNonEmptyQueueProcessing(
    const char* aSource, const MutexAutoLock& aProofOfLock) {
  // In case something tried to load a DLL during shutdown
  if (!mThread) {
    return;
  }

#if defined(ENABLE_TESTS)
  // Don't bother scheduling background processing in short-lived xpcshell
  // processes; it makes the test suites take too long.
  if (MOZ_UNLIKELY(mozilla::EnvHasValue("XPCSHELL_TEST_PROFILE_DIR"))) {
    return;
  }
#endif  // defined(ENABLE_TESTS)

  if (mIdleRunnable) {
    return;
  }

  if (!mAllowProcessing) {
    return;
  }

  // Schedule a runnable to trigger background processing once the main thread
  // has gone idle. We do it this way to ensure that we don't start doing a
  // bunch of processing during periods of heavy main thread activity.
  nsCOMPtr<nsIRunnable> idleRunnable(NewCancelableRunnableMethod<const char*>(
      "UntrustedModulesProcessor::DispatchBackgroundProcessing", this,
      &UntrustedModulesProcessor::DispatchBackgroundProcessing, aSource));

  if (NS_FAILED(NS_DispatchToMainThreadQueue(do_AddRef(idleRunnable),
                                             EventQueuePriority::Idle))) {
    return;
  }

  mIdleRunnable = std::move(idleRunnable);
}

void UntrustedModulesProcessor::CancelScheduledProcessing(
    const MutexAutoLock& aProofOfLock) {
  if (!mIdleRunnable) {
    return;
  }

  nsCOMPtr<nsICancelableRunnable> cancelable(do_QueryInterface(mIdleRunnable));
  if (cancelable) {
    // Stop the pending idle runnable from doing anything
    cancelable->Cancel();
  }

  mIdleRunnable = nullptr;
}

void UntrustedModulesProcessor::DispatchBackgroundProcessing(
    const char* aSource) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mAllowProcessing) {
    return;
  }

  nsCOMPtr<nsIRunnable> runnable(NewRunnableMethod<const char*>(
      "UntrustedModulesProcessor::BackgroundProcessModuleLoadQueue", this,
      &UntrustedModulesProcessor::BackgroundProcessModuleLoadQueue, aSource));

  mThread->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
}

void UntrustedModulesProcessor::Enqueue(
    glue::EnhancedModuleLoadInfo&& aModLoadInfo) {
  if (!mAllowProcessing) {
    return;
  }

  MutexAutoLock lock(mUnprocessedMutex);

  Unused << mUnprocessedModuleLoads.emplaceBack(std::move(aModLoadInfo));

  ScheduleNonEmptyQueueProcessing(__func__, lock);
}

void UntrustedModulesProcessor::Enqueue(ModuleLoadInfoVec&& aEvents) {
  if (!mAllowProcessing) {
    return;
  }

  MutexAutoLock lock(mUnprocessedMutex);

  for (auto& event : aEvents) {
    Unused << mUnprocessedModuleLoads.emplaceBack(std::move(event));
  }

  ScheduleNonEmptyQueueProcessing(__func__, lock);
}

void UntrustedModulesProcessor::AssertRunningOnLazyIdleThread() {
#if defined(DEBUG)
  PRThread* curThread;
  PRThread* lazyIdleThread;

  MOZ_ASSERT(NS_SUCCEEDED(NS_GetCurrentThread()->GetPRThread(&curThread)) &&
             NS_SUCCEEDED(mThread->GetPRThread(&lazyIdleThread)) &&
             curThread == lazyIdleThread);
#endif  // defined(DEBUG)
}

RefPtr<UntrustedModulesPromise> UntrustedModulesProcessor::GetProcessedData() {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<UntrustedModulesProcessor> self(this);
  return InvokeAsync(
      mThread->SerialEventTarget(), __func__,
      [self = std::move(self)]() { return self->GetProcessedDataInternal(); });
}

RefPtr<UntrustedModulesPromise>
UntrustedModulesProcessor::GetProcessedDataInternal() {
  AssertRunningOnLazyIdleThread();

  ProcessModuleLoadQueue(__func__);

  UntrustedModulesData result;

  if (!mProcessedModuleLoads) {
    return UntrustedModulesPromise::CreateAndResolve(Nothing(), __func__);
  }

  result.Swap(mProcessedModuleLoads);

  result.mElapsed = TimeStamp::Now() - TimeStamp::ProcessCreation();

  return UntrustedModulesPromise::CreateAndResolve(
      Some(UntrustedModulesData(std::move(result))), __func__);
}

class MOZ_RAII BackgroundPriorityRegion final {
 public:
  BackgroundPriorityRegion()
      : mIsBackground(::SetThreadPriority(::GetCurrentThread(),
                                          THREAD_MODE_BACKGROUND_BEGIN)) {}

  ~BackgroundPriorityRegion() {
    if (!mIsBackground) {
      return;
    }

    ::SetThreadPriority(::GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
  }

  BackgroundPriorityRegion(const BackgroundPriorityRegion&) = delete;
  BackgroundPriorityRegion(BackgroundPriorityRegion&&) = delete;
  BackgroundPriorityRegion& operator=(const BackgroundPriorityRegion&) = delete;
  BackgroundPriorityRegion& operator=(BackgroundPriorityRegion&&) = delete;

 private:
  const BOOL mIsBackground;
};

void UntrustedModulesProcessor::BackgroundProcessModuleLoadQueue(
    const char* aSource) {
  BackgroundPriorityRegion bgRgn;
  ProcessModuleLoadQueue(aSource);
}

RefPtr<ModuleRecord> UntrustedModulesProcessor::GetModuleRecord(
    UntrustedModulesData::ModulesMap& aModules, const ModuleEvaluator& aModEval,
    const nsAString& aResolvedNtPath) {
  nsAutoString resolvedDosPath;
  if (!NtPathToDosPath(aResolvedNtPath, resolvedDosPath)) {
    return nullptr;
  }

  auto addPtr = aModules.LookupForAdd(resolvedDosPath);
  if (addPtr) {
    return addPtr.Data();
  }

  RefPtr<ModuleRecord> newMod(new ModuleRecord(resolvedDosPath));

  Maybe<ModuleTrustFlags> maybeTrust = aModEval.GetTrust(*newMod);
  if (maybeTrust.isNothing()) {
    return nullptr;
  }

  newMod->mTrustFlags = maybeTrust.value();

  addPtr.OrInsert([newMod]() { return newMod; });

  return newMod;
}

// This function contains multiple |mAllowProcessing| checks so that we can
// quickly bail out at the first sign of shutdown. This may be important when
// the current thread is running under background priority.
void UntrustedModulesProcessor::ProcessModuleLoadQueue(const char* aSource) {
  Vector<glue::EnhancedModuleLoadInfo> loadsToProcess;

  {  // Scope for lock
    MutexAutoLock lock(mUnprocessedMutex);
    CancelScheduledProcessing(lock);
    loadsToProcess.swap(mUnprocessedModuleLoads);
  }

  if (!mAllowProcessing) {
    return;
  }

  ModuleEvaluator modEval;
  MOZ_ASSERT(!!modEval);
  if (!modEval) {
    return;
  }

  Telemetry::BatchProcessedStackGenerator stackProcessor;
  nsRefPtrHashtable<nsStringHashKey, ModuleRecord> modules;

  Maybe<double> maybeXulLoadDuration;
  Vector<Telemetry::ProcessedStack> processedStacks;
  Vector<ProcessedModuleLoadEvent> processedEvents;
  uint32_t sanitizationFailures = 0;
  uint32_t trustTestFailures = 0;

  for (glue::EnhancedModuleLoadInfo& entry : loadsToProcess) {
    if (!mAllowProcessing) {
      return;
    }

    RefPtr<ModuleRecord> module(GetModuleRecord(
        modules, modEval, entry.mNtLoadInfo.mSectionName.AsString()));
    if (!module) {
      // We failed to obtain trust information about the module.
      // Don't include test failures in the ping to avoid flooding it.
      ++trustTestFailures;
      continue;
    }

    if (!mAllowProcessing) {
      return;
    }

    glue::EnhancedModuleLoadInfo::BacktraceType backtrace =
      std::move(entry.mNtLoadInfo.mBacktrace);
    ProcessedModuleLoadEvent event(std::move(entry), std::move(module));

    if (!event) {
      // We don't have a sanitized DLL path, so we cannot include this event
      // for privacy reasons.
      ++sanitizationFailures;
      continue;
    }

    if (!mAllowProcessing) {
      return;
    }

    if (event.IsTrusted()) {
      if (event.IsXULLoad()) {
        maybeXulLoadDuration = event.mLoadDurationMS;
      }

      // Trusted modules are not included in the ping
      continue;
    }

    if (!mAllowProcessing) {
      return;
    }

    Telemetry::ProcessedStack processedStack =
        stackProcessor.GetStackAndModules(backtrace);

    if (!mAllowProcessing) {
      return;
    }

    Unused << processedStacks.emplaceBack(std::move(processedStack));
    Unused << processedEvents.emplaceBack(std::move(event));
  }

  if (processedStacks.empty() && processedEvents.empty() &&
      !sanitizationFailures && !trustTestFailures) {
    // Nothing to save
    return;
  }

  if (!mAllowProcessing) {
    return;
  }

  mProcessedModuleLoads.AddNewLoads(modules, std::move(processedEvents),
                                    std::move(processedStacks));
  if (maybeXulLoadDuration) {
    MOZ_ASSERT(!mProcessedModuleLoads.mXULLoadDurationMS);
    mProcessedModuleLoads.mXULLoadDurationMS = maybeXulLoadDuration;
  }

  mProcessedModuleLoads.mSanitizationFailures += sanitizationFailures;
  mProcessedModuleLoads.mTrustTestFailures += trustTestFailures;
}

}  // namespace mozilla
