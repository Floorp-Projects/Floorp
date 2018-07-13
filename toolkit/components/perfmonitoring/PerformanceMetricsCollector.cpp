/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThreadUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/PerformanceUtils.h"
#include "mozilla/PerformanceMetricsCollector.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WorkerDebugger.h"
#include "mozilla/dom/WorkerDebuggerManager.h"

using namespace mozilla;
using namespace mozilla::dom;

static mozilla::LazyLogModule sPerfLog("PerformanceMetricsCollector");
#ifdef LOG
#undef LOG
#endif
#define LOG(args) MOZ_LOG(sPerfLog, mozilla::LogLevel::Debug, args)

namespace mozilla {

//
// class AggregatedResults
//
AggregatedResults::AggregatedResults(nsID aUUID,
                                     PerformanceMetricsCollector* aCollector,
                                     dom::Promise* aPromise)
  : mPromise(aPromise)
  , mPendingResults(0)
  , mCollector(aCollector)
  , mUUID(aUUID)
{
  MOZ_ASSERT(aCollector);
  MOZ_ASSERT(aPromise);
}

void
AggregatedResults::Abort(nsresult aReason)
{
  MOZ_ASSERT(mPromise);
  MOZ_ASSERT(NS_FAILED(aReason));
  mPromise->MaybeReject(aReason);
  mPromise = nullptr;
  mPendingResults = 0;
}

void
AggregatedResults::AppendResult(const nsTArray<dom::PerformanceInfo>& aMetrics)
{
  if (!mPromise) {
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
    data->mIsWorker = result.isWorker();
    data->mIsTopLevel = result.isTopLevel();
    data->mItems = items;
  }

  mPendingResults--;
  if (mPendingResults) {
    return;
  }

  LOG(("[%s] All data collected, resolving promise", nsIDToCString(mUUID).get()));
  mPromise->MaybeResolve(mData);
  mCollector->ForgetAggregatedResults(mUUID);
}

void
AggregatedResults::SetNumResultsRequired(uint32_t aNumResultsRequired)
{
  MOZ_ASSERT(!mPendingResults && aNumResultsRequired);
  mPendingResults = aNumResultsRequired;
}

//
// class PerformanceMetricsCollector (singleton)
//

// raw pointer for the singleton
PerformanceMetricsCollector* gInstance = nullptr;

PerformanceMetricsCollector::~PerformanceMetricsCollector()
{
  MOZ_ASSERT(gInstance == this);
  gInstance = nullptr;
}

void
PerformanceMetricsCollector::ForgetAggregatedResults(const nsID& aUUID)
{
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
void
PerformanceMetricsCollector::RequestMetrics(dom::Promise* aPromise)
{
  MOZ_ASSERT(aPromise);
  MOZ_ASSERT(XRE_IsParentProcess());
  RefPtr<PerformanceMetricsCollector> pmc = gInstance;
  if (!pmc) {
    pmc = new PerformanceMetricsCollector();
    gInstance = pmc;
  }
  pmc->RequestMetricsInternal(aPromise);
}

void
PerformanceMetricsCollector::RequestMetricsInternal(dom::Promise* aPromise)
{
  // each request has its own UUID
  nsID uuid;
  nsresult rv = nsContentUtils::GenerateUUIDInPlace(uuid);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aPromise->MaybeReject(rv);
    return;
  }

  LOG(("[%s] Requesting Performance Metrics", nsIDToCString(uuid).get()));

  // Getting all content processes
  nsTArray<ContentParent*> children;
  ContentParent::GetAll(children);

  uint32_t numChildren = children.Length();
  LOG(("[%s] Expecting %d results back", nsIDToCString(uuid).get(), numChildren + 1));

  // keep track of all results in an AggregatedResults instance
  UniquePtr<AggregatedResults> results = MakeUnique<AggregatedResults>(uuid, this, aPromise);

  // We want to get back as many results as children, plus the result
  // from the content parent itself
  results->SetNumResultsRequired(numChildren + 1);
  mAggregatedResults.Put(uuid, std::move(results));

  // calling all content processes via IPDL (async)
  for (uint32_t i = 0; i < numChildren; i++) {
    if (NS_WARN_IF(!children[i]->SendRequestPerformanceMetrics(uuid))) {
      LOG(("[%s] Failed to send request to child %d", nsIDToCString(uuid).get(), i));
      mAggregatedResults.GetValue(uuid)->get()->Abort(NS_ERROR_FAILURE);
      return;
    }
  }

  // collecting the current process PerformanceInfo
  nsTArray<PerformanceInfo> info;
  CollectPerformanceInfo(info);
  DataReceived(uuid, info);
}


// static
nsresult
PerformanceMetricsCollector::DataReceived(const nsID& aUUID,
                                          const nsTArray<PerformanceInfo>& aMetrics)
{
  MOZ_ASSERT(gInstance);
  MOZ_ASSERT(XRE_IsParentProcess());
  return gInstance->DataReceivedInternal(aUUID, aMetrics);
}

nsresult
PerformanceMetricsCollector::DataReceivedInternal(const nsID& aUUID,
                                                  const nsTArray<PerformanceInfo>& aMetrics)
{
  MOZ_ASSERT(gInstance == this);
  UniquePtr<AggregatedResults>* results = mAggregatedResults.GetValue(aUUID);
  if (!results) {
    return NS_ERROR_FAILURE;
  }

  LOG(("[%s] Received one PerformanceInfo array", nsIDToCString(aUUID).get()));
  AggregatedResults* aggregatedResults = results->get();
  MOZ_ASSERT(aggregatedResults);

  // If this is the last result, APpendResult() will trigger the deletion
  // of this collector, nothing should be done after this line.
  aggregatedResults->AppendResult(aMetrics);
  return NS_OK;
}

} // namespace
