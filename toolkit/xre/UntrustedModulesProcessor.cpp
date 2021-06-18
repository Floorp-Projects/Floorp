/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "UntrustedModulesProcessor.h"

#include <windows.h>

#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/Likely.h"
#include "mozilla/RDDParent.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "ModuleEvaluator.h"
#include "nsCOMPtr.h"
#include "nsHashKeys.h"
#include "nsIObserverService.h"
#include "nsTHashtable.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "private/prpriv.h"  // For PR_GetThreadID

static DWORD ToWin32ThreadId(nsIThread* aThread) {
  if (!aThread) {
    return 0UL;
  }

  PRThread* prThread;
  nsresult rv = aThread->GetPRThread(&prThread);
  if (NS_FAILED(rv)) {
    // Possible when a LazyInitThread's underlying nsThread is not present
    return 0UL;
  }

  return DWORD(::PR_GetThreadID(prThread));
}

namespace mozilla {

class MOZ_RAII BackgroundPriorityRegion final {
 public:
  BackgroundPriorityRegion()
      : mIsBackground(
            ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_IDLE)) {}

  ~BackgroundPriorityRegion() {
    if (!mIsBackground) {
      return;
    }

    Clear(::GetCurrentThread());
  }

  static void Clear(nsIThread* aThread) {
    DWORD tid = ToWin32ThreadId(aThread);
    if (!tid) {
      return;
    }

    nsAutoHandle thread(
        ::OpenThread(THREAD_SET_LIMITED_INFORMATION, FALSE, tid));
    if (!thread) {
      return;
    }

    Clear(thread);
  }

  BackgroundPriorityRegion(const BackgroundPriorityRegion&) = delete;
  BackgroundPriorityRegion(BackgroundPriorityRegion&&) = delete;
  BackgroundPriorityRegion& operator=(const BackgroundPriorityRegion&) = delete;
  BackgroundPriorityRegion& operator=(BackgroundPriorityRegion&&) = delete;

 private:
  static void Clear(HANDLE aThread) {
    DebugOnly<BOOL> ok = ::SetThreadPriority(aThread, THREAD_PRIORITY_NORMAL);
    MOZ_ASSERT(ok);
  }

 private:
  const BOOL mIsBackground;
};

/* static */
bool UntrustedModulesProcessor::IsSupportedProcessType() {
  switch (XRE_GetProcessType()) {
    case GeckoProcessType_Default:
    case GeckoProcessType_Content:
      return Telemetry::CanRecordReleaseData();
    case GeckoProcessType_RDD:
      // For RDD process, we check the telemetry settings in RDDChild::Init()
      // running in the browser process because CanRecordReleaseData() always
      // returns false here.
      return true;
    default:
      return false;
  }
}

/* static */
RefPtr<UntrustedModulesProcessor> UntrustedModulesProcessor::Create() {
  if (!IsSupportedProcessType()) {
    return nullptr;
  }

  RefPtr<UntrustedModulesProcessor> result(new UntrustedModulesProcessor());
  return result.forget();
}

NS_IMPL_ISUPPORTS(UntrustedModulesProcessor, nsIObserver)

static const uint32_t kThreadTimeoutMS = 120000;  // 2 minutes

UntrustedModulesProcessor::UntrustedModulesProcessor()
    : mThread(new LazyIdleThread(kThreadTimeoutMS, "Untrusted Modules"_ns,
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
  if (XRE_IsContentProcess()) {
    obsServ->AddObserver(this, "content-child-will-shutdown", false);
  }
}

void UntrustedModulesProcessor::Disable() {
  // Ensure that mThread cannot run at low priority anymore
  BackgroundPriorityRegion::Clear(mThread);

  // No more background processing allowed beyond this point
  if (!mAllowProcessing.exchange(false)) {
    return;
  }

  MutexAutoLock lock(mUnprocessedMutex);
  CancelScheduledProcessing(lock);
}

NS_IMETHODIMP UntrustedModulesProcessor::Observe(nsISupports* aSubject,
                                                 const char* aTopic,
                                                 const char16_t* aData) {
  if (!strcmp(aTopic, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID) ||
      !strcmp(aTopic, "content-child-will-shutdown")) {
    Disable();
    return NS_OK;
  }

  if (!strcmp(aTopic, "xpcom-shutdown-threads")) {
    Disable();
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
  if (XRE_IsContentProcess()) {
    obsServ->RemoveObserver(this, "content-child-will-shutdown");
  }
}

void UntrustedModulesProcessor::ScheduleNonEmptyQueueProcessing(
    const MutexAutoLock& aProofOfLock) {
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
  nsCOMPtr<nsIRunnable> idleRunnable(NewCancelableRunnableMethod(
      "UntrustedModulesProcessor::DispatchBackgroundProcessing", this,
      &UntrustedModulesProcessor::DispatchBackgroundProcessing));

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

void UntrustedModulesProcessor::DispatchBackgroundProcessing() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mAllowProcessing) {
    return;
  }

  nsCOMPtr<nsIRunnable> runnable(NewRunnableMethod(
      "UntrustedModulesProcessor::BackgroundProcessModuleLoadQueue", this,
      &UntrustedModulesProcessor::BackgroundProcessModuleLoadQueue));

  mThread->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
}

void UntrustedModulesProcessor::Enqueue(
    glue::EnhancedModuleLoadInfo&& aModLoadInfo) {
  if (!mAllowProcessing) {
    return;
  }

  DWORD bgThreadId = ToWin32ThreadId(mThread);
  if (aModLoadInfo.mNtLoadInfo.mThreadId == bgThreadId) {
    // Exclude loads that were caused by our own background thread
    return;
  }

  MutexAutoLock lock(mUnprocessedMutex);

  Unused << mUnprocessedModuleLoads.emplaceBack(std::move(aModLoadInfo));

  ScheduleNonEmptyQueueProcessing(lock);
}

void UntrustedModulesProcessor::Enqueue(ModuleLoadInfoVec&& aEvents) {
  if (!mAllowProcessing) {
    return;
  }

  // We do not need to attempt to exclude our background thread in this case
  // because |aEvents| was accumulated prior to |mThread|'s existence.

  MutexAutoLock lock(mUnprocessedMutex);

  for (auto& event : aEvents) {
    Unused << mUnprocessedModuleLoads.emplaceBack(std::move(event));
  }

  ScheduleNonEmptyQueueProcessing(lock);
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

  // Clear any background priority in case background processing is running.
  BackgroundPriorityRegion::Clear(mThread);

  RefPtr<UntrustedModulesProcessor> self(this);
  return InvokeAsync(
      mThread->SerialEventTarget(), __func__,
      [self = std::move(self)]() { return self->GetProcessedDataInternal(); });
}

RefPtr<ModulesTrustPromise> UntrustedModulesProcessor::GetModulesTrust(
    ModulePaths&& aModPaths, bool aRunAtNormalPriority) {
  MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());

  if (!mAllowProcessing) {
    return ModulesTrustPromise::CreateAndReject(
        NS_ERROR_ILLEGAL_DURING_SHUTDOWN, __func__);
  }

  RefPtr<UntrustedModulesProcessor> self(this);
  auto run = [self = std::move(self), modPaths = std::move(aModPaths),
              runNormal = aRunAtNormalPriority]() mutable {
    return self->GetModulesTrustInternal(std::move(modPaths), runNormal);
  };

  if (aRunAtNormalPriority) {
    // Clear any background priority in case background processing is running.
    BackgroundPriorityRegion::Clear(mThread);

    return InvokeAsync(mThread->SerialEventTarget(), __func__, std::move(run));
  }

  RefPtr<ModulesTrustPromise::Private> p(
      new ModulesTrustPromise::Private(__func__));
  nsCOMPtr<nsISerialEventTarget> evtTarget(mThread->SerialEventTarget());
  const char* source = __func__;

  auto runWrap = [evtTarget = std::move(evtTarget), p, source,
                  run = std::move(run)]() mutable -> void {
    InvokeAsync(evtTarget, source, std::move(run))->ChainTo(p.forget(), source);
  };

  nsCOMPtr<nsIRunnable> idleRunnable(
      NS_NewRunnableFunction(source, std::move(runWrap)));

  nsresult rv = NS_DispatchToMainThreadQueue(idleRunnable.forget(),
                                             EventQueuePriority::Idle);
  if (NS_FAILED(rv)) {
    p->Reject(rv, source);
  }

  return p;
}

RefPtr<UntrustedModulesPromise>
UntrustedModulesProcessor::GetProcessedDataInternal() {
  AssertRunningOnLazyIdleThread();
  if (!XRE_IsParentProcess()) {
    return GetProcessedDataInternalChildProcess();
  }

  ProcessModuleLoadQueue();

  return GetAllProcessedData(__func__);
}

RefPtr<UntrustedModulesPromise> UntrustedModulesProcessor::GetAllProcessedData(
    const char* aSource) {
  AssertRunningOnLazyIdleThread();

  UntrustedModulesData result;

  if (!mProcessedModuleLoads) {
    return UntrustedModulesPromise::CreateAndResolve(Nothing(), aSource);
  }

  result.Swap(mProcessedModuleLoads);

  result.mElapsed = TimeStamp::Now() - TimeStamp::ProcessCreation();

  return UntrustedModulesPromise::CreateAndResolve(
      Some(UntrustedModulesData(std::move(result))), aSource);
}

RefPtr<UntrustedModulesPromise>
UntrustedModulesProcessor::GetProcessedDataInternalChildProcess() {
  AssertRunningOnLazyIdleThread();
  MOZ_ASSERT(!XRE_IsParentProcess());

  RefPtr<GetModulesTrustPromise> whenProcessed(
      ProcessModuleLoadQueueChildProcess(Priority::Default));

  RefPtr<UntrustedModulesProcessor> self(this);
  RefPtr<UntrustedModulesPromise::Private> p(
      new UntrustedModulesPromise::Private(__func__));
  nsCOMPtr<nsISerialEventTarget> evtTarget(mThread->SerialEventTarget());

  const char* source = __func__;
  auto completionRoutine = [evtTarget = std::move(evtTarget), p,
                            self = std::move(self), source,
                            whenProcessed = std::move(whenProcessed)]() {
    MOZ_ASSERT(NS_IsMainThread());
    if (!self->mAllowProcessing) {
      // We can't do any more work, just reject all the things
      whenProcessed->Then(
          GetMainThreadSerialEventTarget(), source,
          [p, source](Maybe<ModulesMapResultWithLoads>&& aResult) {
            p->Reject(NS_ERROR_ILLEGAL_DURING_SHUTDOWN, source);
          },
          [p, source](nsresult aRv) { p->Reject(aRv, source); });
      return;
    }

    whenProcessed->Then(
        evtTarget, source,
        [p, self = std::move(self),
         source](Maybe<ModulesMapResultWithLoads>&& aResult) mutable {
          if (aResult.isSome()) {
            self->CompleteProcessing(std::move(aResult.ref()));
          }
          self->GetAllProcessedData(source)->ChainTo(p.forget(), source);
        },
        [p, source](nsresult aRv) { p->Reject(aRv, source); });
  };

  // We always send |completionRoutine| on a trip through the main thread
  // due to some subtlety with |mThread| being a LazyIdleThread: we can only
  // Dispatch or Then to |mThread| from its creating thread, which is the
  // main thread. Hopefully we can get rid of this in the future and just
  // invoke whenProcessed->Then() directly.
  nsresult rv = NS_DispatchToMainThread(
      NS_NewRunnableFunction(__func__, std::move(completionRoutine)));
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  if (NS_FAILED(rv)) {
    p->Reject(rv, __func__);
  }

  return p;
}

void UntrustedModulesProcessor::BackgroundProcessModuleLoadQueue() {
  if (!mAllowProcessing) {
    return;
  }

  BackgroundPriorityRegion bgRgn;

  if (!mAllowProcessing) {
    return;
  }

  ProcessModuleLoadQueue();
}

RefPtr<ModuleRecord> UntrustedModulesProcessor::GetOrAddModuleRecord(
    ModulesMap& aModules, const ModuleEvaluator& aModEval,
    const glue::EnhancedModuleLoadInfo& aModLoadInfo) {
  return GetOrAddModuleRecord(aModules, aModEval,
                              aModLoadInfo.mNtLoadInfo.mSectionName.AsString());
}

RefPtr<ModuleRecord> UntrustedModulesProcessor::GetOrAddModuleRecord(
    ModulesMap& aModules, const ModuleEvaluator& aModEval,
    const nsAString& aResolvedNtPath) {
  MOZ_ASSERT(XRE_IsParentProcess());

  return aModules.WithEntryHandle(
      aResolvedNtPath, [&](auto&& addPtr) -> RefPtr<ModuleRecord> {
        if (addPtr) {
          return addPtr.Data();
        }

        RefPtr<ModuleRecord> newMod(new ModuleRecord(aResolvedNtPath));
        if (!(*newMod)) {
          return nullptr;
        }

        Maybe<ModuleTrustFlags> maybeTrust = aModEval.GetTrust(*newMod);
        if (maybeTrust.isNothing()) {
          return nullptr;
        }

        newMod->mTrustFlags = maybeTrust.value();

        return addPtr.Insert(std::move(newMod));
      });
}

RefPtr<ModuleRecord> UntrustedModulesProcessor::GetModuleRecord(
    const ModulesMap& aModules,
    const glue::EnhancedModuleLoadInfo& aModuleLoadInfo) {
  MOZ_ASSERT(!XRE_IsParentProcess());

  return aModules.Get(aModuleLoadInfo.mNtLoadInfo.mSectionName.AsString());
}

void UntrustedModulesProcessor::BackgroundProcessModuleLoadQueueChildProcess() {
  RefPtr<GetModulesTrustPromise> whenProcessed(
      ProcessModuleLoadQueueChildProcess(Priority::Background));

  RefPtr<UntrustedModulesProcessor> self(this);
  nsCOMPtr<nsISerialEventTarget> evtTarget(mThread->SerialEventTarget());

  const char* source = __func__;
  auto completionRoutine = [evtTarget = std::move(evtTarget),
                            self = std::move(self), source,
                            whenProcessed = std::move(whenProcessed)]() {
    MOZ_ASSERT(NS_IsMainThread());
    if (!self->mAllowProcessing) {
      // We can't do any more work, just no-op
      whenProcessed->Then(
          GetMainThreadSerialEventTarget(), source,
          [](Maybe<ModulesMapResultWithLoads>&& aResult) {},
          [](nsresult aRv) {});
      return;
    }

    whenProcessed->Then(
        evtTarget, source,
        [self = std::move(self)](Maybe<ModulesMapResultWithLoads>&& aResult) {
          if (aResult.isNothing() || !self->mAllowProcessing) {
            // Nothing to do
            return;
          }

          BackgroundPriorityRegion bgRgn;
          self->CompleteProcessing(std::move(aResult.ref()));
        },
        [](nsresult aRv) {});
  };

  // We always send |completionRoutine| on a trip through the main thread
  // due to some subtlety with |mThread| being a LazyIdleThread: we can only
  // Dispatch or Then to |mThread| from its creating thread, which is the
  // main thread. Hopefully we can get rid of this in the future and just
  // invoke whenProcessed->Then() directly.
  DebugOnly<nsresult> rv = NS_DispatchToMainThread(
      NS_NewRunnableFunction(__func__, std::move(completionRoutine)));
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

// This function contains multiple |mAllowProcessing| checks so that we can
// quickly bail out at the first sign of shutdown. This may be important when
// the current thread is running under background priority.
void UntrustedModulesProcessor::ProcessModuleLoadQueue() {
  AssertRunningOnLazyIdleThread();
  if (!XRE_IsParentProcess()) {
    BackgroundProcessModuleLoadQueueChildProcess();
    return;
  }

  Vector<glue::EnhancedModuleLoadInfo> loadsToProcess;

  {  // Scope for lock
    MutexAutoLock lock(mUnprocessedMutex);
    CancelScheduledProcessing(lock);

    // The potential size of mProcessedModuleLoads if all of the unprocessed
    // events are from third-party modules.
    const size_t newDataLength = mProcessedModuleLoads.mEvents.length() +
                                 mUnprocessedModuleLoads.length();
    if (newDataLength <= UntrustedModulesData::kMaxEvents) {
      loadsToProcess.swap(mUnprocessedModuleLoads);
    } else {
      // To prevent mProcessedModuleLoads from exceeding |kMaxEvents|,
      // we process the first items in the mUnprocessedModuleLoads,
      // leaving the the remaining events for the next time.
      const size_t capacity = UntrustedModulesData::kMaxEvents >
                                      mProcessedModuleLoads.mEvents.length()
                                  ? (UntrustedModulesData::kMaxEvents -
                                     mProcessedModuleLoads.mEvents.length())
                                  : 0;
      auto moveRangeBegin = mUnprocessedModuleLoads.begin();
      auto moveRangeEnd = moveRangeBegin + capacity;
      Unused << loadsToProcess.moveAppend(moveRangeBegin, moveRangeEnd);
      mUnprocessedModuleLoads.erase(moveRangeBegin, moveRangeEnd);
    }
  }

  if (!mAllowProcessing || loadsToProcess.empty()) {
    return;
  }

  ModuleEvaluator modEval;
  MOZ_ASSERT(!!modEval);
  if (!modEval) {
    return;
  }

  Telemetry::BatchProcessedStackGenerator stackProcessor;
  ModulesMap modules;

  Maybe<double> maybeXulLoadDuration;
  Vector<Telemetry::ProcessedStack> processedStacks;
  Vector<ProcessedModuleLoadEvent> processedEvents;
  uint32_t sanitizationFailures = 0;
  uint32_t trustTestFailures = 0;

  for (glue::EnhancedModuleLoadInfo& entry : loadsToProcess) {
    if (!mAllowProcessing) {
      return;
    }

    RefPtr<ModuleRecord> module(GetOrAddModuleRecord(modules, modEval, entry));
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

template <typename ActorT>
static RefPtr<GetModulesTrustIpcPromise> SendGetModulesTrust(
    ActorT* aActor, ModulePaths&& aModPaths, bool aRunAtNormalPriority) {
  MOZ_ASSERT(NS_IsMainThread());
  return aActor->SendGetModulesTrust(std::move(aModPaths),
                                     aRunAtNormalPriority);
}

RefPtr<GetModulesTrustIpcPromise>
UntrustedModulesProcessor::SendGetModulesTrust(ModulePaths&& aModules,
                                               Priority aPriority) {
  MOZ_ASSERT(NS_IsMainThread());
  bool runNormal = aPriority == Priority::Default;

  switch (XRE_GetProcessType()) {
    case GeckoProcessType_Content: {
      return ::SendGetModulesTrust(dom::ContentChild::GetSingleton(),
                                   std::move(aModules), runNormal);
    }
    case GeckoProcessType_RDD: {
      return ::SendGetModulesTrust(RDDParent::GetSingleton(),
                                   std::move(aModules), runNormal);
    }
    default: {
      MOZ_ASSERT_UNREACHABLE("Unsupported process type");
      return GetModulesTrustIpcPromise::CreateAndReject(
          ipc::ResponseRejectReason::SendError, __func__);
    }
  }
}

/**
 * This method works very similarly to ProcessModuleLoadQueue, with the
 * exception that a sandboxed child process does not have sufficient rights to
 * be able to evaluate a module's trustworthiness. Instead, we accumulate the
 * resolved paths for all of the modules in this batch and send them to the
 * parent to determine trustworthiness.
 *
 * The parent process returns a list of untrusted modules and invokes
 * CompleteProcessing to handle the remainder of the process.
 *
 * By doing it this way, we minimize the amount of data that needs to be sent
 * over IPC and avoid the need to process every load's metadata only
 * to throw most of it away (since most modules will be trusted).
 */
RefPtr<UntrustedModulesProcessor::GetModulesTrustPromise>
UntrustedModulesProcessor::ProcessModuleLoadQueueChildProcess(
    UntrustedModulesProcessor::Priority aPriority) {
  AssertRunningOnLazyIdleThread();
  MOZ_ASSERT(!XRE_IsParentProcess());

  Vector<glue::EnhancedModuleLoadInfo> loadsToProcess;

  {  // Scope for lock
    MutexAutoLock lock(mUnprocessedMutex);
    CancelScheduledProcessing(lock);
    loadsToProcess.swap(mUnprocessedModuleLoads);
  }

  if (loadsToProcess.empty()) {
    // Nothing to process
    return GetModulesTrustPromise::CreateAndResolve(Nothing(), __func__);
  }

  if (!mAllowProcessing) {
    return GetModulesTrustPromise::CreateAndReject(
        NS_ERROR_ILLEGAL_DURING_SHUTDOWN, __func__);
  }

  nsTHashtable<nsStringCaseInsensitiveHashKey> moduleNtPathSet;

  // Build a set of modules to be processed by the parent
  for (glue::EnhancedModuleLoadInfo& entry : loadsToProcess) {
    if (!mAllowProcessing) {
      return GetModulesTrustPromise::CreateAndReject(
          NS_ERROR_ILLEGAL_DURING_SHUTDOWN, __func__);
    }

    moduleNtPathSet.PutEntry(entry.mNtLoadInfo.mSectionName.AsString());
  }

  if (!mAllowProcessing) {
    return GetModulesTrustPromise::CreateAndReject(
        NS_ERROR_ILLEGAL_DURING_SHUTDOWN, __func__);
  }

  MOZ_ASSERT(!moduleNtPathSet.IsEmpty());
  if (moduleNtPathSet.IsEmpty()) {
    // Nothing to process
    return GetModulesTrustPromise::CreateAndResolve(Nothing(), __func__);
  }

  ModulePaths moduleNtPaths(std::move(moduleNtPathSet));

  if (!mAllowProcessing) {
    return GetModulesTrustPromise::CreateAndReject(
        NS_ERROR_ILLEGAL_DURING_SHUTDOWN, __func__);
  }

  RefPtr<UntrustedModulesProcessor> self(this);

  auto invoker = [self = std::move(self),
                  moduleNtPaths = std::move(moduleNtPaths),
                  priority = aPriority]() mutable {
    return self->SendGetModulesTrust(std::move(moduleNtPaths), priority);
  };

  RefPtr<GetModulesTrustPromise::Private> p(
      new GetModulesTrustPromise::Private(__func__));

  if (!mAllowProcessing) {
    p->Reject(NS_ERROR_ILLEGAL_DURING_SHUTDOWN, __func__);
    return p;
  }

  // Send the IPC request via the main thread
  InvokeAsync(GetMainThreadSerialEventTarget(), __func__, std::move(invoker))
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [p, loads = std::move(loadsToProcess)](
              Maybe<ModulesMapResult>&& aResult) mutable {
            ModulesMapResultWithLoads result(std::move(aResult),
                                             std::move(loads));
            p->Resolve(Some(ModulesMapResultWithLoads(std::move(result))),
                       __func__);
          },
          [p](ipc::ResponseRejectReason aReason) {
            p->Reject(NS_ERROR_FAILURE, __func__);
          });

  return p;
}

void UntrustedModulesProcessor::CompleteProcessing(
    UntrustedModulesProcessor::ModulesMapResultWithLoads&& aModulesAndLoads) {
  MOZ_ASSERT(!XRE_IsParentProcess());
  AssertRunningOnLazyIdleThread();

  if (!mAllowProcessing) {
    return;
  }

  if (aModulesAndLoads.mModMapResult.isNothing()) {
    // No untrusted modules in this batch, nothing to save.
    return;
  }

  // This map only contains information about modules deemed to be untrusted,
  // plus xul.dll. Any module referenced by load requests that is *not* in the
  // map is deemed to be trusted.
  ModulesMap& modules = aModulesAndLoads.mModMapResult.ref().mModules;
  const uint32_t& trustTestFailures =
      aModulesAndLoads.mModMapResult.ref().mTrustTestFailures;
  LoadsVec& loads = aModulesAndLoads.mLoads;

  if (modules.IsEmpty() && !trustTestFailures) {
    // No data, nothing to save.
    return;
  }

  if (!mAllowProcessing) {
    return;
  }

  Telemetry::BatchProcessedStackGenerator stackProcessor;

  Maybe<double> maybeXulLoadDuration;
  Vector<Telemetry::ProcessedStack> processedStacks;
  Vector<ProcessedModuleLoadEvent> processedEvents;
  uint32_t sanitizationFailures = 0;

  if (!modules.IsEmpty()) {
    for (auto&& item : loads) {
      if (!mAllowProcessing) {
        return;
      }

      RefPtr<ModuleRecord> module(GetModuleRecord(modules, item));
      if (!module) {
        // If module is null then |item| is trusted
        continue;
      }

      if (!mAllowProcessing) {
        return;
      }

      glue::EnhancedModuleLoadInfo::BacktraceType backtrace =
          std::move(item.mNtLoadInfo.mBacktrace);
      ProcessedModuleLoadEvent event(std::move(item), std::move(module));

      if (!mAllowProcessing) {
        return;
      }

      if (!event) {
        // We don't have a sanitized DLL path, so we cannot include this event
        // for privacy reasons.
        ++sanitizationFailures;
        continue;
      }

      if (!mAllowProcessing) {
        return;
      }

      if (event.IsXULLoad()) {
        maybeXulLoadDuration = event.mLoadDurationMS;
        // We saved the XUL load duration, but it is still trusted, so we
        // continue.
        continue;
      }

      if (!mAllowProcessing) {
        return;
      }

      Telemetry::ProcessedStack processedStack =
          stackProcessor.GetStackAndModules(backtrace);

      Unused << processedStacks.emplaceBack(std::move(processedStack));
      Unused << processedEvents.emplaceBack(std::move(event));
    }
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

// The thread priority of this job should match the priority that the child
// process is running with, as specified by |aRunAtNormalPriority|.
RefPtr<ModulesTrustPromise> UntrustedModulesProcessor::GetModulesTrustInternal(
    ModulePaths&& aModPaths, bool aRunAtNormalPriority) {
  MOZ_ASSERT(XRE_IsParentProcess());
  AssertRunningOnLazyIdleThread();

  if (!mAllowProcessing) {
    return ModulesTrustPromise::CreateAndReject(
        NS_ERROR_ILLEGAL_DURING_SHUTDOWN, __func__);
  }

  if (aRunAtNormalPriority) {
    return GetModulesTrustInternal(std::move(aModPaths));
  }

  BackgroundPriorityRegion bgRgn;
  return GetModulesTrustInternal(std::move(aModPaths));
}

// For each module in |aModPaths|, evaluate its trustworthiness and only send
// ModuleRecords for untrusted modules back to the child process. We also save
// XUL's ModuleRecord so that the child process may report XUL's load time.
RefPtr<ModulesTrustPromise> UntrustedModulesProcessor::GetModulesTrustInternal(
    ModulePaths&& aModPaths) {
  MOZ_ASSERT(XRE_IsParentProcess());
  AssertRunningOnLazyIdleThread();

  ModulesMapResult result;

  ModulesMap& modMap = result.mModules;
  uint32_t& trustTestFailures = result.mTrustTestFailures;

  ModuleEvaluator modEval;
  MOZ_ASSERT(!!modEval);
  if (!modEval) {
    return ModulesTrustPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  // This map holds all modules regardless of trust status; we use this to
  // filter any duplicates from the input.
  ModulesMap modules;

  for (auto& resolvedNtPath :
       aModPaths.mModuleNtPaths.as<ModulePaths::VecType>()) {
    if (!mAllowProcessing) {
      return ModulesTrustPromise::CreateAndReject(
          NS_ERROR_ILLEGAL_DURING_SHUTDOWN, __func__);
    }

    MOZ_ASSERT(!resolvedNtPath.IsEmpty());
    if (resolvedNtPath.IsEmpty()) {
      continue;
    }

    RefPtr<ModuleRecord> module(
        GetOrAddModuleRecord(modules, modEval, resolvedNtPath));
    if (!module) {
      // We failed to obtain trust information.
      ++trustTestFailures;
      continue;
    }

    if (!mAllowProcessing) {
      return ModulesTrustPromise::CreateAndReject(
          NS_ERROR_ILLEGAL_DURING_SHUTDOWN, __func__);
    }

    if (module->IsTrusted() && !module->IsXUL()) {
      // If the module is trusted we exclude it from results, unless it's XUL.
      // (We save XUL so that the child process may report XUL's load time)
      continue;
    }

    if (!mAllowProcessing) {
      return ModulesTrustPromise::CreateAndReject(
          NS_ERROR_ILLEGAL_DURING_SHUTDOWN, __func__);
    }

    modMap.InsertOrUpdate(resolvedNtPath, std::move(module));
  }

  return ModulesTrustPromise::CreateAndResolve(std::move(result), __func__);
}

}  // namespace mozilla
