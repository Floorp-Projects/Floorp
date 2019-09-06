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

// Batches and streams Telemetry samples to a JNI delegate which will
// (presumably) do something with the data. Expected to be used to route data
// up to the Android Components layer to be translated into Glean metrics.
namespace GeckoViewStreamingTelemetry {

static StaticMutexNotRecorded gMutex;

// -- The following state is accessed across threads.
// -- Do not touch these if you do not hold gMutex.

// The time the batch began.
TimeStamp gBatchBegan;
// The batch of histograms and samples.
typedef nsDataHashtable<nsCStringHashKey, nsTArray<uint32_t>> HistogramBatch;
HistogramBatch gBatch;
HistogramBatch gCategoricalBatch;
// The batches of Scalars and their values.
typedef nsDataHashtable<nsCStringHashKey, bool> BoolScalarBatch;
BoolScalarBatch gBoolScalars;
typedef nsDataHashtable<nsCStringHashKey, nsCString> StringScalarBatch;
StringScalarBatch gStringScalars;
typedef nsDataHashtable<nsCStringHashKey, uint32_t> UintScalarBatch;
UintScalarBatch gUintScalars;
// The delegate to receive the samples and values.
StaticRefPtr<StreamingTelemetryDelegate> gDelegate;

// -- End of gMutex-protected thread-unsafe-accessed data

void RegisterDelegate(const RefPtr<StreamingTelemetryDelegate>& aDelegate) {
  StaticMutexAutoLock lock(gMutex);
  gDelegate = aDelegate;
}

class SendBatchRunnable : public Runnable {
 public:
  explicit SendBatchRunnable(RefPtr<StreamingTelemetryDelegate> aDelegate,
                             HistogramBatch&& aBatch,
                             HistogramBatch&& aCategoricalBatch,
                             BoolScalarBatch&& aBoolScalars,
                             StringScalarBatch&& aStringScalars,
                             UintScalarBatch&& aUintScalars)
      : Runnable("SendBatchRunnable"),
        mDelegate(std::move(aDelegate)),
        mBatch(std::move(aBatch)),
        mCategoricalBatch(std::move(aCategoricalBatch)),
        mBoolScalars(std::move(aBoolScalars)),
        mStringScalars(std::move(aStringScalars)),
        mUintScalars(std::move(aUintScalars)) {}

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mDelegate);

    for (auto iter = mBatch.Iter(); !iter.Done(); iter.Next()) {
      const nsCString& histogramName = PromiseFlatCString(iter.Key());
      const nsTArray<uint32_t>& samples = iter.Data();

      mDelegate->ReceiveHistogramSamples(histogramName, samples);
    }
    mBatch.Clear();

    for (auto iter = mCategoricalBatch.Iter(); !iter.Done(); iter.Next()) {
      const nsCString& histogramName = PromiseFlatCString(iter.Key());
      const nsTArray<uint32_t>& samples = iter.Data();

      mDelegate->ReceiveCategoricalHistogramSamples(histogramName, samples);
    }
    mCategoricalBatch.Clear();

    for (auto iter = mBoolScalars.Iter(); !iter.Done(); iter.Next()) {
      const nsCString& scalarName = PromiseFlatCString(iter.Key());
      mDelegate->ReceiveBoolScalarValue(scalarName, iter.Data());
    }
    mBoolScalars.Clear();

    for (auto iter = mStringScalars.Iter(); !iter.Done(); iter.Next()) {
      const nsCString& scalarName = PromiseFlatCString(iter.Key());
      const nsCString& scalarValue = PromiseFlatCString(iter.Data());
      mDelegate->ReceiveStringScalarValue(scalarName, scalarValue);
    }
    mStringScalars.Clear();

    for (auto iter = mUintScalars.Iter(); !iter.Done(); iter.Next()) {
      const nsCString& scalarName = PromiseFlatCString(iter.Key());
      mDelegate->ReceiveUintScalarValue(scalarName, iter.Data());
    }
    mUintScalars.Clear();

    return NS_OK;
  }

 private:
  RefPtr<StreamingTelemetryDelegate> mDelegate;
  HistogramBatch mBatch;
  HistogramBatch mCategoricalBatch;
  BoolScalarBatch mBoolScalars;
  StringScalarBatch mStringScalars;
  UintScalarBatch mUintScalars;
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
  // move the batches' contents into the Runner.
  HistogramBatch histogramCopy;
  gBatch.SwapElements(histogramCopy);
  HistogramBatch categoricalCopy;
  gCategoricalBatch.SwapElements(categoricalCopy);
  BoolScalarBatch boolScalarCopy;
  gBoolScalars.SwapElements(boolScalarCopy);
  StringScalarBatch stringScalarCopy;
  gStringScalars.SwapElements(stringScalarCopy);
  UintScalarBatch uintScalarCopy;
  gUintScalars.SwapElements(uintScalarCopy);
  RefPtr<SendBatchRunnable> runnable = new SendBatchRunnable(
      gDelegate, std::move(histogramCopy), std::move(categoricalCopy),
      std::move(boolScalarCopy), std::move(stringScalarCopy),
      std::move(uintScalarCopy));

  // To make things easier for the delegate, dispatch to the main thread.
  NS_DispatchToMainThread(runnable);
}

// Can be called on any thread.
void BatchCheck(const StaticMutexAutoLock& aLock) {
  if (gBatchBegan.IsNull()) {
    gBatchBegan = TimeStamp::Now();
  }
  double batchDurationMs = (TimeStamp::Now() - gBatchBegan).ToMilliseconds();
  if (batchDurationMs >
      mozilla::StaticPrefs::toolkit_telemetry_geckoview_batchDurationMS()) {
    SendBatch(aLock);
    gBatchBegan = TimeStamp();
  }
}

// Can be called on any thread.
void HistogramAccumulate(const nsCString& aName, bool aIsCategorical,
                         uint32_t aValue) {
  StaticMutexAutoLock lock(gMutex);

  if (aIsCategorical) {
    nsTArray<uint32_t>& samples = gCategoricalBatch.GetOrInsert(aName);
    samples.AppendElement(aValue);
  } else {
    nsTArray<uint32_t>& samples = gBatch.GetOrInsert(aName);
    samples.AppendElement(aValue);
  }

  BatchCheck(lock);
}

void BoolScalarSet(const nsCString& aName, bool aValue) {
  StaticMutexAutoLock lock(gMutex);

  gBoolScalars.Put(aName, aValue);

  BatchCheck(lock);
}

void StringScalarSet(const nsCString& aName, const nsCString& aValue) {
  StaticMutexAutoLock lock(gMutex);

  gStringScalars.Put(aName, aValue);

  BatchCheck(lock);
}

void UintScalarSet(const nsCString& aName, uint32_t aValue) {
  StaticMutexAutoLock lock(gMutex);

  gUintScalars.Put(aName, aValue);

  BatchCheck(lock);
}

}  // namespace GeckoViewStreamingTelemetry
