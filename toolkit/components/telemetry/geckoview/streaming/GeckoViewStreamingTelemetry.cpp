/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoViewStreamingTelemetry.h"

#include "mozilla/Assertions.h"
#include "mozilla/Services.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/StaticPrefs_toolkit.h"
#include "mozilla/TimeStamp.h"
#include "nsTHashMap.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsITimer.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

using mozilla::Runnable;
using mozilla::StaticMutex;
using mozilla::StaticMutexAutoLock;
using mozilla::StaticRefPtr;
using mozilla::TimeStamp;

// Batches and streams Telemetry samples to a JNI delegate which will
// (presumably) do something with the data. Expected to be used to route data
// up to the Android Components layer to be translated into Glean metrics.
namespace GeckoViewStreamingTelemetry {

class LifecycleObserver;
void SendBatch(const StaticMutexAutoLock& aLock);

// Topic on which we flush the batch.
static const char* const kApplicationBackgroundTopic = "application-background";

static StaticMutex gMutex;

// -- The following state is accessed across threads.
// -- Do not touch these if you do not hold gMutex.

// The time the batch began.
TimeStamp gBatchBegan;
// The batch of histograms and samples.
typedef nsTHashMap<nsCStringHashKey, nsTArray<uint32_t>> HistogramBatch;
HistogramBatch gBatch;
HistogramBatch gCategoricalBatch;
// The batches of Scalars and their values.
typedef nsTHashMap<nsCStringHashKey, bool> BoolScalarBatch;
BoolScalarBatch gBoolScalars;
typedef nsTHashMap<nsCStringHashKey, nsCString> StringScalarBatch;
StringScalarBatch gStringScalars;
typedef nsTHashMap<nsCStringHashKey, uint32_t> UintScalarBatch;
UintScalarBatch gUintScalars;
// The delegate to receive the samples and values.
StaticRefPtr<StreamingTelemetryDelegate> gDelegate;
// Lifecycle observer used to flush the batch when backgrounded.
StaticRefPtr<LifecycleObserver> gObserver;

// -- End of gMutex-protected thread-unsafe-accessed data

// Timer that ensures data in the batch never gets too stale.
// This timer may only be manipulated on the Main Thread.
StaticRefPtr<nsITimer> gJICTimer;

class LifecycleObserver final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  LifecycleObserver() = default;

 protected:
  ~LifecycleObserver() = default;
};

NS_IMPL_ISUPPORTS(LifecycleObserver, nsIObserver);

NS_IMETHODIMP
LifecycleObserver::Observe(nsISupports* aSubject, const char* aTopic,
                           const char16_t* aData) {
  if (!strcmp(aTopic, kApplicationBackgroundTopic)) {
    StaticMutexAutoLock lock(gMutex);
    SendBatch(lock);
  }
  return NS_OK;
}

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

    if (gJICTimer) {
      gJICTimer->Cancel();
    }

    for (const auto& entry : mBatch) {
      const nsCString& histogramName = PromiseFlatCString(entry.GetKey());
      const nsTArray<uint32_t>& samples = entry.GetData();

      mDelegate->ReceiveHistogramSamples(histogramName, samples);
    }
    mBatch.Clear();

    for (const auto& entry : mCategoricalBatch) {
      const nsCString& histogramName = PromiseFlatCString(entry.GetKey());
      const nsTArray<uint32_t>& samples = entry.GetData();

      mDelegate->ReceiveCategoricalHistogramSamples(histogramName, samples);
    }
    mCategoricalBatch.Clear();

    for (const auto& entry : mBoolScalars) {
      const nsCString& scalarName = PromiseFlatCString(entry.GetKey());
      mDelegate->ReceiveBoolScalarValue(scalarName, entry.GetData());
    }
    mBoolScalars.Clear();

    for (const auto& entry : mStringScalars) {
      const nsCString& scalarName = PromiseFlatCString(entry.GetKey());
      const nsCString& scalarValue = PromiseFlatCString(entry.GetData());
      mDelegate->ReceiveStringScalarValue(scalarName, scalarValue);
    }
    mStringScalars.Clear();

    for (const auto& entry : mUintScalars) {
      const nsCString& scalarName = PromiseFlatCString(entry.GetKey());
      mDelegate->ReceiveUintScalarValue(scalarName, entry.GetData());
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
// NOTE: Pay special attention to what you call in this method as if it
// accumulates to a gv-streaming-enabled probe we will deadlock the calling
// thread.
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
  if (!gObserver) {
    gObserver = new LifecycleObserver();
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->AddObserver(gObserver, kApplicationBackgroundTopic, false);
    }
  }
  if (gBatchBegan.IsNull()) {
    // Time to begin a new batch.
    gBatchBegan = TimeStamp::Now();
    // Set a just-in-case timer to enforce an upper-bound on batch staleness.
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "GeckoviewStreamingTelemetry::ArmTimer", []() -> void {
          if (!gJICTimer) {
            gJICTimer = NS_NewTimer().take();
          }
          if (gJICTimer) {
            gJICTimer->InitWithNamedFuncCallback(
                [](nsITimer*, void*) -> void {
                  StaticMutexAutoLock locker(gMutex);
                  SendBatch(locker);
                },
                nullptr,
                mozilla::StaticPrefs::
                    toolkit_telemetry_geckoview_maxBatchStalenessMS(),
                nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY,
                "GeckoviewStreamingTelemetry::SendBatch");
          }
        }));
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
    nsTArray<uint32_t>& samples = gCategoricalBatch.LookupOrInsert(aName);
    samples.AppendElement(aValue);
  } else {
    nsTArray<uint32_t>& samples = gBatch.LookupOrInsert(aName);
    samples.AppendElement(aValue);
  }

  BatchCheck(lock);
}

void BoolScalarSet(const nsCString& aName, bool aValue) {
  StaticMutexAutoLock lock(gMutex);

  gBoolScalars.InsertOrUpdate(aName, aValue);

  BatchCheck(lock);
}

void StringScalarSet(const nsCString& aName, const nsCString& aValue) {
  StaticMutexAutoLock lock(gMutex);

  gStringScalars.InsertOrUpdate(aName, aValue);

  BatchCheck(lock);
}

void UintScalarSet(const nsCString& aName, uint32_t aValue) {
  StaticMutexAutoLock lock(gMutex);

  gUintScalars.InsertOrUpdate(aName, aValue);

  BatchCheck(lock);
}

}  // namespace GeckoViewStreamingTelemetry
