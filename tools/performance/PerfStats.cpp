/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerfStats.h"
#include "nsAppRunner.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentProcessManager.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/gfx/GPUChild.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/JSONWriter.h"

using namespace mozilla::dom;
using namespace mozilla::gfx;

namespace mozilla {

#define METRIC_NAME(metric) #metric,
static const char* const sMetricNames[] = {
    FOR_EACH_PERFSTATS_METRIC(METRIC_NAME)
#undef METRIC_NAME
        "Invalid"};

PerfStats::MetricMask PerfStats::sCollectionMask = 0;
StaticMutex PerfStats::sMutex;
StaticAutoPtr<PerfStats> PerfStats::sSingleton;

void PerfStats::SetCollectionMask(MetricMask aMask) {
  sCollectionMask = aMask;
  GetSingleton()->ResetCollection();

  if (!XRE_IsParentProcess()) {
    return;
  }

  GPUProcessManager* gpuManager = GPUProcessManager::Get();
  GPUChild* gpuChild = nullptr;

  if (gpuManager) {
    gpuChild = gpuManager->GetGPUChild();
    if (gpuChild) {
      gpuChild->SendUpdatePerfStatsCollectionMask(aMask);
    }
  }

  nsTArray<ContentParent*> contentParents;
  ContentParent::GetAll(contentParents);

  for (ContentParent* parent : contentParents) {
    Unused << parent->SendUpdatePerfStatsCollectionMask(aMask);
  }
}

PerfStats::MetricMask PerfStats::GetCollectionMask() { return sCollectionMask; }

PerfStats* PerfStats::GetSingleton() {
  if (!sSingleton) {
    sSingleton = new PerfStats;
  }

  return sSingleton.get();
}

void PerfStats::RecordMeasurementStartInternal(Metric aMetric) {
  StaticMutexAutoLock lock(sMutex);

  GetSingleton()->mRecordedStarts[static_cast<size_t>(aMetric)] =
      TimeStamp::Now();
}

void PerfStats::RecordMeasurementEndInternal(Metric aMetric) {
  StaticMutexAutoLock lock(sMutex);

  MOZ_ASSERT(sSingleton);

  sSingleton->mRecordedTimes[static_cast<size_t>(aMetric)] +=
      (TimeStamp::Now() -
       sSingleton->mRecordedStarts[static_cast<size_t>(aMetric)])
          .ToMilliseconds();
  sSingleton->mRecordedCounts[static_cast<size_t>(aMetric)]++;
}

void PerfStats::RecordMeasurementInternal(Metric aMetric,
                                          TimeDuration aDuration) {
  StaticMutexAutoLock lock(sMutex);

  MOZ_ASSERT(sSingleton);

  sSingleton->mRecordedTimes[static_cast<size_t>(aMetric)] +=
      aDuration.ToMilliseconds();
  sSingleton->mRecordedCounts[static_cast<size_t>(aMetric)]++;
}

void PerfStats::RecordMeasurementCounterInternal(Metric aMetric,
                                                 uint64_t aIncrementAmount) {
  StaticMutexAutoLock lock(sMutex);

  MOZ_ASSERT(sSingleton);

  sSingleton->mRecordedTimes[static_cast<size_t>(aMetric)] +=
      double(aIncrementAmount);
  sSingleton->mRecordedCounts[static_cast<size_t>(aMetric)]++;
}

struct StringWriteFunc : public JSONWriteFunc {
  nsCString& mString;

  explicit StringWriteFunc(nsCString& aString) : mString(aString) {}
  virtual void Write(const Span<const char>& aStr) override {
    mString.Append(aStr);
  }
};

void AppendJSONStringAsProperty(nsCString& aDest, const char* aPropertyName,
                                const nsCString& aJSON) {
  // We need to manually append into the string here, since JSONWriter has no
  // way to allow us to write an existing JSON object into a property.
  aDest.Append(",\n\"");
  aDest.Append(aPropertyName);
  aDest.Append("\": ");
  aDest.Append(aJSON);
}

static void WriteContentParent(nsCString& aRawString, JSONWriter& aWriter,
                               const nsCString& aString,
                               ContentParent* aParent) {
  aWriter.StringProperty("type", "content");
  aWriter.IntProperty("id", aParent->ChildID());
  const ManagedContainer<PBrowserParent>& browsers =
      aParent->ManagedPBrowserParent();

  aWriter.StartArrayProperty("urls");
  for (const auto& key : browsers) {
    // This only reports -current- URLs, not ones that may have been here in
    // the past, this is unfortunate especially for processes which are dying
    // and that have no more active URLs.
    RefPtr<BrowserParent> parent = BrowserParent::GetFrom(key);

    CanonicalBrowsingContext* ctx = parent->GetBrowsingContext();
    if (!ctx) {
      continue;
    }

    WindowGlobalParent* windowGlobal = ctx->GetCurrentWindowGlobal();
    if (!windowGlobal) {
      continue;
    }

    RefPtr<nsIURI> uri = windowGlobal->GetDocumentURI();
    if (!uri) {
      continue;
    }

    nsAutoCString url;
    uri->GetSpec(url);

    aWriter.StringElement(url);
  }
  aWriter.EndArray();
  AppendJSONStringAsProperty(aRawString, "perfstats", aString);
}

struct PerfStatsCollector {
  PerfStatsCollector() : writer(MakeUnique<StringWriteFunc>(string)) {}

  void AppendPerfStats(const nsCString& aString, ContentParent* aParent) {
    writer.StartObjectElement();
    WriteContentParent(string, writer, aString, aParent);
    writer.EndObject();
  }

  void AppendPerfStats(const nsCString& aString, GPUChild* aChild) {
    writer.StartObjectElement();
    writer.StringProperty("type", "gpu");
    writer.IntProperty("id", aChild->Id());
    AppendJSONStringAsProperty(string, "perfstats", aString);
    writer.EndObject();
  }

  ~PerfStatsCollector() {
    writer.EndArray();
    writer.End();
    promise.Resolve(string, __func__);
  }
  nsCString string;
  JSONWriter writer;
  MozPromiseHolder<PerfStats::PerfStatsPromise> promise;
};

void PerfStats::ResetCollection() {
  for (uint64_t i = 0; i < static_cast<uint64_t>(Metric::Max); i++) {
    if (!(sCollectionMask & 1 << i)) {
      continue;
    }

    mRecordedTimes[i] = 0;
    mRecordedCounts[i] = 0;
  }

  mStoredPerfStats.Clear();
}

void PerfStats::StorePerfStatsInternal(dom::ContentParent* aParent,
                                       const nsCString& aPerfStats) {
  nsCString jsonString;
  JSONWriter w(MakeUnique<StringWriteFunc>(jsonString));

  // To generate correct JSON here we don't call start and end. That causes
  // this to use Single Line mode, sadly.
  WriteContentParent(jsonString, w, aPerfStats, aParent);

  mStoredPerfStats.AppendElement(jsonString);
}

auto PerfStats::CollectPerfStatsJSONInternal() -> RefPtr<PerfStatsPromise> {
  if (!PerfStats::sCollectionMask) {
    return PerfStatsPromise::CreateAndReject(false, __func__);
  }

  if (!XRE_IsParentProcess()) {
    return PerfStatsPromise::CreateAndResolve(
        CollectLocalPerfStatsJSONInternal(), __func__);
  }

  std::shared_ptr<PerfStatsCollector> collector =
      std::make_shared<PerfStatsCollector>();

  JSONWriter& w = collector->writer;

  w.Start();
  {
    w.StartArrayProperty("processes");
    {
      w.StartObjectElement();
      {
        w.StringProperty("type", "parent");
        AppendJSONStringAsProperty(collector->string, "perfstats",
                                   CollectLocalPerfStatsJSONInternal());
      }
      w.EndObject();

      // Append any processes that closed earlier.
      for (nsCString& string : mStoredPerfStats) {
        w.StartObjectElement();
        // This trick makes indentation even more messed up than it already
        // was. However it produces technically correct JSON.
        collector->string.Append(string);
        w.EndObject();
      }
      // We do not clear this, we only clear stored perfstats when the mask is
      // reset.

      GPUProcessManager* gpuManager = GPUProcessManager::Get();
      GPUChild* gpuChild = nullptr;

      if (gpuManager) {
        gpuChild = gpuManager->GetGPUChild();
      }
      nsTArray<ContentParent*> contentParents;
      ContentParent::GetAll(contentParents);

      if (gpuChild) {
        gpuChild->SendCollectPerfStatsJSON(
            [collector, gpuChild](const nsCString& aString) {
              collector->AppendPerfStats(aString, gpuChild);
            },
            // The only feasible errors here are if something goes wrong in the
            // the bridge, we choose to ignore those.
            [](mozilla::ipc::ResponseRejectReason) {});
      }
      for (ContentParent* parent : contentParents) {
        RefPtr<ContentParent> parentRef = parent;
        parent->SendCollectPerfStatsJSON(
            [collector, parentRef](const nsCString& aString) {
              collector->AppendPerfStats(aString, parentRef.get());
            },
            // The only feasible errors here are if something goes wrong in the
            // the bridge, we choose to ignore those.
            [](mozilla::ipc::ResponseRejectReason) {});
      }
    }
  }

  return collector->promise.Ensure(__func__);
}

nsCString PerfStats::CollectLocalPerfStatsJSONInternal() {
  StaticMutexAutoLock lock(PerfStats::sMutex);

  nsCString jsonString;

  JSONWriter w(MakeUnique<StringWriteFunc>(jsonString));
  w.Start();
  {
    w.StartArrayProperty("metrics");
    {
      for (uint64_t i = 0; i < static_cast<uint64_t>(Metric::Max); i++) {
        if (!(sCollectionMask & (1 << i))) {
          continue;
        }

        w.StartObjectElement();
        {
          w.IntProperty("id", i);
          w.StringProperty("metric", MakeStringSpan(sMetricNames[i]));
          w.DoubleProperty("time", mRecordedTimes[i]);
          w.IntProperty("count", mRecordedCounts[i]);
        }
        w.EndObject();
      }
    }
    w.EndArray();
  }
  w.End();

  return jsonString;
}

}  // namespace mozilla
