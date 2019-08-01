/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoViewStreamingTelemetry.h"

#include "mozilla/Assertions.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPrefs_toolkit.h"
#include "mozilla/TimeStamp.h"
#include "nsDataHashtable.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

using mozilla::Runnable;
using mozilla::StaticMutexAutoLock;
using mozilla::StaticMutexNotRecorded;
using mozilla::StaticRefPtr;
using mozilla::TimeStamp;

// Batches and streams Histogram accumulations to a JNI delegate which will
// (presumably) do something with the data. Expected to be used to route data
// up to the Android Components layer to be translated into Glean metrics.
namespace GeckoViewStreamingTelemetry {

static StaticMutexNotRecorded gMutex;

// -- The following state is accessed across threads.
// -- Do not touch these if you do not hold gMutex.

// The time the batch began.
TimeStamp gBatchBegan;
// The batch of histograms and samples.
typedef nsDataHashtable<nsCStringHashKey, nsTArray<uint32_t>> Batch;
Batch gBatch;
// The delegate to receive the Histograms' samples.
StaticRefPtr<StreamingTelemetryDelegate> gDelegate;

// -- End of gMutex-protected thread-unsafe-accessed data

void RegisterDelegate(const RefPtr<StreamingTelemetryDelegate>& aDelegate) {
  StaticMutexAutoLock lock(gMutex);
  gDelegate = aDelegate;
}

class SendBatchRunnable : public Runnable {
 public:
  explicit SendBatchRunnable(RefPtr<StreamingTelemetryDelegate> aDelegate,
                             Batch&& aBatch)
      : Runnable("SendBatchRunnable"),
        mDelegate(std::move(aDelegate)),
        mBatch(std::move(aBatch)) {}

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mDelegate);

    for (auto iter = mBatch.Iter(); !iter.Done(); iter.Next()) {
      const nsCString& histogramName = PromiseFlatCString(iter.Key());
      const nsTArray<uint32_t>& samples = iter.Data();

      mDelegate->ReceiveHistogramSamples(histogramName, samples);
    }

    mBatch.Clear();
    return NS_OK;
  }

 private:
  RefPtr<StreamingTelemetryDelegate> mDelegate;
  Batch mBatch;
};  // class SendBatchRunnable

// Can be called on any thread.
void SendBatch(const StaticMutexAutoLock& aLock) {
  if (!gDelegate) {
    NS_WARNING(
        "Being asked to send Streaming Telemetry with no registered Streaming "
        "Telemetry Delegate. Will try again later.");
    // Give us another full Batch Duration to register a delegate.
    gBatchBegan = TimeStamp::Now();
    return;
  }

  // To make it so accumulations within the delegation don't deadlock us,
  // move the batch's contents into the Runner.
  Batch copy;
  gBatch.SwapElements(copy);
  RefPtr<SendBatchRunnable> runnable =
      new SendBatchRunnable(gDelegate, std::move(copy));

  // To make things easier for the delegate, dispatch to the main thread.
  NS_DispatchToMainThread(runnable);
}

// Can be called on any thread.
void HistogramAccumulate(const nsCString& aName, uint32_t aValue) {
  StaticMutexAutoLock lock(gMutex);

  if (gBatch.Count() == 0) {
    gBatchBegan = TimeStamp::Now();
  }
  nsTArray<uint32_t>& samples = gBatch.GetOrInsert(aName);
  samples.AppendElement(aValue);

  double batchDurationMs = (TimeStamp::Now() - gBatchBegan).ToMilliseconds();
  if (batchDurationMs >
      mozilla::StaticPrefs::toolkit_telemetry_geckoview_batchDurationMS()) {
    SendBatch(lock);
  }
}

}  // namespace GeckoViewStreamingTelemetry
