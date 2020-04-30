/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentUtils.h"
#include "nsThreadUtils.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/Logging.h"
#include "mozilla/PerformanceUtils.h"
#include "mozilla/PerformanceMetricsCollector.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WorkerDebugger.h"
#include "mozilla/dom/WorkerDebuggerManager.h"

using namespace mozilla;
using namespace mozilla::dom;

static mozilla::LazyLogModule sPerfLog("PerformanceMetricsCollector");
#ifdef LOG
#  undef LOG
#endif
#define LOG(args) MOZ_LOG(sPerfLog, mozilla::LogLevel::Debug, args)

namespace mozilla {

//
// class IPCTimeout
//
NS_IMPL_ISUPPORTS(IPCTimeout, nsIObserver)

// static
IPCTimeout* IPCTimeout::CreateInstance(AggregatedResults* aResults) {
  MOZ_ASSERT(aResults);
  uint32_t delay = StaticPrefs::dom_performance_children_results_ipc_timeout();
  if (delay == 0) {
    return nullptr;
  }
  return new IPCTimeout(aResults, delay);
}

IPCTimeout::IPCTimeout(AggregatedResults* aResults, uint32_t aDelay)
    : mResults(aResults) {
  MOZ_ASSERT(aResults);
  MOZ_ASSERT(aDelay > 0);
  mozilla::DebugOnly<nsresult> rv = NS_NewTimerWithObserver(
      getter_AddRefs(mTimer), this, aDelay, nsITimer::TYPE_ONE_SHOT);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  LOG(("IPCTimeout timer created"));
}

IPCTimeout::~IPCTimeout() { Cancel(); }

void IPCTimeout::Cancel() {
  if (mTimer) {
    LOG(("IPCTimeout timer canceled"));
    mTimer->Cancel();
    mTimer = nullptr;
  }
}

NS_IMETHODIMP
IPCTimeout::Observe(nsISupports* aSubject, const char* aTopic,
                    const char16_t* aData) {
  MOZ_ASSERT(strcmp(aTopic, NS_TIMER_CALLBACK_TOPIC) == 0);
  LOG(("IPCTimeout timer triggered"));
  mResults->ResolveNow();
  return NS_OK;
}

//
// class AggregatedResults
//
AggregatedResults::AggregatedResults(nsID aUUID,
                                     PerformanceMetricsCollector* aCollector)
    : mPendingResults(0), mCollector(aCollector), mUUID(aUUID) {
  MOZ_ASSERT(aCollector);
  mIPCTimeout = IPCTimeout::CreateInstance(this);
}

void AggregatedResults::Abort(nsresult aReason) {
  MOZ_ASSERT(!mHolder.IsEmpty());
  MOZ_ASSERT(NS_FAILED(aReason));
  if (mIPCTimeout) {
    mIPCTimeout->Cancel();
    mIPCTimeout = nullptr;
  }
  mHolder.Reject(aReason, __func__);
  mPendingResults = 0;
}

void AggregatedResults::ResolveNow() {
  MOZ_ASSERT(!mHolder.IsEmpty());
  LOG(("[%s] Early resolve", nsIDToCString(mUUID).get()));
  mHolder.Resolve(CopyableTArray(mData), __func__);
  mIPCTimeout = nullptr;
  mCollector->ForgetAggregatedResults(mUUID);
}

void AggregatedResults::AppendResult(
    const nsTArray<dom::PerformanceInfo>& aMetrics) {
  if (mHolder.IsEmpty()) {
    // A previous call failed and the promise was already rejected
    return;
  }
  MOZ_ASSERT(mPendingResults > 0);

  // Each PerformanceInfo is converted into a PerformanceInfoDictionary
  for (const PerformanceInfo& result : aMetrics) {
    mozilla::dom::Sequence<mozilla::dom::CategoryDispatchDictionary> items;

    for (const CategoryDispatch& entry : result.items()) {
      uint32_t count = entry.count();
      if (count == 0) {
        continue;
      }
      CategoryDispatchDictionary* item = items.AppendElement(fallible);
      if (NS_WARN_IF(!item)) {
        Abort(NS_ERROR_OUT_OF_MEMORY);
        return;
      }
      item->mCategory = entry.category();
      item->mCount = count;
    }

    PerformanceInfoDictionary* data = mData.AppendElement(fallible);
    if (NS_WARN_IF(!data)) {
      Abort(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    data->mPid = result.pid();
    data->mWindowId = result.windowId();
    data->mHost.Assign(result.host());
    data->mDuration = result.duration();
    data->mCounterId = result.counterId();
    data->mIsWorker = result.isWorker();
    data->mIsTopLevel = result.isTopLevel();
    data->mMemoryInfo.mDomDom = result.memory().domDom();
    data->mMemoryInfo.mDomStyle = result.memory().domStyle();
    data->mMemoryInfo.mDomOther = result.memory().domOther();
    data->mMemoryInfo.mGCHeapUsage = result.memory().GCHeapUsage();
    data->mMemoryInfo.mMedia.mAudioSize = result.memory().media().audioSize();
    data->mMemoryInfo.mMedia.mVideoSize = result.memory().media().videoSize();
    data->mMemoryInfo.mMedia.mResourcesSize =
        result.memory().media().resourcesSize();
    data->mItems = items;
  }

  mPendingResults--;
  if (mPendingResults) {
    return;
  }

  LOG(("[%s] All data collected, resolving promise",
       nsIDToCString(mUUID).get()));
  if (mIPCTimeout) {
    mIPCTimeout->Cancel();
    mIPCTimeout = nullptr;
  }
  nsTArray<dom::PerformanceInfoDictionary> data;
  data.Assign(mData);
  mHolder.Resolve(std::move(data), __func__);
  mCollector->ForgetAggregatedResults(mUUID);
}

void AggregatedResults::SetNumResultsRequired(uint32_t aNumResultsRequired) {
  MOZ_ASSERT(!mPendingResults && aNumResultsRequired);
  mPendingResults = aNumResultsRequired;
}

RefPtr<RequestMetricsPromise> AggregatedResults::GetPromise() {
  return mHolder.Ensure(__func__);
}

//
// class PerformanceMetricsCollector (singleton)
//

// raw pointer for the singleton
PerformanceMetricsCollector* gInstance = nullptr;

PerformanceMetricsCollector::~PerformanceMetricsCollector() {
  MOZ_ASSERT(gInstance == this);
  gInstance = nullptr;
}

void PerformanceMetricsCollector::ForgetAggregatedResults(const nsID& aUUID) {
  MOZ_ASSERT(gInstance);
  MOZ_ASSERT(XRE_IsParentProcess());
  // This Remove() call will trigger AggregatedResults DTOR and if its
  // the last in the table, the DTOR of PerformanceMetricsCollector.
  // That's why we need to make sure we hold a reference here before the call
  RefPtr<PerformanceMetricsCollector> kungFuDeathGrip = this;
  LOG(("[%s] Removing from the table", nsIDToCString(aUUID).get()));
  mAggregatedResults.Remove(aUUID);
}

// static
RefPtr<RequestMetricsPromise> PerformanceMetricsCollector::RequestMetrics() {
  MOZ_ASSERT(XRE_IsParentProcess());
  RefPtr<PerformanceMetricsCollector> pmc = gInstance;
  if (!pmc) {
    pmc = new PerformanceMetricsCollector();
    gInstance = pmc;
  }
  return pmc->RequestMetricsInternal();
}

RefPtr<RequestMetricsPromise>
PerformanceMetricsCollector::RequestMetricsInternal() {
  // each request has its own UUID
  nsID uuid;
  nsresult rv = nsContentUtils::GenerateUUIDInPlace(uuid);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return RequestMetricsPromise::CreateAndReject(rv, __func__);
  }

  LOG(("[%s] Requesting Performance Metrics", nsIDToCString(uuid).get()));

  // Getting all content processes
  nsTArray<ContentParent*> children;
  ContentParent::GetAll(children);
  uint32_t numChildren = children.Length();

  // keep track of all results in an AggregatedResults instance
  UniquePtr<AggregatedResults> results =
      MakeUnique<AggregatedResults>(uuid, this);
  RefPtr<RequestMetricsPromise> promise = results->GetPromise();

  // We want to get back as many results as children + one parent if needed
  uint32_t numResultsRequired = children.Length();
  nsTArray<RefPtr<PerformanceInfoPromise>> localPromises =
      CollectPerformanceInfo();
  if (!localPromises.IsEmpty()) {
    numResultsRequired++;
  }

  LOG(("[%s] Expecting %d results back", nsIDToCString(uuid).get(),
       numResultsRequired));
  results->SetNumResultsRequired(numResultsRequired);
  mAggregatedResults.Put(uuid, std::move(results));

  // calling all content processes via IPDL (async)
  for (uint32_t i = 0; i < numChildren; i++) {
    if (NS_WARN_IF(!children[i]->SendRequestPerformanceMetrics(uuid))) {
      LOG(("[%s] Failed to send request to child %d", nsIDToCString(uuid).get(),
           i));
      mAggregatedResults.GetValue(uuid)->get()->Abort(NS_ERROR_FAILURE);
      return RequestMetricsPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
    }
    LOG(("[%s] Request sent to child %d", nsIDToCString(uuid).get(), i));
  }

  nsTArray<RefPtr<PerformanceInfoPromise>> promises = CollectPerformanceInfo();
  if (promises.IsEmpty()) {
    return promise;
  }

  // collecting the current process PerformanceInfo
  PerformanceInfoPromise::All(NS_GetCurrentThread(), localPromises)
      ->Then(
          NS_GetCurrentThread(), __func__,
          [uuid](const nsTArray<mozilla::dom::PerformanceInfo> aResult) {
            LOG(("[%s] Local CollectPerformanceInfo promise resolved",
                 nsIDToCString(uuid).get()));
            DataReceived(uuid, aResult);
          },
          [](const nsresult aResult) {});

  return promise;
}

// static
nsresult PerformanceMetricsCollector::DataReceived(
    const nsID& aUUID, const nsTArray<PerformanceInfo>& aMetrics) {
  // If some content process were unresponsive on shutdown, we may get called
  // here with late data received from children - so instead of asserting
  // that gInstance is available, we just return.
  if (!gInstance) {
    LOG(("[%s] gInstance is gone", nsIDToCString(aUUID).get()));
    return NS_OK;
  }
  MOZ_ASSERT(XRE_IsParentProcess());
  return gInstance->DataReceivedInternal(aUUID, aMetrics);
}

nsresult PerformanceMetricsCollector::DataReceivedInternal(
    const nsID& aUUID, const nsTArray<PerformanceInfo>& aMetrics) {
  MOZ_ASSERT(gInstance == this);
  UniquePtr<AggregatedResults>* results = mAggregatedResults.GetValue(aUUID);
  if (!results) {
    LOG(("[%s] UUID is gone from mAggregatedResults",
         nsIDToCString(aUUID).get()));
    return NS_ERROR_FAILURE;
  }

  LOG(("[%s] Received one PerformanceInfo array", nsIDToCString(aUUID).get()));
  AggregatedResults* aggregatedResults = results->get();
  MOZ_ASSERT(aggregatedResults);

  // If this is the last result, AppendResult() will trigger the deletion
  // of this collector, nothing should be done after this line.
  aggregatedResults->AppendResult(aMetrics);
  return NS_OK;
}

}  // namespace mozilla
