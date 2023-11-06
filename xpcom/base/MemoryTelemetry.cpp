/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MemoryTelemetry.h"
#include "nsMemoryReporterManager.h"

#include "mozilla/ClearOnShutdown.h"
#ifdef MOZ_PHC
#  include "mozilla/PHCManager.h"
#endif
#include "mozilla/Result.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Services.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/SimpleEnumerator.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsContentUtils.h"
#include "nsGlobalWindowOuter.h"
#include "nsIBrowserDOMWindow.h"
#include "nsIMemoryReporter.h"
#include "nsIWindowMediator.h"
#include "nsImportModule.h"
#include "nsITelemetry.h"
#include "nsNetCID.h"
#include "nsObserverService.h"
#include "nsReadableUtils.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "xpcpublic.h"

#include <cstdlib>

using namespace mozilla;

using mozilla::dom::AutoJSAPI;
using mozilla::dom::ContentParent;

// Do not gather data more than once a minute (ms)
static constexpr uint32_t kTelemetryInterval = 60 * 1000;

static constexpr const char* kTopicCycleCollectorBegin =
    "cycle-collector-begin";

namespace {

enum class PrevValue : uint32_t {
#ifdef XP_WIN
  LOW_MEMORY_EVENTS_VIRTUAL,
  LOW_MEMORY_EVENTS_COMMIT_SPACE,
  LOW_MEMORY_EVENTS_PHYSICAL,
#endif
#if defined(XP_LINUX) && !defined(ANDROID)
  PAGE_FAULTS_HARD,
#endif
  SIZE_,
};

}  // anonymous namespace

constexpr uint32_t kUninitialized = ~0;

static uint32_t gPrevValues[uint32_t(PrevValue::SIZE_)];

static uint32_t PrevValueIndex(Telemetry::HistogramID aId) {
  switch (aId) {
#ifdef XP_WIN
    case Telemetry::LOW_MEMORY_EVENTS_VIRTUAL:
      return uint32_t(PrevValue::LOW_MEMORY_EVENTS_VIRTUAL);
    case Telemetry::LOW_MEMORY_EVENTS_COMMIT_SPACE:
      return uint32_t(PrevValue::LOW_MEMORY_EVENTS_COMMIT_SPACE);
    case Telemetry::LOW_MEMORY_EVENTS_PHYSICAL:
      return uint32_t(PrevValue::LOW_MEMORY_EVENTS_PHYSICAL);
#endif
#if defined(XP_LINUX) && !defined(ANDROID)
    case Telemetry::PAGE_FAULTS_HARD:
      return uint32_t(PrevValue::PAGE_FAULTS_HARD);
#endif
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected histogram ID");
      return 0;
  }
}

NS_IMPL_ISUPPORTS(MemoryTelemetry, nsIObserver, nsISupportsWeakReference)

MemoryTelemetry::MemoryTelemetry()
    : mThreadPool(do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID)) {}

void MemoryTelemetry::Init() {
  for (auto& val : gPrevValues) {
    val = kUninitialized;
  }

  if (XRE_IsContentProcess()) {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    MOZ_RELEASE_ASSERT(obs);

    obs->AddObserver(this, "content-child-shutdown", true);
  }
}

/* static */ MemoryTelemetry& MemoryTelemetry::Get() {
  static RefPtr<MemoryTelemetry> sInstance;

  MOZ_ASSERT(NS_IsMainThread());

  if (!sInstance) {
    sInstance = new MemoryTelemetry();
    sInstance->Init();
    ClearOnShutdown(&sInstance);
  }
  return *sInstance;
}

nsresult MemoryTelemetry::DelayedInit() {
  if (Telemetry::CanRecordExtended()) {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    MOZ_RELEASE_ASSERT(obs);

    obs->AddObserver(this, kTopicCycleCollectorBegin, true);
  }

  GatherReports();

  return NS_OK;
}

nsresult MemoryTelemetry::Shutdown() {
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  MOZ_RELEASE_ASSERT(obs);

  obs->RemoveObserver(this, kTopicCycleCollectorBegin);

  return NS_OK;
}

static inline void HandleMemoryReport(Telemetry::HistogramID aId,
                                      int32_t aUnits, uint64_t aAmount,
                                      const nsCString& aKey = VoidCString()) {
  uint32_t val;
  switch (aUnits) {
    case nsIMemoryReporter::UNITS_BYTES:
      val = uint32_t(aAmount / 1024);
      break;

    case nsIMemoryReporter::UNITS_PERCENTAGE:
      // UNITS_PERCENTAGE amounts are 100x greater than their raw value.
      val = uint32_t(aAmount / 100);
      break;

    case nsIMemoryReporter::UNITS_COUNT:
      val = uint32_t(aAmount);
      break;

    case nsIMemoryReporter::UNITS_COUNT_CUMULATIVE: {
      // If the reporter gives us a cumulative count, we'll report the
      // difference in its value between now and our previous ping.

      uint32_t idx = PrevValueIndex(aId);
      uint32_t prev = gPrevValues[idx];
      gPrevValues[idx] = aAmount;

      if (prev == kUninitialized) {
        // If this is the first time we're reading this reporter, store its
        // current value but don't report it in the telemetry ping, so we
        // ignore the effect startup had on the reporter.
        return;
      }
      val = aAmount - prev;
      break;
    }

    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected aUnits value");
      return;
  }

  // Note: The reference equality check here should allow the compiler to
  // optimize this case out at compile time when we weren't given a key,
  // while IsEmpty() or IsVoid() most likely will not.
  if (&aKey == &VoidCString()) {
    Telemetry::Accumulate(aId, val);
  } else {
    Telemetry::Accumulate(aId, aKey, val);
  }
}

nsresult MemoryTelemetry::GatherReports(
    const std::function<void()>& aCompletionCallback) {
  auto cleanup = MakeScopeExit([&]() {
    if (aCompletionCallback) {
      aCompletionCallback();
    }
  });

  RefPtr<nsMemoryReporterManager> mgr = nsMemoryReporterManager::GetOrCreate();
  MOZ_DIAGNOSTIC_ASSERT(mgr);
  NS_ENSURE_TRUE(mgr, NS_ERROR_FAILURE);

#define RECORD(id, metric, units)                                       \
  do {                                                                  \
    int64_t amt;                                                        \
    nsresult rv = mgr->Get##metric(&amt);                               \
    if (NS_SUCCEEDED(rv)) {                                             \
      HandleMemoryReport(Telemetry::id, nsIMemoryReporter::units, amt); \
    } else if (rv != NS_ERROR_NOT_AVAILABLE) {                          \
      NS_WARNING("Failed to retrieve memory telemetry for " #metric);   \
    }                                                                   \
  } while (0)

  // GHOST_WINDOWS is opt-out as of Firefox 55
  RECORD(GHOST_WINDOWS, GhostWindows, UNITS_COUNT);

  // If we're running in the parent process, collect data from all processes for
  // the MEMORY_TOTAL histogram.
  if (XRE_IsParentProcess() && !mGatheringTotalMemory) {
    GatherTotalMemory();
  }

  if (!Telemetry::CanRecordReleaseData()) {
    return NS_OK;
  }

  // Get memory measurements from distinguished amount attributes.  We used
  // to measure "explicit" too, but it could cause hangs, and the data was
  // always really noisy anyway.  See bug 859657.
  //
  // test_TelemetrySession.js relies on some of these histograms being
  // here.  If you remove any of the following histograms from here, you'll
  // have to modify test_TelemetrySession.js:
  //
  //   * MEMORY_TOTAL,
  //   * MEMORY_JS_GC_HEAP, and
  //   * MEMORY_JS_COMPARTMENTS_SYSTEM.
  //
  // The distinguished amount attribute names don't match the telemetry id
  // names in some cases due to a combination of (a) historical reasons, and
  // (b) the fact that we can't change telemetry id names without breaking
  // data continuity.

  // Collect cheap or main-thread only metrics synchronously, on the main
  // thread.
  RECORD(MEMORY_JS_GC_HEAP, JSMainRuntimeGCHeap, UNITS_BYTES);
  RECORD(MEMORY_JS_COMPARTMENTS_SYSTEM, JSMainRuntimeCompartmentsSystem,
         UNITS_COUNT);
  RECORD(MEMORY_JS_COMPARTMENTS_USER, JSMainRuntimeCompartmentsUser,
         UNITS_COUNT);
  RECORD(MEMORY_JS_REALMS_SYSTEM, JSMainRuntimeRealmsSystem, UNITS_COUNT);
  RECORD(MEMORY_JS_REALMS_USER, JSMainRuntimeRealmsUser, UNITS_COUNT);
  RECORD(MEMORY_IMAGES_CONTENT_USED_UNCOMPRESSED, ImagesContentUsedUncompressed,
         UNITS_BYTES);
  RECORD(MEMORY_STORAGE_SQLITE, StorageSQLite, UNITS_BYTES);
#ifdef XP_WIN
  RECORD(LOW_MEMORY_EVENTS_PHYSICAL, LowMemoryEventsPhysical,
         UNITS_COUNT_CUMULATIVE);
#endif
#if defined(XP_LINUX) && !defined(ANDROID)
  RECORD(PAGE_FAULTS_HARD, PageFaultsHard, UNITS_COUNT_CUMULATIVE);
#endif

#ifdef HAVE_JEMALLOC_STATS
  jemalloc_stats_t stats;
  jemalloc_stats(&stats);
  HandleMemoryReport(Telemetry::MEMORY_HEAP_ALLOCATED,
                     nsIMemoryReporter::UNITS_BYTES, mgr->HeapAllocated(stats));
  HandleMemoryReport(Telemetry::MEMORY_HEAP_OVERHEAD_FRACTION,
                     nsIMemoryReporter::UNITS_PERCENTAGE,
                     mgr->HeapOverheadFraction(stats));
#endif

#ifdef MOZ_PHC
  ReportPHCTelemetry();
#endif

  RefPtr<Runnable> completionRunnable;
  if (aCompletionCallback) {
    completionRunnable = NS_NewRunnableFunction(__func__, aCompletionCallback);
  }

  // Collect expensive metrics that can be calculated off-main-thread
  // asynchronously, on a background thread.
  RefPtr<Runnable> runnable = NS_NewRunnableFunction(
      "MemoryTelemetry::GatherReports", [mgr, completionRunnable]() mutable {
        Telemetry::AutoTimer<Telemetry::MEMORY_COLLECTION_TIME> autoTimer;
        RECORD(MEMORY_VSIZE, Vsize, UNITS_BYTES);
#if !defined(HAVE_64BIT_BUILD) || !defined(XP_WIN)
        RECORD(MEMORY_VSIZE_MAX_CONTIGUOUS, VsizeMaxContiguous, UNITS_BYTES);
#endif
        RECORD(MEMORY_RESIDENT_FAST, ResidentFast, UNITS_BYTES);
        RECORD(MEMORY_RESIDENT_PEAK, ResidentPeak, UNITS_BYTES);
// Although we can measure unique memory on MacOS we choose not to, because
// doing so is too slow for telemetry.
#ifndef XP_MACOSX
        RECORD(MEMORY_UNIQUE, ResidentUnique, UNITS_BYTES);
#endif

        if (completionRunnable) {
          NS_DispatchToMainThread(completionRunnable.forget(),
                                  NS_DISPATCH_NORMAL);
        }
      });

#undef RECORD

  nsresult rv = mThreadPool->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
  if (!NS_WARN_IF(NS_FAILED(rv))) {
    cleanup.release();
  }

  return NS_OK;
}

namespace {
struct ChildProcessInfo {
  GeckoProcessType mType;
#if defined(XP_WIN)
  HANDLE mHandle;
#elif defined(XP_MACOSX)
  task_t mHandle;
#else
  pid_t mHandle;
#endif
};
}  // namespace

/**
 * Runs a task on the background thread pool to fetch the memory usage of all
 * processes.
 */
void MemoryTelemetry::GatherTotalMemory() {
  MOZ_ASSERT(!mGatheringTotalMemory);
  mGatheringTotalMemory = true;

  nsTArray<ChildProcessInfo> infos;
  mozilla::ipc::GeckoChildProcessHost::GetAll(
      [&](mozilla::ipc::GeckoChildProcessHost* aGeckoProcess) {
        if (!aGeckoProcess->GetChildProcessHandle()) {
          return;
        }

        ChildProcessInfo info{};
        info.mType = aGeckoProcess->GetProcessType();

        // NOTE: For now we ignore non-content processes here for compatibility
        // with the existing probe. We may want to introduce a new probe in the
        // future which also collects data for non-content processes.
        if (info.mType != GeckoProcessType_Content) {
          return;
        }

#if defined(XP_WIN)
        if (!::DuplicateHandle(::GetCurrentProcess(),
                               aGeckoProcess->GetChildProcessHandle(),
                               ::GetCurrentProcess(), &info.mHandle, 0, false,
                               DUPLICATE_SAME_ACCESS)) {
          return;
        }
#elif defined(XP_MACOSX)
        info.mHandle = aGeckoProcess->GetChildTask();
        if (mach_port_mod_refs(mach_task_self(), info.mHandle,
                               MACH_PORT_RIGHT_SEND, 1) != KERN_SUCCESS) {
          return;
        }
#else
        info.mHandle = aGeckoProcess->GetChildProcessId();
#endif

        infos.AppendElement(info);
      });

  mThreadPool->Dispatch(NS_NewRunnableFunction(
      "MemoryTelemetry::GatherTotalMemory", [infos = std::move(infos)] {
        RefPtr<nsMemoryReporterManager> mgr =
            nsMemoryReporterManager::GetOrCreate();
        MOZ_RELEASE_ASSERT(mgr);

        int64_t totalMemory = mgr->ResidentFast();
        nsTArray<int64_t> childSizes(infos.Length());

        // Use our handle for the remote process to collect resident unique set
        // size information for that process.
        for (const auto& info : infos) {
#ifdef XP_MACOSX
          int64_t memory =
              nsMemoryReporterManager::PhysicalFootprint(info.mHandle);
#else
	  int64_t memory =
	      nsMemoryReporterManager::ResidentUnique(info.mHandle);
#endif
          if (memory > 0) {
            childSizes.AppendElement(memory);
            totalMemory += memory;
          }

#if defined(XP_WIN)
          ::CloseHandle(info.mHandle);
#elif defined(XP_MACOSX)
          mach_port_deallocate(mach_task_self(), info.mHandle);
#endif
        }

        NS_DispatchToMainThread(NS_NewRunnableFunction(
            "MemoryTelemetry::FinishGatheringTotalMemory",
            [totalMemory, childSizes = std::move(childSizes)] {
              MemoryTelemetry::Get().FinishGatheringTotalMemory(totalMemory,
                                                                childSizes);
            }));
      }));
}

nsresult MemoryTelemetry::FinishGatheringTotalMemory(
    int64_t aTotalMemory, const nsTArray<int64_t>& aChildSizes) {
  mGatheringTotalMemory = false;

  // Total memory usage can be difficult to measure both accurately and fast
  // enough for telemetry (iterating memory maps can jank whole processes on
  // MacOS).  Therefore this shouldn't be relied on as an absolute measurement
  // especially on MacOS where it double-counts shared memory.  For a more
  // detailed explaination see:
  // https://groups.google.com/a/mozilla.org/g/dev-platform/c/WGNOtjHdsdA
  HandleMemoryReport(Telemetry::MEMORY_TOTAL, nsIMemoryReporter::UNITS_BYTES,
                     aTotalMemory);

  if (aChildSizes.Length() > 1) {
    int32_t tabsCount;
    MOZ_TRY_VAR(tabsCount, GetOpenTabsCount());

    nsCString key;
    if (tabsCount <= 10) {
      key = "0 - 10 tabs";
    } else if (tabsCount <= 500) {
      key = "11 - 500 tabs";
    } else {
      key = "more tabs";
    }

    // Mean of the USS of all the content processes.
    int64_t mean = 0;
    for (auto size : aChildSizes) {
      mean += size;
    }
    mean /= aChildSizes.Length();

    // For some users, for unknown reasons (though most likely because they're
    // in a sandbox without procfs mounted), we wind up with 0 here, which
    // triggers a floating point exception if we try to calculate values using
    // it.
    if (!mean) {
      return NS_ERROR_UNEXPECTED;
    }

    // Absolute error of USS for each content process, normalized by the mean
    // (*100 to get it in percentage). 20% means for a content process that it
    // is using 20% more or 20% less than the mean.
    for (auto size : aChildSizes) {
      int64_t diff = llabs(size - mean) * 100 / mean;

      HandleMemoryReport(Telemetry::MEMORY_DISTRIBUTION_AMONG_CONTENT,
                         nsIMemoryReporter::UNITS_COUNT, diff, key);
    }
  }

  // This notification is for testing only.
  if (nsCOMPtr<nsIObserverService> obs = services::GetObserverService()) {
    obs->NotifyObservers(nullptr, "gather-memory-telemetry-finished", nullptr);
  }

  return NS_OK;
}

/* static */ Result<uint32_t, nsresult> MemoryTelemetry::GetOpenTabsCount() {
  nsresult rv;

  nsCOMPtr<nsIWindowMediator> windowMediator(
      do_GetService(NS_WINDOWMEDIATOR_CONTRACTID, &rv));
  MOZ_TRY(rv);

  nsCOMPtr<nsISimpleEnumerator> enumerator;
  MOZ_TRY(windowMediator->GetEnumerator(u"navigator:browser",
                                        getter_AddRefs(enumerator)));

  uint32_t total = 0;
  for (const auto& window : SimpleEnumerator<nsPIDOMWindowOuter>(enumerator)) {
    nsCOMPtr<nsIBrowserDOMWindow> browserWin =
        nsGlobalWindowOuter::Cast(window)->GetBrowserDOMWindow();

    NS_ENSURE_TRUE(browserWin, Err(NS_ERROR_UNEXPECTED));

    uint32_t tabCount;
    MOZ_TRY(browserWin->GetTabCount(&tabCount));
    total += tabCount;
  }

  return total;
}

nsresult MemoryTelemetry::Observe(nsISupports* aSubject, const char* aTopic,
                                  const char16_t* aData) {
  if (strcmp(aTopic, kTopicCycleCollectorBegin) == 0) {
    auto now = TimeStamp::Now();
    if (!mLastPoll.IsNull() &&
        (now - mLastPoll).ToMilliseconds() < kTelemetryInterval) {
      return NS_OK;
    }

    mLastPoll = now;

    NS_DispatchToCurrentThreadQueue(
        NewRunnableMethod<std::function<void()>>(
            "MemoryTelemetry::GatherReports", this,
            &MemoryTelemetry::GatherReports, nullptr),
        EventQueuePriority::Idle);
  } else if (strcmp(aTopic, "content-child-shutdown") == 0) {
    if (nsCOMPtr<nsITelemetry> telemetry =
            do_GetService("@mozilla.org/base/telemetry;1")) {
      telemetry->FlushBatchedChildTelemetry();
    }
  }
  return NS_OK;
}
