/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PerformanceMetricsCollector_h
#define PerformanceMetricsCollector_h

#include "nsIObserver.h"
#include "nsITimer.h"
#include "nsID.h"
#include "nsTHashMap.h"
#include "mozilla/dom/ChromeUtilsBinding.h"  // defines PerformanceInfoDictionary
#include "mozilla/dom/DOMTypes.h"            // defines PerformanceInfo
#include "mozilla/PerformanceTypes.h"

namespace mozilla {

namespace dom {
class Promise;
}

class PerformanceMetricsCollector;
class AggregatedResults;

class IPCTimeout final : public nsIObserver {
 public:
  NS_DECL_NSIOBSERVER
  NS_DECL_ISUPPORTS
  static IPCTimeout* CreateInstance(AggregatedResults* aResults);
  void Cancel();

 private:
  IPCTimeout(AggregatedResults* aResults, uint32_t aDelay);
  ~IPCTimeout();

  nsCOMPtr<nsITimer> mTimer;
  AggregatedResults* mResults;
};

// AggregatedResults receives PerformanceInfo results that are collected
// asynchronously via IPDL from all content processes.
// They are converted into an array of
// PerformanceInfoDictionary dictionaries (webidl)
//
// Once every process have sent back its results, AggregatedResults will
// resolve the MozPromise returned by GetPromise()
// with all the collected data.
//
// See ChromeUtils::RequestPerformanceMetrics.
class AggregatedResults final {
 public:
  AggregatedResults(nsID aUUID, PerformanceMetricsCollector* aCollector);
  ~AggregatedResults() = default;
  void AppendResult(const nsTArray<dom::PerformanceInfo>& aMetrics);
  void SetNumResultsRequired(uint32_t aNumResultsRequired);
  void Abort(nsresult aReason);
  void ResolveNow();
  RefPtr<RequestMetricsPromise> GetPromise();

 private:
  RefPtr<IPCTimeout> mIPCTimeout;
  MozPromiseHolder<RequestMetricsPromise> mHolder;
  uint32_t mPendingResults;
  FallibleTArray<dom::PerformanceInfoDictionary> mData;

  // AggregatedResults keeps a reference on the collector
  // so it gets destructed when all pending AggregatedResults
  // are themselves destructed when removed from
  // PerformanceMetricsCollector::mAggregatedResults.
  //
  // This lifecycle ensures that everything is released once
  // all pending results are sent.
  RefPtr<PerformanceMetricsCollector> mCollector;
  nsID mUUID;
};

//
// PerformanceMetricsCollector is instanciated as a singleton, and creates
// one AggregatedResults instance everytime metrics are requested.
//
// Each AggregatedResults has a unique identifier (UUID) that is used
// to send metrics requests via IPDL. When metrics are back in an
// asynchronous fashion, the UUID is used to append the data to the
// right AggregatedResults instance and eventually let it resolve the
// linked promise.
//
class PerformanceMetricsCollector final {
 public:
  NS_INLINE_DECL_REFCOUNTING(PerformanceMetricsCollector)
  static RefPtr<RequestMetricsPromise> RequestMetrics();
  static nsresult DataReceived(const nsID& aUUID,
                               const nsTArray<dom::PerformanceInfo>& aMetrics);
  void ForgetAggregatedResults(const nsID& aUUID);

 private:
  ~PerformanceMetricsCollector();
  RefPtr<RequestMetricsPromise> RequestMetricsInternal();
  nsresult DataReceivedInternal(const nsID& aUUID,
                                const nsTArray<dom::PerformanceInfo>& aMetrics);
  nsTHashMap<nsID, UniquePtr<AggregatedResults>> mAggregatedResults;
};

}  // namespace mozilla
#endif  // PerformanceMetricsCollector_h
