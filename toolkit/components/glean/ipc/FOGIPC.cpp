/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FOGIPC.h"

#include <limits>
#include "mozilla/glean/fog_ffi_generated.h"
#include "mozilla/glean/GleanMetrics.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/gfx/GPUChild.h"
#include "mozilla/gfx/GPUParent.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/Hal.h"
#include "mozilla/MozPromise.h"
#include "mozilla/net/SocketProcessChild.h"
#include "mozilla/net/SocketProcessParent.h"
#include "mozilla/ProcInfo.h"
#include "mozilla/RDDChild.h"
#include "mozilla/RDDParent.h"
#include "mozilla/RDDProcessManager.h"
#include "mozilla/ipc/UtilityProcessChild.h"
#include "mozilla/ipc/UtilityProcessManager.h"
#include "mozilla/ipc/UtilityProcessParent.h"
#include "mozilla/ipc/UtilityProcessSandboxing.h"
#include "mozilla/Unused.h"
#include "GMPPlatform.h"
#include "GMPServiceParent.h"
#include "nsIClassifiedChannel.h"
#include "nsIXULRuntime.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

using mozilla::dom::ContentParent;
using mozilla::gfx::GPUChild;
using mozilla::gfx::GPUProcessManager;
using mozilla::ipc::ByteBuf;
using mozilla::ipc::UtilityProcessChild;
using mozilla::ipc::UtilityProcessManager;
using mozilla::ipc::UtilityProcessParent;
using FlushFOGDataPromise = mozilla::dom::ContentParent::FlushFOGDataPromise;

namespace geckoprofiler::markers {

using namespace mozilla;

struct ProcessingTimeMarker {
  static constexpr Span<const char> MarkerTypeName() {
    return MakeStringSpan("ProcessingTime");
  }
  static void StreamJSONMarkerData(baseprofiler::SpliceableJSONWriter& aWriter,
                                   int64_t aDiffMs,
                                   const ProfilerString8View& aType,
                                   const ProfilerString8View& aTrackerType) {
    aWriter.IntProperty("time", aDiffMs);
    aWriter.StringProperty("label", aType);
    if (aTrackerType.Length() > 0) {
      aWriter.StringProperty("tracker", aTrackerType);
    }
  }
  static MarkerSchema MarkerTypeDisplay() {
    using MS = MarkerSchema;
    MS schema{MS::Location::MarkerChart, MS::Location::MarkerTable};
    schema.AddKeyLabelFormat("time", "Recorded Time", MS::Format::Milliseconds);
    schema.AddKeyLabelFormat("tracker", "Tracker Type", MS::Format::String);
    schema.SetTooltipLabel("{marker.name} - {marker.data.label}");
    schema.SetTableLabel(
        "{marker.name} - {marker.data.label}: {marker.data.time}");
    return schema;
  }
};

}  // namespace geckoprofiler::markers

namespace mozilla::glean {

#ifdef NIGHTLY_BUILD
// The 2 following static global variables are set within RecordPowerMetrics
// so that RecordThreadCpuUse can avoid computing the Glean process type
// (matching a label of per_process_type_labels).
// This is useful because RecordThreadCpuUse will either be called in a loop
// for every thread (recomputing the process type for every thread would be
// expensive), or will be called off main thread when a thread is unregisters
// itself (some APIs needed to compute the process type might not be available
// off main thread).
// It is fine to call RecordThreadCpuUse during startup before the first
// RecordPowerMetrics call. In that case the parent process will be recorded
// as inactive, and other processes will be ignored (content processes start
// in the 'prealloc' type for which we don't record per-thread CPU use data).
using LabeledCounterMetric = const impl::Labeled<impl::CounterMetric>;
static Atomic<LabeledCounterMetric*> gCpuTimePerThreadMetric(nullptr);
static Atomic<LabeledCounterMetric*> gWakeupsPerThreadMetric(nullptr);

// These 2 macros are only meant to reduce code duplication, there is no
// requirement of the 2 variables being set atomically as a single value.
#  define SET_PER_THREAD_CPU_METRICS(aProcessType)                    \
    gCpuTimePerThreadMetric = &power_cpu_ms_per_thread::aProcessType; \
    gWakeupsPerThreadMetric = &power_wakeups_per_thread::aProcessType;

#  define RESET_PER_THREAD_CPU_METRICS() \
    gCpuTimePerThreadMetric = nullptr;   \
    gWakeupsPerThreadMetric = nullptr;

void RecordThreadCpuUse(const nsACString& aThreadName, uint64_t aCpuTimeMs,
                        uint64_t aWakeCount) {
  // Copy the values of the atomics to local variables so that we don't have to
  // worry about other threads changing them during the execution of this
  // function.
  LabeledCounterMetric* cpuTimeMetric = gCpuTimePerThreadMetric;
  LabeledCounterMetric* wakeupsMetric = gWakeupsPerThreadMetric;

  if (!cpuTimeMetric || !wakeupsMetric) {
    if (XRE_IsParentProcess()) {
      // The metrics can be null for the parent process during startup,
      // and we want to record during that time.
      SET_PER_THREAD_CPU_METRICS(parent_inactive);
      cpuTimeMetric = gCpuTimePerThreadMetric;
      wakeupsMetric = gWakeupsPerThreadMetric;
      if (!cpuTimeMetric || !wakeupsMetric) {
        return;
      }
    } else {
      // We are not interested in per-thread CPU use data for the current
      // process type.
      return;
    }
  }

  nsAutoCString threadName(aThreadName);
  for (size_t i = 0; i < threadName.Length(); ++i) {
    const char c = threadName.CharAt(i);

    // Valid characters.
    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || c == '-' ||
        c == '_') {
      continue;
    }

    // Should only use lower case characters
    if (c >= 'A' && c <= 'Z') {
      threadName.SetCharAt(c + ('a' - 'A'), i);
      continue;
    }

    // Replace everything else with _
    threadName.SetCharAt('_', i);
  }

  if (aCpuTimeMs != 0 &&
      MOZ_LIKELY(aCpuTimeMs < std::numeric_limits<int32_t>::max())) {
    cpuTimeMetric->Get(threadName).Add(int32_t(aCpuTimeMs));
  }

  if (aWakeCount != 0 &&
      MOZ_LIKELY(aWakeCount < std::numeric_limits<int32_t>::max())) {
    wakeupsMetric->Get(threadName).Add(int32_t(aWakeCount));
  }
}
#else  // ifdef NIGHTLY_BUILD
#  define SET_PER_THREAD_CPU_METRICS(aProcessType)
#  define RESET_PER_THREAD_CPU_METRICS()
#endif

void GetTrackerType(nsAutoCString& aTrackerType) {
  using namespace mozilla::dom;
  uint32_t trackingFlags =
      (nsIClassifiedChannel::CLASSIFIED_CRYPTOMINING |
       nsIClassifiedChannel::CLASSIFIED_FINGERPRINTING |
       nsIClassifiedChannel::CLASSIFIED_TRACKING |
       nsIClassifiedChannel::CLASSIFIED_TRACKING_AD |
       nsIClassifiedChannel::CLASSIFIED_TRACKING_ANALYTICS |
       nsIClassifiedChannel::CLASSIFIED_TRACKING_SOCIAL);
  AutoTArray<RefPtr<BrowsingContextGroup>, 5> bcGroups;
  BrowsingContextGroup::GetAllGroups(bcGroups);
  for (auto& bcGroup : bcGroups) {
    AutoTArray<DocGroup*, 5> docGroups;
    bcGroup->GetDocGroups(docGroups);
    for (auto* docGroup : docGroups) {
      for (Document* doc : *docGroup) {
        nsCOMPtr<nsIClassifiedChannel> classifiedChannel =
            do_QueryInterface(doc->GetChannel());
        if (classifiedChannel) {
          uint32_t classificationFlags =
              classifiedChannel->GetThirdPartyClassificationFlags();
          trackingFlags &= classificationFlags;
          if (!trackingFlags) {
            return;
          }
        }
      }
    }
  }

  // The if-elseif-else chain works because the tracker types listed here are
  // currently mutually exclusive and should be maintained that way by policy.
  if (trackingFlags == nsIClassifiedChannel::CLASSIFIED_TRACKING_AD) {
    aTrackerType = "ad";
  } else if (trackingFlags ==
             nsIClassifiedChannel::CLASSIFIED_TRACKING_ANALYTICS) {
    aTrackerType = "analytics";
  } else if (trackingFlags ==
             nsIClassifiedChannel::CLASSIFIED_TRACKING_SOCIAL) {
    aTrackerType = "social";
  } else if (trackingFlags == nsIClassifiedChannel::CLASSIFIED_CRYPTOMINING) {
    aTrackerType = "cryptomining";
  } else if (trackingFlags == nsIClassifiedChannel::CLASSIFIED_FINGERPRINTING) {
    aTrackerType = "fingerprinting";
  } else if (trackingFlags == nsIClassifiedChannel::CLASSIFIED_TRACKING) {
    // CLASSIFIED_TRACKING means we were not able to identify the type of
    // classification.
    aTrackerType = "unknown";
  }
}

void RecordPowerMetrics() {
  static uint64_t previousCpuTime = 0, previousGpuTime = 0;

  uint64_t cpuTime, newCpuTime = 0;
  if (NS_SUCCEEDED(GetCpuTimeSinceProcessStartInMs(&cpuTime)) &&
      cpuTime > previousCpuTime) {
    newCpuTime = cpuTime - previousCpuTime;
  }

  uint64_t gpuTime, newGpuTime = 0;
  // Avoid loading gdi32.dll for the Socket process where the GPU is never used.
  if (!XRE_IsSocketProcess() &&
      NS_SUCCEEDED(GetGpuTimeSinceProcessStartInMs(&gpuTime)) &&
      gpuTime > previousGpuTime) {
    newGpuTime = gpuTime - previousGpuTime;
  }

  if (!newCpuTime && !newGpuTime) {
    // Nothing to record.
    return;
  }

  // Compute the process type string.
  nsAutoCString type(XRE_GetProcessTypeString());
  nsAutoCString trackerType;
  if (XRE_IsContentProcess()) {
    auto* cc = dom::ContentChild::GetSingleton();
    if (cc) {
      type.Assign(dom::RemoteTypePrefix(cc->GetRemoteType()));
      if (StringBeginsWith(type, WEB_REMOTE_TYPE)) {
        type.AssignLiteral("web");
        switch (cc->GetProcessPriority()) {
          case hal::PROCESS_PRIORITY_BACKGROUND:
            type.AppendLiteral(".background");
            SET_PER_THREAD_CPU_METRICS(content_background);
            break;
          case hal::PROCESS_PRIORITY_FOREGROUND:
            type.AppendLiteral(".foreground");
            SET_PER_THREAD_CPU_METRICS(content_foreground);
            break;
          case hal::PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE:
            type.AppendLiteral(".background-perceivable");
            RESET_PER_THREAD_CPU_METRICS();
            break;
          default:
            RESET_PER_THREAD_CPU_METRICS();
            break;
        }
      }
      GetTrackerType(trackerType);
    } else {
      RESET_PER_THREAD_CPU_METRICS();
    }
  } else if (XRE_IsParentProcess()) {
    if (nsContentUtils::GetUserIsInteracting()) {
      type.AssignLiteral("parent.active");
      SET_PER_THREAD_CPU_METRICS(parent_active);
    } else {
      type.AssignLiteral("parent.inactive");
      SET_PER_THREAD_CPU_METRICS(parent_inactive);
    }
    hal::WakeLockInformation info;
    GetWakeLockInfo(u"video-playing"_ns, &info);
    if (info.numLocks() != 0 && info.numHidden() < info.numLocks()) {
      type.AppendLiteral(".playing-video");
    } else {
      GetWakeLockInfo(u"audio-playing"_ns, &info);
      if (info.numLocks()) {
        type.AppendLiteral(".playing-audio");
      }
    }
  } else if (XRE_IsGPUProcess()) {
    SET_PER_THREAD_CPU_METRICS(gpu_process);
  } else {
    RESET_PER_THREAD_CPU_METRICS();
  }

  if (newCpuTime) {
    // The counters are reset at least once a day. Assuming all cores are used
    // continuously, an int32 can hold the data for 24.85 cores.
    // This should be fine for now, but may overflow in the future.
    // Bug 1751277 tracks a newer, bigger counter.
    int32_t nNewCpuTime = int32_t(newCpuTime);
    if (newCpuTime < std::numeric_limits<int32_t>::max()) {
      power::total_cpu_time_ms.Add(nNewCpuTime);
      power::cpu_time_per_process_type_ms.Get(type).Add(nNewCpuTime);
      if (!trackerType.IsEmpty()) {
        power::cpu_time_per_tracker_type_ms.Get(trackerType).Add(nNewCpuTime);
      }
    } else {
      power::cpu_time_bogus_values.Add(1);
    }
    PROFILER_MARKER("Process CPU Time", OTHER, {}, ProcessingTimeMarker,
                    nNewCpuTime, type, trackerType);
    previousCpuTime += newCpuTime;
  }

  if (newGpuTime) {
    int32_t nNewGpuTime = int32_t(newGpuTime);
    if (newGpuTime < std::numeric_limits<int32_t>::max()) {
      power::total_gpu_time_ms.Add(nNewGpuTime);
      power::gpu_time_per_process_type_ms.Get(type).Add(nNewGpuTime);
    } else {
      power::gpu_time_bogus_values.Add(1);
    }
    PROFILER_MARKER("Process GPU Time", OTHER, {}, ProcessingTimeMarker,
                    nNewGpuTime, type, trackerType);
    previousGpuTime += newGpuTime;
  }

  profiler_record_wakeup_count(type);
}

/**
 * Flush your data ASAP, either because the parent process is asking you to
 * or because the process is about to shutdown.
 *
 * @param aResolver - The function you need to call with the bincoded,
 *                    serialized payload that the Rust impl hands you.
 */
void FlushFOGData(std::function<void(ipc::ByteBuf&&)>&& aResolver) {
  // Record power metrics right before data is sent to the parent.
  RecordPowerMetrics();

  ByteBuf buf;
  uint32_t ipcBufferSize = impl::fog_serialize_ipc_buf();
  bool ok = buf.Allocate(ipcBufferSize);
  if (!ok) {
    return;
  }
  uint32_t writtenLen = impl::fog_give_ipc_buf(buf.mData, buf.mLen);
  if (writtenLen != ipcBufferSize) {
    return;
  }
  aResolver(std::move(buf));
}

/**
 * Called by FOG on the parent process when it wants to flush all its
 * children's data.
 * @param aResolver - The function that'll be called with the results.
 */
void FlushAllChildData(
    std::function<void(nsTArray<ipc::ByteBuf>&&)>&& aResolver) {
  auto timerId = fog_ipc::flush_durations.Start();

  nsTArray<ContentParent*> parents;
  ContentParent::GetAll(parents);
  nsTArray<RefPtr<FlushFOGDataPromise>> promises;
  for (auto* parent : parents) {
    promises.EmplaceBack(parent->SendFlushFOGData());
  }

  if (GPUProcessManager* gpuManager = GPUProcessManager::Get()) {
    if (GPUChild* gpuChild = gpuManager->GetGPUChild()) {
      promises.EmplaceBack(gpuChild->SendFlushFOGData());
    }
  }

  if (RDDProcessManager* rddManager = RDDProcessManager::Get()) {
    if (RDDChild* rddChild = rddManager->GetRDDChild()) {
      promises.EmplaceBack(rddChild->SendFlushFOGData());
    }
  }

  if (net::SocketProcessParent* socketParent =
          net::SocketProcessParent::GetSingleton()) {
    promises.EmplaceBack(socketParent->SendFlushFOGData());
  }

  {
    RefPtr<mozilla::gmp::GeckoMediaPluginServiceParent> gmps(
        mozilla::gmp::GeckoMediaPluginServiceParent::GetSingleton());
    // There can be multiple Gecko Media Plugin processes, but iterating
    // through them requires locking a mutex and the IPCs need to be sent
    // from a different thread, so it's better to let the
    // GeckoMediaPluginServiceParent code do it for us.
    gmps->SendFlushFOGData(promises);
  }

  if (RefPtr<UtilityProcessManager> utilityManager =
          UtilityProcessManager::GetIfExists()) {
    for (RefPtr<UtilityProcessParent>& parent :
         utilityManager->GetAllProcessesProcessParent()) {
      promises.EmplaceBack(parent->SendFlushFOGData());
    }
  }

  if (promises.Length() == 0) {
    // No child processes at the moment. Resolve synchronously.
    fog_ipc::flush_durations.Cancel(std::move(timerId));
    nsTArray<ipc::ByteBuf> results;
    aResolver(std::move(results));
    return;
  }

  // If fog.ipc.flush_failures ever gets too high:
  // TODO: Don't throw away resolved data if some of the promises reject.
  // (not sure how, but it'll mean not using ::All... maybe a custom copy of
  // AllPromiseHolder? Might be impossible outside MozPromise.h)
  FlushFOGDataPromise::All(GetCurrentSerialEventTarget(), promises)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [aResolver = std::move(aResolver), timerId](
                 FlushFOGDataPromise::AllPromiseType::ResolveOrRejectValue&&
                     aValue) {
               fog_ipc::flush_durations.StopAndAccumulate(std::move(timerId));
               if (aValue.IsResolve()) {
                 aResolver(std::move(aValue.ResolveValue()));
               } else {
                 fog_ipc::flush_failures.Add(1);
                 nsTArray<ipc::ByteBuf> results;
                 aResolver(std::move(results));
               }
             });
}

/**
 * A child process has sent you this buf as a treat.
 * @param buf - a bincoded serialized payload that the Rust impl understands.
 */
void FOGData(ipc::ByteBuf&& buf) {
  fog_ipc::buffer_sizes.Accumulate(buf.mLen);
  impl::fog_use_ipc_buf(buf.mData, buf.mLen);
}

/**
 * Called by FOG on a child process when it wants to send a buf to the parent.
 * @param buf - a bincoded serialized payload that the Rust impl understands.
 */
void SendFOGData(ipc::ByteBuf&& buf) {
  switch (XRE_GetProcessType()) {
    case GeckoProcessType_Content:
      mozilla::dom::ContentChild::GetSingleton()->SendFOGData(std::move(buf));
      break;
    case GeckoProcessType_GMPlugin: {
      mozilla::gmp::SendFOGData(std::move(buf));
    } break;
    case GeckoProcessType_GPU:
      Unused << mozilla::gfx::GPUParent::GetSingleton()->SendFOGData(
          std::move(buf));
      break;
    case GeckoProcessType_RDD:
      Unused << mozilla::RDDParent::GetSingleton()->SendFOGData(std::move(buf));
      break;
    case GeckoProcessType_Socket:
      Unused << net::SocketProcessChild::GetSingleton()->SendFOGData(
          std::move(buf));
      break;
    case GeckoProcessType_Utility:
      Unused << ipc::UtilityProcessChild::GetSingleton()->SendFOGData(
          std::move(buf));
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unsuppored process type");
  }
}

/**
 * Called on the parent process to ask all child processes for data,
 * sending it all down into Rust to be used.
 */
RefPtr<GenericPromise> FlushAndUseFOGData() {
  // Record power metrics on the parent before sending requests to child
  // processes.
  RecordPowerMetrics();

  RefPtr<GenericPromise::Private> ret = new GenericPromise::Private(__func__);
  std::function<void(nsTArray<ByteBuf>&&)> resolver =
      [ret](nsTArray<ByteBuf>&& bufs) {
        for (ByteBuf& buf : bufs) {
          FOGData(std::move(buf));
        }
        ret->Resolve(true, __func__);
      };
  FlushAllChildData(std::move(resolver));
  return ret;
}

void TestTriggerMetrics(uint32_t aProcessType,
                        const RefPtr<dom::Promise>& promise) {
  switch (aProcessType) {
    case nsIXULRuntime::PROCESS_TYPE_GMPLUGIN: {
      RefPtr<mozilla::gmp::GeckoMediaPluginServiceParent> gmps(
          mozilla::gmp::GeckoMediaPluginServiceParent::GetSingleton());
      gmps->TestTriggerMetrics()->Then(
          GetCurrentSerialEventTarget(), __func__,
          [promise]() { promise->MaybeResolveWithUndefined(); },
          [promise]() { promise->MaybeRejectWithUndefined(); });
    } break;
    case nsIXULRuntime::PROCESS_TYPE_GPU:
      gfx::GPUProcessManager::Get()->TestTriggerMetrics()->Then(
          GetCurrentSerialEventTarget(), __func__,
          [promise]() { promise->MaybeResolveWithUndefined(); },
          [promise]() { promise->MaybeRejectWithUndefined(); });
      break;
    case nsIXULRuntime::PROCESS_TYPE_RDD:
      RDDProcessManager::Get()->TestTriggerMetrics()->Then(
          GetCurrentSerialEventTarget(), __func__,
          [promise]() { promise->MaybeResolveWithUndefined(); },
          [promise]() { promise->MaybeRejectWithUndefined(); });
      break;
    case nsIXULRuntime::PROCESS_TYPE_SOCKET:
      Unused << net::SocketProcessParent::GetSingleton()
                    ->SendTestTriggerMetrics()
                    ->Then(
                        GetCurrentSerialEventTarget(), __func__,
                        [promise]() { promise->MaybeResolveWithUndefined(); },
                        [promise]() { promise->MaybeRejectWithUndefined(); });
      break;
    case nsIXULRuntime::PROCESS_TYPE_UTILITY:
      Unused << ipc::UtilityProcessManager::GetSingleton()
                    ->GetProcessParent(ipc::SandboxingKind::GENERIC_UTILITY)
                    ->SendTestTriggerMetrics()
                    ->Then(
                        GetCurrentSerialEventTarget(), __func__,
                        [promise]() { promise->MaybeResolveWithUndefined(); },
                        [promise]() { promise->MaybeRejectWithUndefined(); });
      break;
    default:
      promise->MaybeRejectWithUndefined();
      break;
  }
}

}  // namespace mozilla::glean
