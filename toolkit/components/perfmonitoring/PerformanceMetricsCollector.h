/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PerformanceMetricsCollector_h
#define PerformanceMetricsCollector_h

#include "nsID.h"
#include "mozilla/dom/ChromeUtilsBinding.h"  // defines PerformanceInfoDictionary
#include "mozilla/dom/DOMTypes.h"   // defines PerformanceInfo

namespace mozilla {

namespace dom {
  class Promise;
}

class PerformanceMetricsCollector;

// AggregatedResults receives PerformanceInfo results that are collected
// via IPDL from all content processes and the main process. They
// are converted into an array of PerformanceInfoDictionary dictionaries (webidl)
//
// The class is instanciated with a Promise and a number of processes
// that are supposed to send back results.
//
// Once every process have sent back its results, AggregatedResults will
// resolve the promise with all the collected data and send back the
// dictionnary.
//
class AggregatedResults final
{
public:
  AggregatedResults(nsID aUUID, PerformanceMetricsCollector* aCollector,
                    dom::Promise* aPromise);
  ~AggregatedResults() = default;
  void AppendResult(const nsTArray<dom::PerformanceInfo>& aMetrics);
  void SetNumResultsRequired(uint32_t aNumResultsRequired);
  void Abort(nsresult aReason);

private:
  RefPtr<dom::Promise> mPromise;
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
class PerformanceMetricsCollector final
{
public:
  NS_INLINE_DECL_REFCOUNTING(PerformanceMetricsCollector)

  static void RequestMetrics(dom::Promise* aPromise);
  static nsresult DataReceived(const nsID& aUUID,
                               const nsTArray<dom::PerformanceInfo>& aMetrics);
  void ForgetAggregatedResults(const nsID& aUUID);

private:
  ~PerformanceMetricsCollector();
  void RequestMetricsInternal(dom::Promise* aPromise);
  nsresult DataReceivedInternal(const nsID& aUUID,
                                const nsTArray<dom::PerformanceInfo>& aMetrics);
  nsDataHashtable<nsIDHashKey, UniquePtr<AggregatedResults>> mAggregatedResults;
};

} // namespace mozilla
#endif   // PerformanceMetricsCollector_h
