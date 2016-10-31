/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/GCAPI.h"
#include "nsString.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsBaseHashtable.h"
#include "nsClassHashtable.h"
#include "nsITelemetry.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/gfx/GPUParent.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/Atomics.h"
#include "mozilla/StartupTimeline.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Unused.h"

#include "TelemetryCommon.h"
#include "TelemetryHistogram.h"

#include "base/histogram.h"

using base::Histogram;
using base::StatisticsRecorder;
using base::BooleanHistogram;
using base::CountHistogram;
using base::FlagHistogram;
using base::LinearHistogram;
using mozilla::StaticMutex;
using mozilla::StaticMutexAutoLock;
using mozilla::StaticAutoPtr;
using mozilla::Telemetry::Accumulation;
using mozilla::Telemetry::KeyedAccumulation;


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// Naming: there are two kinds of functions in this file:
//
// * Functions named internal_*: these can only be reached via an
//   interface function (TelemetryHistogram::*).  They mostly expect
//   the interface function to have acquired
//   |gTelemetryHistogramMutex|, so they do not have to be
//   thread-safe.  However, those internal_* functions that are
//   reachable from internal_WrapAndReturnHistogram and
//   internal_WrapAndReturnKeyedHistogram can sometimes be called
//   without |gTelemetryHistogramMutex|, and so might be racey.
//
// * Functions named TelemetryHistogram::*.  This is the external interface.
//   Entries and exits to these functions are serialised using
//   |gTelemetryHistogramMutex|, except for GetAddonHistogramSnapshots,
//   GetKeyedHistogramSnapshots and CreateHistogramSnapshots.
//
// Avoiding races and deadlocks:
//
// All functions in the external interface (TelemetryHistogram::*) are
// serialised using the mutex |gTelemetryHistogramMutex|.  This means
// that the external interface is thread-safe, and many of the
// internal_* functions can ignore thread safety.  But it also brings
// a danger of deadlock if any function in the external interface can
// get back to that interface.  That is, we will deadlock on any call
// chain like this
//
// TelemetryHistogram::* -> .. any functions .. -> TelemetryHistogram::*
//
// To reduce the danger of that happening, observe the following rules:
//
// * No function in TelemetryHistogram::* may directly call, nor take the
//   address of, any other function in TelemetryHistogram::*.
//
// * No internal function internal_* may call, nor take the address
//   of, any function in TelemetryHistogram::*.
//
// internal_WrapAndReturnHistogram and
// internal_WrapAndReturnKeyedHistogram are not protected by
// |gTelemetryHistogramMutex| because they make calls to the JS
// engine, but that can in turn call back to Telemetry and hence back
// to a TelemetryHistogram:: function, in order to report GC and other
// statistics.  This would lead to deadlock due to attempted double
// acquisition of |gTelemetryHistogramMutex|, if the internal_* functions
// were required to be protected by |gTelemetryHistogramMutex|.  To
// break that cycle, we relax that requirement.  Unfortunately this
// means that this file is not guaranteed race-free.


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE TYPES

#define EXPIRED_ID "__expired__"
#define SUBSESSION_HISTOGRAM_PREFIX "sub#"
#define KEYED_HISTOGRAM_NAME_SEPARATOR "#"
#define CONTENT_HISTOGRAM_SUFFIX "#content"
#define GPU_HISTOGRAM_SUFFIX "#gpu"

namespace {

using mozilla::Telemetry::Common::AutoHashtable;
using mozilla::Telemetry::Common::IsExpiredVersion;
using mozilla::Telemetry::Common::CanRecordDataset;
using mozilla::Telemetry::Common::IsInDataset;

class KeyedHistogram;

typedef nsBaseHashtableET<nsDepCharHashKey, mozilla::Telemetry::ID>
          CharPtrEntryType;

typedef AutoHashtable<CharPtrEntryType> HistogramMapType;

typedef nsClassHashtable<nsCStringHashKey, KeyedHistogram>
          KeyedHistogramMapType;

// Hardcoded probes
struct HistogramInfo {
  uint32_t min;
  uint32_t max;
  uint32_t bucketCount;
  uint32_t histogramType;
  uint32_t id_offset;
  uint32_t expiration_offset;
  uint32_t dataset;
  uint32_t label_index;
  uint32_t label_count;
  bool keyed;

  const char *id() const;
  const char *expiration() const;
  nsresult label_id(const char* label, uint32_t* labelId) const;
};

struct AddonHistogramInfo {
  uint32_t min;
  uint32_t max;
  uint32_t bucketCount;
  uint32_t histogramType;
  Histogram *h;
};

enum reflectStatus {
  REFLECT_OK,
  REFLECT_CORRUPT,
  REFLECT_FAILURE
};

typedef StatisticsRecorder::Histograms::iterator HistogramIterator;

typedef nsBaseHashtableET<nsCStringHashKey, AddonHistogramInfo>
          AddonHistogramEntryType;

typedef AutoHashtable<AddonHistogramEntryType>
          AddonHistogramMapType;

typedef nsBaseHashtableET<nsCStringHashKey, AddonHistogramMapType *>
          AddonEntryType;

typedef AutoHashtable<AddonEntryType> AddonMapType;

} // namespace


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE STATE, SHARED BY ALL THREADS

namespace {

// Set to true once this global state has been initialized
bool gInitDone = false;

bool gCanRecordBase = false;
bool gCanRecordExtended = false;

HistogramMapType gHistogramMap(mozilla::Telemetry::HistogramCount);

KeyedHistogramMapType gKeyedHistograms;

bool gCorruptHistograms[mozilla::Telemetry::HistogramCount];

// This is for gHistograms, gHistogramStringTable
#include "TelemetryHistogramData.inc"

AddonMapType gAddonMap;

// The singleton StatisticsRecorder object for this process.
base::StatisticsRecorder* gStatisticsRecorder = nullptr;

// For batching and sending child process accumulations to the parent
nsITimer* gIPCTimer = nullptr;
mozilla::Atomic<bool, mozilla::Relaxed> gIPCTimerArmed(false);
mozilla::Atomic<bool, mozilla::Relaxed> gIPCTimerArming(false);
StaticAutoPtr<nsTArray<Accumulation>> gAccumulations;
StaticAutoPtr<nsTArray<KeyedAccumulation>> gKeyedAccumulations;

// Has XPCOM started shutting down?
mozilla::Atomic<bool, mozilla::Relaxed>  gShuttingDown(false);

} // namespace


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE CONSTANTS

namespace {

// List of histogram IDs which should have recording disabled initially.
const mozilla::Telemetry::ID kRecordingInitiallyDisabledIDs[] = {
  mozilla::Telemetry::FX_REFRESH_DRIVER_SYNC_SCROLL_FRAME_DELAY_MS,

  // The array must not be empty. Leave these item here.
  mozilla::Telemetry::TELEMETRY_TEST_COUNT_INIT_NO_RECORD,
  mozilla::Telemetry::TELEMETRY_TEST_KEYED_COUNT_INIT_NO_RECORD
};

// Sending each remote accumulation immediately places undue strain on the
// IPC subsystem. Batch the remote accumulations for a period of time before
// sending them all at once. This value was chosen as a balance between data
// timeliness and performance (see bug 1218576)
const uint32_t kBatchTimeoutMs = 2000;

// To stop growing unbounded in memory while waiting for kBatchTimeoutMs to
// drain the g*Accumulations arrays, request an immediate flush if the arrays
// manage to reach this high water mark of elements.
const size_t kAccumulationsArrayHighWaterMark = 5 * 1024;

} // namespace


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE: Misc small helpers

namespace {

bool
internal_CanRecordBase() {
  return gCanRecordBase;
}

bool
internal_CanRecordExtended() {
  return gCanRecordExtended;
}

bool
internal_IsHistogramEnumId(mozilla::Telemetry::ID aID)
{
  static_assert(((mozilla::Telemetry::ID)-1 > 0), "ID should be unsigned.");
  return aID < mozilla::Telemetry::HistogramCount;
}

// Note: this is completely unrelated to mozilla::IsEmpty.
bool
internal_IsEmpty(const Histogram *h)
{
  Histogram::SampleSet ss;
  h->SnapshotSample(&ss);
  return ss.counts(0) == 0 && ss.sum() == 0;
}

bool
internal_IsExpired(const Histogram *histogram)
{
  return histogram->histogram_name() == EXPIRED_ID;
}

nsresult
internal_GetRegisteredHistogramIds(bool keyed, uint32_t dataset,
                                   uint32_t *aCount, char*** aHistograms)
{
  nsTArray<char*> collection;

  for (size_t i = 0; i < mozilla::ArrayLength(gHistograms); ++i) {
    const HistogramInfo& h = gHistograms[i];
    if (IsExpiredVersion(h.expiration()) ||
        h.keyed != keyed ||
        !IsInDataset(h.dataset, dataset)) {
      continue;
    }

    const char* id = h.id();
    const size_t len = strlen(id);
    collection.AppendElement(static_cast<char*>(nsMemory::Clone(id, len+1)));
  }

  const size_t bytes = collection.Length() * sizeof(char*);
  char** histograms = static_cast<char**>(moz_xmalloc(bytes));
  memcpy(histograms, collection.Elements(), bytes);
  *aHistograms = histograms;
  *aCount = collection.Length();

  return NS_OK;
}

const char *
HistogramInfo::id() const
{
  return &gHistogramStringTable[this->id_offset];
}

const char *
HistogramInfo::expiration() const
{
  return &gHistogramStringTable[this->expiration_offset];
}

nsresult
HistogramInfo::label_id(const char* label, uint32_t* labelId) const
{
  MOZ_ASSERT(label);
  MOZ_ASSERT(this->histogramType == nsITelemetry::HISTOGRAM_CATEGORICAL);
  if (this->histogramType != nsITelemetry::HISTOGRAM_CATEGORICAL) {
    return NS_ERROR_FAILURE;
  }

  for (uint32_t i = 0; i < this->label_count; ++i) {
    // gHistogramLabelTable contains the indices of the label strings in the
    // gHistogramStringTable.
    // They are stored in-order and consecutively, from the offset label_index
    // to (label_index + label_count).
    uint32_t string_offset = gHistogramLabelTable[this->label_index + i];
    const char* const str = &gHistogramStringTable[string_offset];
    if (::strcmp(label, str) == 0) {
      *labelId = i;
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

struct TelemetryShutdownObserver : public nsIObserver
{
  NS_DECL_ISUPPORTS

  NS_IMETHOD Observe(nsISupports*, const char* aTopic, const char16_t*) override
  {
    if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) != 0) {
      return NS_OK;
    }
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    }
    gShuttingDown = true;
    return NS_OK;
  }
private:
  virtual ~TelemetryShutdownObserver() { }
};
NS_IMPL_ISUPPORTS(TelemetryShutdownObserver, nsIObserver)

} // namespace


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE: Histogram Get, Add, Clone, Clear functions

namespace {

nsresult
internal_CheckHistogramArguments(uint32_t histogramType,
                                 uint32_t min, uint32_t max,
                                 uint32_t bucketCount, bool haveOptArgs)
{
  if (histogramType != nsITelemetry::HISTOGRAM_BOOLEAN
      && histogramType != nsITelemetry::HISTOGRAM_FLAG
      && histogramType != nsITelemetry::HISTOGRAM_COUNT) {
    // The min, max & bucketCount arguments are not optional for this type.
    if (!haveOptArgs)
      return NS_ERROR_ILLEGAL_VALUE;

    // Sanity checks for histogram parameters.
    if (min >= max)
      return NS_ERROR_ILLEGAL_VALUE;

    if (bucketCount <= 2)
      return NS_ERROR_ILLEGAL_VALUE;

    if (min < 1)
      return NS_ERROR_ILLEGAL_VALUE;
  }

  return NS_OK;
}

/*
 * min, max & bucketCount are optional for boolean, flag & count histograms.
 * haveOptArgs has to be set if the caller provides them.
 */
nsresult
internal_HistogramGet(const char *name, const char *expiration,
                      uint32_t histogramType, uint32_t min, uint32_t max,
                      uint32_t bucketCount, bool haveOptArgs,
                      Histogram **result)
{
  nsresult rv = internal_CheckHistogramArguments(histogramType, min, max,
                                                 bucketCount, haveOptArgs);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (IsExpiredVersion(expiration)) {
    name = EXPIRED_ID;
    min = 1;
    max = 2;
    bucketCount = 3;
    histogramType = nsITelemetry::HISTOGRAM_LINEAR;
  }

  switch (histogramType) {
  case nsITelemetry::HISTOGRAM_EXPONENTIAL:
    *result = Histogram::FactoryGet(name, min, max, bucketCount, Histogram::kUmaTargetedHistogramFlag);
    break;
  case nsITelemetry::HISTOGRAM_LINEAR:
  case nsITelemetry::HISTOGRAM_CATEGORICAL:
    *result = LinearHistogram::FactoryGet(name, min, max, bucketCount, Histogram::kUmaTargetedHistogramFlag);
    break;
  case nsITelemetry::HISTOGRAM_BOOLEAN:
    *result = BooleanHistogram::FactoryGet(name, Histogram::kUmaTargetedHistogramFlag);
    break;
  case nsITelemetry::HISTOGRAM_FLAG:
    *result = FlagHistogram::FactoryGet(name, Histogram::kUmaTargetedHistogramFlag);
    break;
  case nsITelemetry::HISTOGRAM_COUNT:
    *result = CountHistogram::FactoryGet(name, Histogram::kUmaTargetedHistogramFlag);
    break;
  default:
    NS_ASSERTION(false, "Invalid histogram type");
    return NS_ERROR_INVALID_ARG;
  }
  return NS_OK;
}

// Read the process type from the given histogram name. The process type, if
// one exists, is embedded in a suffix.
GeckoProcessType
GetProcessFromName(const nsACString& aString)
{
  if (StringEndsWith(aString, NS_LITERAL_CSTRING(CONTENT_HISTOGRAM_SUFFIX))) {
    return GeckoProcessType_Content;
  }
  if (StringEndsWith(aString, NS_LITERAL_CSTRING(GPU_HISTOGRAM_SUFFIX))) {
    return GeckoProcessType_GPU;
  }
  return GeckoProcessType_Default;
}

const char*
SuffixForProcessType(GeckoProcessType aProcessType)
{
  switch (aProcessType) {
    case GeckoProcessType_Default:
      return nullptr;
    case GeckoProcessType_Content:
      return CONTENT_HISTOGRAM_SUFFIX;
    case GeckoProcessType_GPU:
      return GPU_HISTOGRAM_SUFFIX;
    default:
      MOZ_ASSERT_UNREACHABLE("unknown process type");
      return nullptr;
  }
}

CharPtrEntryType*
internal_GetHistogramMapEntry(const char* aName)
{
  nsDependentCString name(aName);
  GeckoProcessType process = GetProcessFromName(name);
  const char* suffix = SuffixForProcessType(process);
  if (!suffix) {
    return gHistogramMap.GetEntry(aName);
  }

  auto root = Substring(name, 0, name.Length() - strlen(suffix));
  return gHistogramMap.GetEntry(PromiseFlatCString(root).get());
}

nsresult
internal_GetHistogramEnumId(const char *name, mozilla::Telemetry::ID *id)
{
  if (!gInitDone) {
    return NS_ERROR_FAILURE;
  }

  CharPtrEntryType *entry = internal_GetHistogramMapEntry(name);
  if (!entry) {
    return NS_ERROR_INVALID_ARG;
  }
  *id = entry->mData;
  return NS_OK;
}

// O(1) histogram lookup by numeric id
nsresult
internal_GetHistogramByEnumId(mozilla::Telemetry::ID id, Histogram **ret, GeckoProcessType aProcessType)
{
  static Histogram* knownHistograms[mozilla::Telemetry::HistogramCount] = {0};
  static Histogram* knownContentHistograms[mozilla::Telemetry::HistogramCount] = {0};
  static Histogram* knownGPUHistograms[mozilla::Telemetry::HistogramCount] = {0};

  Histogram** knownList = nullptr;

  switch (aProcessType) {
  case GeckoProcessType_Default:
    knownList = knownHistograms;
    break;
  case GeckoProcessType_Content:
    knownList = knownContentHistograms;
    break;
  case GeckoProcessType_GPU:
    knownList = knownGPUHistograms;
    break;
  default:
    MOZ_ASSERT_UNREACHABLE("unknown process type");
    return NS_ERROR_FAILURE;
  }

  Histogram* h = knownList[id];
  if (h) {
    *ret = h;
    return NS_OK;
  }

  const HistogramInfo &p = gHistograms[id];
  if (p.keyed) {
    return NS_ERROR_FAILURE;
  }

  nsCString histogramName;
  histogramName.Append(p.id());
  if (const char* suffix = SuffixForProcessType(aProcessType)) {
    histogramName.AppendASCII(suffix);
  }

  nsresult rv = internal_HistogramGet(histogramName.get(), p.expiration(),
                                      p.histogramType, p.min, p.max,
                                      p.bucketCount, true, &h);
  if (NS_FAILED(rv))
    return rv;

#ifdef DEBUG
  // Check that the C++ Histogram code computes the same ranges as the
  // Python histogram code.
  if (!IsExpiredVersion(p.expiration())) {
    const struct bounds &b = gBucketLowerBoundIndex[id];
    if (b.length != 0) {
      MOZ_ASSERT(size_t(b.length) == h->bucket_count(),
                 "C++/Python bucket # mismatch");
      for (int i = 0; i < b.length; ++i) {
        MOZ_ASSERT(gBucketLowerBounds[b.offset + i] == h->ranges(i),
                   "C++/Python bucket mismatch");
      }
    }
  }
#endif

  knownList[id] = h;
  *ret = h;
  return NS_OK;
}

nsresult
internal_GetHistogramByName(const nsACString &name, Histogram **ret)
{
  mozilla::Telemetry::ID id;
  nsresult rv
    = internal_GetHistogramEnumId(PromiseFlatCString(name).get(), &id);
  if (NS_FAILED(rv)) {
    return rv;
  }

  GeckoProcessType process = GetProcessFromName(name);
  rv = internal_GetHistogramByEnumId(id, ret, process);
  if (NS_FAILED(rv))
    return rv;

  return NS_OK;
}

/**
 * This clones a histogram |existing| with the id |existingId| to a
 * new histogram with the name |newName|.
 * For simplicity this is limited to registered histograms.
 */
Histogram*
internal_CloneHistogram(const nsACString& newName,
                        mozilla::Telemetry::ID existingId,
                        Histogram& existing)
{
  const HistogramInfo &info = gHistograms[existingId];
  Histogram *clone = nullptr;
  nsresult rv;

  rv = internal_HistogramGet(PromiseFlatCString(newName).get(),
                             info.expiration(),
                             info.histogramType, existing.declared_min(),
                             existing.declared_max(), existing.bucket_count(),
                             true, &clone);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  Histogram::SampleSet ss;
  existing.SnapshotSample(&ss);
  clone->AddSampleSet(ss);

  return clone;
}

/**
 * This clones a histogram with the id |existingId| to a new histogram
 * with the name |newName|.
 * For simplicity this is limited to registered histograms.
 */
Histogram*
internal_CloneHistogram(const nsACString& newName,
                        mozilla::Telemetry::ID existingId)
{
  Histogram *existing = nullptr;
  nsresult rv = internal_GetHistogramByEnumId(existingId, &existing, GeckoProcessType_Default);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return internal_CloneHistogram(newName, existingId, *existing);
}

#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)

GeckoProcessType
GetProcessFromName(const std::string& aString)
{
  nsDependentCString string(aString.c_str(), aString.length());
  return GetProcessFromName(string);
}

Histogram*
internal_GetSubsessionHistogram(Histogram& existing)
{
  mozilla::Telemetry::ID id;
  nsresult rv
    = internal_GetHistogramEnumId(existing.histogram_name().c_str(), &id);
  if (NS_FAILED(rv) || gHistograms[id].keyed) {
    return nullptr;
  }

  static Histogram* subsession[mozilla::Telemetry::HistogramCount] = {};
  static Histogram* subsessionContent[mozilla::Telemetry::HistogramCount] = {};
  static Histogram* subsessionGPU[mozilla::Telemetry::HistogramCount] = {};

  Histogram** cache = nullptr;

  GeckoProcessType process = GetProcessFromName(existing.histogram_name());
  switch (process) {
  case GeckoProcessType_Default:
    cache = subsession;
    break;
  case GeckoProcessType_Content:
    cache = subsessionContent;
    break;
  case GeckoProcessType_GPU:
    cache = subsessionGPU;
    break;
  default:
    MOZ_ASSERT_UNREACHABLE("unknown process type");
    return nullptr;
  }

  if (Histogram* cached = cache[id]) {
    return cached;
  }

  NS_NAMED_LITERAL_CSTRING(prefix, SUBSESSION_HISTOGRAM_PREFIX);
  nsDependentCString existingName(gHistograms[id].id());
  if (StringBeginsWith(existingName, prefix)) {
    return nullptr;
  }

  nsCString subsessionName(prefix);
  subsessionName.Append(existing.histogram_name().c_str());

  Histogram* clone = internal_CloneHistogram(subsessionName, id, existing);
  cache[id] = clone;
  return clone;
}
#endif

nsresult
internal_HistogramAdd(Histogram& histogram, int32_t value, uint32_t dataset)
{
  // Check if we are allowed to record the data.
  bool canRecordDataset = CanRecordDataset(dataset,
                                           internal_CanRecordBase(),
                                           internal_CanRecordExtended());
  if (!canRecordDataset || !histogram.IsRecordingEnabled()) {
    return NS_OK;
  }

#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
  if (Histogram* subsession = internal_GetSubsessionHistogram(histogram)) {
    subsession->Add(value);
  }
#endif

  // It is safe to add to the histogram now: the subsession histogram was already
  // cloned from this so we won't add the sample twice.
  histogram.Add(value);

  return NS_OK;
}

nsresult
internal_HistogramAdd(Histogram& histogram, int32_t value)
{
  uint32_t dataset = nsITelemetry::DATASET_RELEASE_CHANNEL_OPTIN;
  // We only really care about the dataset of the histogram if we are not recording
  // extended telemetry. Otherwise, we always record histogram data.
  if (!internal_CanRecordExtended()) {
    mozilla::Telemetry::ID id;
    nsresult rv
      = internal_GetHistogramEnumId(histogram.histogram_name().c_str(), &id);
    if (NS_FAILED(rv)) {
      // If we can't look up the dataset, it might be because the histogram was added
      // at runtime. Since we're not recording extended telemetry, bail out.
      return NS_OK;
    }
    dataset = gHistograms[id].dataset;
  }

  return internal_HistogramAdd(histogram, value, dataset);
}

void
internal_HistogramClear(Histogram& aHistogram, bool onlySubsession)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  if (!XRE_IsParentProcess()) {
    return;
  }
  if (!onlySubsession) {
    aHistogram.Clear();
  }

#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
  if (Histogram* subsession = internal_GetSubsessionHistogram(aHistogram)) {
    subsession->Clear();
  }
#endif
}

} // namespace


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE: Histogram corruption helpers

namespace {

void internal_Accumulate(mozilla::Telemetry::ID aHistogram, uint32_t aSample);

void
internal_IdentifyCorruptHistograms(StatisticsRecorder::Histograms &hs)
{
  for (HistogramIterator it = hs.begin(); it != hs.end(); ++it) {
    Histogram *h = *it;

    mozilla::Telemetry::ID id;
    nsresult rv = internal_GetHistogramEnumId(h->histogram_name().c_str(), &id);
    // This histogram isn't a static histogram, just ignore it.
    if (NS_FAILED(rv)) {
      continue;
    }

    if (gCorruptHistograms[id]) {
      continue;
    }

    Histogram::SampleSet ss;
    h->SnapshotSample(&ss);

    Histogram::Inconsistencies check = h->FindCorruption(ss);
    bool corrupt = (check != Histogram::NO_INCONSISTENCIES);

    if (corrupt) {
      mozilla::Telemetry::ID corruptID = mozilla::Telemetry::HistogramCount;
      if (check & Histogram::RANGE_CHECKSUM_ERROR) {
        corruptID = mozilla::Telemetry::RANGE_CHECKSUM_ERRORS;
      } else if (check & Histogram::BUCKET_ORDER_ERROR) {
        corruptID = mozilla::Telemetry::BUCKET_ORDER_ERRORS;
      } else if (check & Histogram::COUNT_HIGH_ERROR) {
        corruptID = mozilla::Telemetry::TOTAL_COUNT_HIGH_ERRORS;
      } else if (check & Histogram::COUNT_LOW_ERROR) {
        corruptID = mozilla::Telemetry::TOTAL_COUNT_LOW_ERRORS;
      }
      internal_Accumulate(corruptID, 1);
    }

    gCorruptHistograms[id] = corrupt;
  }
}

} // namespace


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE: Histogram reflection helpers

namespace {

bool
internal_FillRanges(JSContext *cx, JS::Handle<JSObject*> array, Histogram *h)
{
  JS::Rooted<JS::Value> range(cx);
  for (size_t i = 0; i < h->bucket_count(); i++) {
    range.setInt32(h->ranges(i));
    if (!JS_DefineElement(cx, array, i, range, JSPROP_ENUMERATE))
      return false;
  }
  return true;
}

enum reflectStatus
internal_ReflectHistogramAndSamples(JSContext *cx,
                                    JS::Handle<JSObject*> obj, Histogram *h,
                                    const Histogram::SampleSet &ss)
{
  // We don't want to reflect corrupt histograms.
  if (h->FindCorruption(ss) != Histogram::NO_INCONSISTENCIES) {
    return REFLECT_CORRUPT;
  }

  if (!(JS_DefineProperty(cx, obj, "min",
                          h->declared_min(), JSPROP_ENUMERATE)
        && JS_DefineProperty(cx, obj, "max",
                             h->declared_max(), JSPROP_ENUMERATE)
        && JS_DefineProperty(cx, obj, "histogram_type",
                             h->histogram_type(), JSPROP_ENUMERATE)
        && JS_DefineProperty(cx, obj, "sum",
                             double(ss.sum()), JSPROP_ENUMERATE))) {
    return REFLECT_FAILURE;
  }

  const size_t count = h->bucket_count();
  JS::Rooted<JSObject*> rarray(cx, JS_NewArrayObject(cx, count));
  if (!rarray) {
    return REFLECT_FAILURE;
  }
  if (!(internal_FillRanges(cx, rarray, h)
        && JS_DefineProperty(cx, obj, "ranges", rarray, JSPROP_ENUMERATE))) {
    return REFLECT_FAILURE;
  }

  JS::Rooted<JSObject*> counts_array(cx, JS_NewArrayObject(cx, count));
  if (!counts_array) {
    return REFLECT_FAILURE;
  }
  if (!JS_DefineProperty(cx, obj, "counts", counts_array, JSPROP_ENUMERATE)) {
    return REFLECT_FAILURE;
  }
  for (size_t i = 0; i < count; i++) {
    if (!JS_DefineElement(cx, counts_array, i,
                          ss.counts(i), JSPROP_ENUMERATE)) {
      return REFLECT_FAILURE;
    }
  }

  return REFLECT_OK;
}

enum reflectStatus
internal_ReflectHistogramSnapshot(JSContext *cx,
                                  JS::Handle<JSObject*> obj, Histogram *h)
{
  Histogram::SampleSet ss;
  h->SnapshotSample(&ss);
  return internal_ReflectHistogramAndSamples(cx, obj, h, ss);
}

bool
internal_ShouldReflectHistogram(Histogram *h)
{
  const char *name = h->histogram_name().c_str();
  mozilla::Telemetry::ID id;
  nsresult rv = internal_GetHistogramEnumId(name, &id);
  if (NS_FAILED(rv)) {
    // GetHistogramEnumId generally should not fail.  But a lookup
    // failure shouldn't prevent us from reflecting histograms into JS.
    //
    // However, these two histograms are created by Histogram itself for
    // tracking corruption.  We have our own histograms for that, so
    // ignore these two.
    if (strcmp(name, "Histogram.InconsistentCountHigh") == 0
        || strcmp(name, "Histogram.InconsistentCountLow") == 0) {
      return false;
    }
    return true;
  } else {
    return !gCorruptHistograms[id];
  }
}

} // namespace


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE: class KeyedHistogram

namespace {

class KeyedHistogram {
public:
  KeyedHistogram(const nsACString &name, const nsACString &expiration,
                 uint32_t histogramType, uint32_t min, uint32_t max,
                 uint32_t bucketCount, uint32_t dataset);
  nsresult GetHistogram(const nsCString& name, Histogram** histogram, bool subsession);
  Histogram* GetHistogram(const nsCString& name, bool subsession);
  uint32_t GetHistogramType() const { return mHistogramType; }
  nsresult GetDataset(uint32_t* dataset) const;
  nsresult GetJSKeys(JSContext* cx, JS::CallArgs& args);
  nsresult GetJSSnapshot(JSContext* cx, JS::Handle<JSObject*> obj,
                         bool subsession, bool clearSubsession);

  void SetRecordingEnabled(bool aEnabled) { mRecordingEnabled = aEnabled; };
  bool IsRecordingEnabled() const { return mRecordingEnabled; };

  nsresult Add(const nsCString& key, uint32_t aSample);
  void Clear(bool subsession);

  nsresult GetEnumId(mozilla::Telemetry::ID& id);

private:
  typedef nsBaseHashtableET<nsCStringHashKey, Histogram*> KeyedHistogramEntry;
  typedef AutoHashtable<KeyedHistogramEntry> KeyedHistogramMapType;
  KeyedHistogramMapType mHistogramMap;
#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
  KeyedHistogramMapType mSubsessionMap;
#endif

  static bool ReflectKeyedHistogram(KeyedHistogramEntry* entry,
                                    JSContext* cx,
                                    JS::Handle<JSObject*> obj);

  const nsCString mName;
  const nsCString mExpiration;
  const uint32_t mHistogramType;
  const uint32_t mMin;
  const uint32_t mMax;
  const uint32_t mBucketCount;
  const uint32_t mDataset;
  mozilla::Atomic<bool, mozilla::Relaxed> mRecordingEnabled;
};

KeyedHistogram::KeyedHistogram(const nsACString &name,
                               const nsACString &expiration,
                               uint32_t histogramType,
                               uint32_t min, uint32_t max,
                               uint32_t bucketCount, uint32_t dataset)
  : mHistogramMap()
#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
  , mSubsessionMap()
#endif
  , mName(name)
  , mExpiration(expiration)
  , mHistogramType(histogramType)
  , mMin(min)
  , mMax(max)
  , mBucketCount(bucketCount)
  , mDataset(dataset)
  , mRecordingEnabled(true)
{
}

nsresult
KeyedHistogram::GetHistogram(const nsCString& key, Histogram** histogram,
                             bool subsession)
{
#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
  KeyedHistogramMapType& map = subsession ? mSubsessionMap : mHistogramMap;
#else
  KeyedHistogramMapType& map = mHistogramMap;
#endif
  KeyedHistogramEntry* entry = map.GetEntry(key);
  if (entry) {
    *histogram = entry->mData;
    return NS_OK;
  }

  nsCString histogramName;
#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
  if (subsession) {
    histogramName.AppendLiteral(SUBSESSION_HISTOGRAM_PREFIX);
  }
#endif
  histogramName.Append(mName);
  histogramName.AppendLiteral(KEYED_HISTOGRAM_NAME_SEPARATOR);
  histogramName.Append(key);

  Histogram* h;
  nsresult rv = internal_HistogramGet(histogramName.get(), mExpiration.get(),
                                      mHistogramType, mMin, mMax, mBucketCount,
                                      true, &h);
  if (NS_FAILED(rv)) {
    return rv;
  }

  h->ClearFlags(Histogram::kUmaTargetedHistogramFlag);
  *histogram = h;

  entry = map.PutEntry(key);
  if (MOZ_UNLIKELY(!entry)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  entry->mData = h;
  return NS_OK;
}

Histogram*
KeyedHistogram::GetHistogram(const nsCString& key, bool subsession)
{
  Histogram* h = nullptr;
  if (NS_FAILED(GetHistogram(key, &h, subsession))) {
    return nullptr;
  }
  return h;
}

nsresult
KeyedHistogram::GetDataset(uint32_t* dataset) const
{
  MOZ_ASSERT(dataset);
  *dataset = mDataset;
  return NS_OK;
}

nsresult
KeyedHistogram::Add(const nsCString& key, uint32_t sample)
{
  bool canRecordDataset = CanRecordDataset(mDataset,
                                           internal_CanRecordBase(),
                                           internal_CanRecordExtended());
  if (!canRecordDataset) {
    return NS_OK;
  }

  Histogram* histogram = GetHistogram(key, false);
  MOZ_ASSERT(histogram);
  if (!histogram) {
    return NS_ERROR_FAILURE;
  }
#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
  Histogram* subsession = GetHistogram(key, true);
  MOZ_ASSERT(subsession);
  if (!subsession) {
    return NS_ERROR_FAILURE;
  }
#endif

  if (!IsRecordingEnabled()) {
    return NS_OK;
  }

  histogram->Add(sample);
#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
  subsession->Add(sample);
#endif
  return NS_OK;
}

void
KeyedHistogram::Clear(bool onlySubsession)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  if (!XRE_IsParentProcess()) {
    return;
  }
#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
  for (auto iter = mSubsessionMap.Iter(); !iter.Done(); iter.Next()) {
    iter.Get()->mData->Clear();
  }
  mSubsessionMap.Clear();
  if (onlySubsession) {
    return;
  }
#endif

  for (auto iter = mHistogramMap.Iter(); !iter.Done(); iter.Next()) {
    iter.Get()->mData->Clear();
  }
  mHistogramMap.Clear();
}

nsresult
KeyedHistogram::GetJSKeys(JSContext* cx, JS::CallArgs& args)
{
  JS::AutoValueVector keys(cx);
  if (!keys.reserve(mHistogramMap.Count())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (auto iter = mHistogramMap.Iter(); !iter.Done(); iter.Next()) {
    JS::RootedValue jsKey(cx);
    const NS_ConvertUTF8toUTF16 key(iter.Get()->GetKey());
    jsKey.setString(JS_NewUCStringCopyN(cx, key.Data(), key.Length()));
    if (!keys.append(jsKey)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  JS::RootedObject jsKeys(cx, JS_NewArrayObject(cx, keys));
  if (!jsKeys) {
    return NS_ERROR_FAILURE;
  }

  args.rval().setObject(*jsKeys);
  return NS_OK;
}

bool
KeyedHistogram::ReflectKeyedHistogram(KeyedHistogramEntry* entry,
                                      JSContext* cx, JS::Handle<JSObject*> obj)
{
  JS::RootedObject histogramSnapshot(cx, JS_NewPlainObject(cx));
  if (!histogramSnapshot) {
    return false;
  }

  if (internal_ReflectHistogramSnapshot(cx, histogramSnapshot,
                                        entry->mData) != REFLECT_OK) {
    return false;
  }

  const NS_ConvertUTF8toUTF16 key(entry->GetKey());
  if (!JS_DefineUCProperty(cx, obj, key.Data(), key.Length(),
                           histogramSnapshot, JSPROP_ENUMERATE)) {
    return false;
  }

  return true;
}

nsresult
KeyedHistogram::GetJSSnapshot(JSContext* cx, JS::Handle<JSObject*> obj,
                              bool subsession, bool clearSubsession)
{
#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
  KeyedHistogramMapType& map = subsession ? mSubsessionMap : mHistogramMap;
#else
  KeyedHistogramMapType& map = mHistogramMap;
#endif
  if (!map.ReflectIntoJS(&KeyedHistogram::ReflectKeyedHistogram, cx, obj)) {
    return NS_ERROR_FAILURE;
  }

#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
  if (subsession && clearSubsession) {
    Clear(true);
  }
#endif

  return NS_OK;
}

nsresult
KeyedHistogram::GetEnumId(mozilla::Telemetry::ID& id)
{
  return internal_GetHistogramEnumId(mName.get(), &id);
}

} // namespace


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE: KeyedHistogram helpers

namespace {

KeyedHistogram*
internal_GetKeyedHistogramById(const nsACString &name)
{
  if (!gInitDone) {
    return nullptr;
  }

  KeyedHistogram* keyed = nullptr;
  gKeyedHistograms.Get(name, &keyed);
  return keyed;
}

} // namespace


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE: functions related to addon histograms

namespace {

// Compute the name to pass into Histogram for the addon histogram
// 'name' from the addon 'id'.  We can't use 'name' directly because it
// might conflict with other histograms in other addons or even with our
// own.
void
internal_AddonHistogramName(const nsACString &id, const nsACString &name,
                            nsACString &ret)
{
  ret.Append(id);
  ret.Append(':');
  ret.Append(name);
}

bool
internal_CreateHistogramForAddon(const nsACString &name,
                                 AddonHistogramInfo &info)
{
  Histogram *h;
  nsresult rv = internal_HistogramGet(PromiseFlatCString(name).get(), "never",
                                      info.histogramType, info.min, info.max,
                                      info.bucketCount, true, &h);
  if (NS_FAILED(rv)) {
    return false;
  }
  // Don't let this histogram be reported via the normal means
  // (e.g. Telemetry.registeredHistograms); we'll make it available in
  // other ways.
  h->ClearFlags(Histogram::kUmaTargetedHistogramFlag);
  info.h = h;
  return true;
}

bool
internal_AddonHistogramReflector(AddonHistogramEntryType *entry,
                                 JSContext *cx, JS::Handle<JSObject*> obj)
{
  AddonHistogramInfo &info = entry->mData;

  // Never even accessed the histogram.
  if (!info.h) {
    // Have to force creation of HISTOGRAM_FLAG histograms.
    if (info.histogramType != nsITelemetry::HISTOGRAM_FLAG)
      return true;

    if (!internal_CreateHistogramForAddon(entry->GetKey(), info)) {
      return false;
    }
  }

  if (internal_IsEmpty(info.h)) {
    return true;
  }

  JS::Rooted<JSObject*> snapshot(cx, JS_NewPlainObject(cx));
  if (!snapshot) {
    // Just consider this to be skippable.
    return true;
  }
  switch (internal_ReflectHistogramSnapshot(cx, snapshot, info.h)) {
  case REFLECT_FAILURE:
  case REFLECT_CORRUPT:
    return false;
  case REFLECT_OK:
    const nsACString &histogramName = entry->GetKey();
    if (!JS_DefineProperty(cx, obj, PromiseFlatCString(histogramName).get(),
                           snapshot, JSPROP_ENUMERATE)) {
      return false;
    }
    break;
  }
  return true;
}

bool
internal_AddonReflector(AddonEntryType *entry, JSContext *cx,
                        JS::Handle<JSObject*> obj)
{
  const nsACString &addonId = entry->GetKey();
  JS::Rooted<JSObject*> subobj(cx, JS_NewPlainObject(cx));
  if (!subobj) {
    return false;
  }

  AddonHistogramMapType *map = entry->mData;
  if (!(map->ReflectIntoJS(internal_AddonHistogramReflector, cx, subobj)
        && JS_DefineProperty(cx, obj, PromiseFlatCString(addonId).get(),
                             subobj, JSPROP_ENUMERATE))) {
    return false;
  }
  return true;
}

} // namespace


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE: thread-unsafe helpers for the external interface

// This is a StaticMutex rather than a plain Mutex (1) so that
// it gets initialised in a thread-safe manner the first time
// it is used, and (2) because it is never de-initialised, and
// a normal Mutex would show up as a leak in BloatView.  StaticMutex
// also has the "OffTheBooks" property, so it won't show as a leak
// in BloatView.
static StaticMutex gTelemetryHistogramMutex;

namespace {

void
internal_SetHistogramRecordingEnabled(mozilla::Telemetry::ID aID, bool aEnabled)
{
  if (!internal_IsHistogramEnumId(aID)) {
    MOZ_ASSERT(false, "Telemetry::SetHistogramRecordingEnabled(...) must be used with an enum id");
    return;
  }

  if (gHistograms[aID].keyed) {
    const nsDependentCString id(gHistograms[aID].id());
    KeyedHistogram* keyed = internal_GetKeyedHistogramById(id);
    if (keyed) {
      keyed->SetRecordingEnabled(aEnabled);
      return;
    }
  } else {
    Histogram *h;
    nsresult rv = internal_GetHistogramByEnumId(aID, &h, GeckoProcessType_Default);
    if (NS_SUCCEEDED(rv)) {
      h->SetRecordingEnabled(aEnabled);
      return;
    }
  }

  MOZ_ASSERT(false, "Telemetry::SetHistogramRecordingEnabled(...) id not found");
}

void internal_armIPCTimerMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
  gIPCTimerArming = false;
  if (gIPCTimerArmed) {
    return;
  }
  if (!gIPCTimer) {
    CallCreateInstance(NS_TIMER_CONTRACTID, &gIPCTimer);
  }
  if (gIPCTimer) {
    gIPCTimer->InitWithFuncCallback(TelemetryHistogram::IPCTimerFired,
                                    nullptr, kBatchTimeoutMs,
                                    nsITimer::TYPE_ONE_SHOT);
    gIPCTimerArmed = true;
  }
}

void internal_armIPCTimer()
{
  if (gIPCTimerArmed || gIPCTimerArming) {
    return;
  }
  gIPCTimerArming = true;
  if (NS_IsMainThread()) {
    internal_armIPCTimerMainThread();
  } else if (!gShuttingDown) {
    NS_DispatchToMainThread(NS_NewRunnableFunction([]() -> void {
      StaticMutexAutoLock locker(gTelemetryHistogramMutex);
      internal_armIPCTimerMainThread();
    }));
  }
}

bool
internal_RemoteAccumulate(mozilla::Telemetry::ID aId, uint32_t aSample)
{
  if (XRE_IsParentProcess()) {
    return false;
  }
  if (!gAccumulations) {
    gAccumulations = new nsTArray<Accumulation>();
  }
  if (gAccumulations->Length() == kAccumulationsArrayHighWaterMark) {
    NS_DispatchToMainThread(NS_NewRunnableFunction([]() -> void {
      TelemetryHistogram::IPCTimerFired(nullptr, nullptr);
    }));
  }
  gAccumulations->AppendElement(Accumulation{aId, aSample});
  internal_armIPCTimer();
  return true;
}

bool
internal_RemoteAccumulate(mozilla::Telemetry::ID aId,
                    const nsCString& aKey, uint32_t aSample)
{
  if (XRE_IsParentProcess()) {
    return false;
  }
  if (!gKeyedAccumulations) {
    gKeyedAccumulations = new nsTArray<KeyedAccumulation>();
  }
  if (gKeyedAccumulations->Length() == kAccumulationsArrayHighWaterMark) {
    NS_DispatchToMainThread(NS_NewRunnableFunction([]() -> void {
      TelemetryHistogram::IPCTimerFired(nullptr, nullptr);
    }));
  }
  gKeyedAccumulations->AppendElement(KeyedAccumulation{aId, aSample, aKey});
  internal_armIPCTimer();
  return true;
}

void internal_Accumulate(mozilla::Telemetry::ID aHistogram, uint32_t aSample)
{
  bool isValid = internal_IsHistogramEnumId(aHistogram);
  MOZ_ASSERT(isValid, "Accumulation using invalid id");
  if (!internal_CanRecordBase() || !isValid ||
      internal_RemoteAccumulate(aHistogram, aSample)) {
    return;
  }
  Histogram *h;
  nsresult rv = internal_GetHistogramByEnumId(aHistogram, &h, GeckoProcessType_Default);
  if (NS_SUCCEEDED(rv)) {
    internal_HistogramAdd(*h, aSample, gHistograms[aHistogram].dataset);
  }
}

void
internal_Accumulate(mozilla::Telemetry::ID aID,
                    const nsCString& aKey, uint32_t aSample)
{
  bool isValid = internal_IsHistogramEnumId(aID);
  MOZ_ASSERT(isValid, "Child keyed telemetry accumulation using invalid id");
  if (!gInitDone || !internal_CanRecordBase() || !isValid ||
      internal_RemoteAccumulate(aID, aKey, aSample)) {
    return;
  }
  const HistogramInfo& th = gHistograms[aID];
  KeyedHistogram* keyed
     = internal_GetKeyedHistogramById(nsDependentCString(th.id()));
  MOZ_ASSERT(keyed);
  keyed->Add(aKey, aSample);
}

void
internal_Accumulate(Histogram& aHistogram, uint32_t aSample)
{
  if (XRE_IsParentProcess()) {
    internal_HistogramAdd(aHistogram, aSample);
    return;
  }

  mozilla::Telemetry::ID id;
  nsresult rv = internal_GetHistogramEnumId(aHistogram.histogram_name().c_str(), &id);
  if (NS_SUCCEEDED(rv)) {
    internal_RemoteAccumulate(id, aSample);
  }
}

void
internal_Accumulate(KeyedHistogram& aKeyed,
                    const nsCString& aKey, uint32_t aSample)
{
  if (XRE_IsParentProcess()) {
    aKeyed.Add(aKey, aSample);
    return;
  }

  mozilla::Telemetry::ID id;
  if (NS_SUCCEEDED(aKeyed.GetEnumId(id))) {
    internal_RemoteAccumulate(id, aKey, aSample);
  }
}

void
internal_AccumulateChild(GeckoProcessType aProcessType, mozilla::Telemetry::ID aId, uint32_t aSample)
{
  if (!internal_CanRecordBase()) {
    return;
  }
  Histogram* h;
  nsresult rv = internal_GetHistogramByEnumId(aId, &h, aProcessType);
  if (NS_SUCCEEDED(rv)) {
    internal_HistogramAdd(*h, aSample, gHistograms[aId].dataset);
  } else {
    NS_WARNING("NS_FAILED GetHistogramByEnumId for CHILD");
  }
}

void
internal_AccumulateChildKeyed(GeckoProcessType aProcessType, mozilla::Telemetry::ID aId,
                              const nsCString& aKey, uint32_t aSample)
{
  if (!gInitDone || !internal_CanRecordBase()) {
    return;
  }

  const char* suffix = SuffixForProcessType(aProcessType);
  if (!suffix) {
    MOZ_ASSERT_UNREACHABLE("suffix should not be null");
    return;
  }

  const HistogramInfo& th = gHistograms[aId];

  nsCString id;
  id.Append(th.id());
  id.AppendASCII(suffix);

  KeyedHistogram* keyed = internal_GetKeyedHistogramById(id);
  MOZ_ASSERT(keyed);
  keyed->Add(aKey, aSample);
}

} // namespace


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE: JSHistogram_* functions

// NOTE: the functions in this section:
//
//   internal_JSHistogram_Add
//   internal_JSHistogram_Snapshot
//   internal_JSHistogram_Clear
//   internal_JSHistogram_Dataset
//   internal_WrapAndReturnHistogram
//
// all run without protection from |gTelemetryHistogramMutex|.  If they
// held |gTelemetryHistogramMutex|, there would be the possibility of
// deadlock because the JS_ calls that they make may call back into the
// TelemetryHistogram interface, hence trying to re-acquire the mutex.
//
// This means that these functions potentially race against threads, but
// that seems preferable to risking deadlock.

namespace {

bool
internal_JSHistogram_Add(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JSObject *obj = JS_THIS_OBJECT(cx, vp);
  MOZ_ASSERT(obj);
  if (!obj) {
    return false;
  }

  Histogram *h = static_cast<Histogram*>(JS_GetPrivate(obj));
  MOZ_ASSERT(h);
  Histogram::ClassType type = h->histogram_type();

  JS::CallArgs args = CallArgsFromVp(argc, vp);

  if (!internal_CanRecordBase()) {
    return true;
  }

  uint32_t value = 0;
  mozilla::Telemetry::ID id;
  if ((type == base::CountHistogram::COUNT_HISTOGRAM) && (args.length() == 0)) {
    // If we don't have an argument for the count histogram, assume an increment of 1.
    // Otherwise, make sure to run some sanity checks on the argument.
    value = 1;
  } else if (type == base::LinearHistogram::LINEAR_HISTOGRAM &&
      (args.length() > 0) && args[0].isString() &&
      NS_SUCCEEDED(internal_GetHistogramEnumId(h->histogram_name().c_str(), &id)) &&
      gHistograms[id].histogramType == nsITelemetry::HISTOGRAM_CATEGORICAL) {
    // For categorical histograms we allow passing a string argument that specifies the label.
    nsAutoJSString label;
    if (!label.init(cx, args[0])) {
      JS_ReportErrorASCII(cx, "Invalid string parameter");
      return false;
    }

    nsresult rv = gHistograms[id].label_id(NS_ConvertUTF16toUTF8(label).get(), &value);
    if (NS_FAILED(rv)) {
      JS_ReportErrorASCII(cx, "Unknown label for categorical histogram");
      return false;
    }
  } else {
    // All other accumulations expect one numerical argument.
    if (!args.length()) {
      JS_ReportErrorASCII(cx, "Expected one argument");
      return false;
    }

    if (!(args[0].isNumber() || args[0].isBoolean())) {
      JS_ReportErrorASCII(cx, "Not a number");
      return false;
    }

    if (!JS::ToUint32(cx, args[0], &value)) {
      JS_ReportErrorASCII(cx, "Failed to convert argument");
      return false;
    }
  }

  {
    StaticMutexAutoLock locker(gTelemetryHistogramMutex);
    internal_Accumulate(*h, value);
  }
  return true;
}

bool
internal_JSHistogram_Snapshot(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  JSObject *obj = JS_THIS_OBJECT(cx, vp);
  if (!obj) {
    return false;
  }

  Histogram *h = static_cast<Histogram*>(JS_GetPrivate(obj));
  JS::Rooted<JSObject*> snapshot(cx, JS_NewPlainObject(cx));
  if (!snapshot)
    return false;

  switch (internal_ReflectHistogramSnapshot(cx, snapshot, h)) {
  case REFLECT_FAILURE:
    return false;
  case REFLECT_CORRUPT:
    JS_ReportErrorASCII(cx, "Histogram is corrupt");
    return false;
  case REFLECT_OK:
    args.rval().setObject(*snapshot);
    return true;
  default:
    MOZ_CRASH("unhandled reflection status");
  }
}

bool
internal_JSHistogram_Clear(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JSObject *obj = JS_THIS_OBJECT(cx, vp);
  if (!obj) {
    return false;
  }

  bool onlySubsession = false;
#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

  if (args.length() >= 1) {
    if (!args[0].isBoolean()) {
      JS_ReportErrorASCII(cx, "Not a boolean");
      return false;
    }

    onlySubsession = JS::ToBoolean(args[0]);
  }
#endif

  Histogram *h = static_cast<Histogram*>(JS_GetPrivate(obj));
  MOZ_ASSERT(h);
  if (h) {
    internal_HistogramClear(*h, onlySubsession);
  }

  return true;
}

bool
internal_JSHistogram_Dataset(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  JSObject *obj = JS_THIS_OBJECT(cx, vp);
  if (!obj) {
    return false;
  }

  Histogram *h = static_cast<Histogram*>(JS_GetPrivate(obj));
  mozilla::Telemetry::ID id;
  nsresult rv = internal_GetHistogramEnumId(h->histogram_name().c_str(), &id);
  if (NS_SUCCEEDED(rv)) {
    args.rval().setNumber(gHistograms[id].dataset);
    return true;
  }

  return false;
}

// NOTE: Runs without protection from |gTelemetryHistogramMutex|.
// See comment at the top of this section.
nsresult
internal_WrapAndReturnHistogram(Histogram *h, JSContext *cx,
                                JS::MutableHandle<JS::Value> ret)
{
  static const JSClass JSHistogram_class = {
    "JSHistogram",  /* name */
    JSCLASS_HAS_PRIVATE  /* flags */
  };

  JS::Rooted<JSObject*> obj(cx, JS_NewObject(cx, &JSHistogram_class));
  if (!obj)
    return NS_ERROR_FAILURE;
  // The 4 functions that are wrapped up here are eventually called
  // by the same thread that runs this function.
  if (!(JS_DefineFunction(cx, obj, "add", internal_JSHistogram_Add, 1, 0)
        && JS_DefineFunction(cx, obj, "snapshot",
                             internal_JSHistogram_Snapshot, 0, 0)
        && JS_DefineFunction(cx, obj, "clear", internal_JSHistogram_Clear, 0, 0)
        && JS_DefineFunction(cx, obj, "dataset",
                             internal_JSHistogram_Dataset, 0, 0))) {
    return NS_ERROR_FAILURE;
  }
  JS_SetPrivate(obj, h);
  ret.setObject(*obj);
  return NS_OK;
}

} // namespace


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE: JSKeyedHistogram_* functions

// NOTE: the functions in this section:
//
//   internal_KeyedHistogram_SnapshotImpl
//   internal_JSKeyedHistogram_Add
//   internal_JSKeyedHistogram_Keys
//   internal_JSKeyedHistogram_Snapshot
//   internal_JSKeyedHistogram_SubsessionSnapshot
//   internal_JSKeyedHistogram_SnapshotSubsessionAndClear
//   internal_JSKeyedHistogram_Clear
//   internal_JSKeyedHistogram_Dataset
//   internal_WrapAndReturnKeyedHistogram
//
// Same comments as above, at the JSHistogram_* section, regarding
// deadlock avoidance, apply.

namespace {

bool
internal_KeyedHistogram_SnapshotImpl(JSContext *cx, unsigned argc,
                                     JS::Value *vp,
                                     bool subsession, bool clearSubsession)
{
  JSObject *obj = JS_THIS_OBJECT(cx, vp);
  if (!obj) {
    return false;
  }

  KeyedHistogram* keyed = static_cast<KeyedHistogram*>(JS_GetPrivate(obj));
  if (!keyed) {
    return false;
  }

  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

  if (args.length() == 0) {
    JS::RootedObject snapshot(cx, JS_NewPlainObject(cx));
    if (!snapshot) {
      JS_ReportErrorASCII(cx, "Failed to create object");
      return false;
    }

    if (!NS_SUCCEEDED(keyed->GetJSSnapshot(cx, snapshot, subsession, clearSubsession))) {
      JS_ReportErrorASCII(cx, "Failed to reflect keyed histograms");
      return false;
    }

    args.rval().setObject(*snapshot);
    return true;
  }

  nsAutoJSString key;
  if (!args[0].isString() || !key.init(cx, args[0])) {
    JS_ReportErrorASCII(cx, "Not a string");
    return false;
  }

  Histogram* h = nullptr;
  nsresult rv = keyed->GetHistogram(NS_ConvertUTF16toUTF8(key), &h, subsession);
  if (NS_FAILED(rv)) {
    JS_ReportErrorASCII(cx, "Failed to get histogram");
    return false;
  }

  JS::RootedObject snapshot(cx, JS_NewPlainObject(cx));
  if (!snapshot) {
    return false;
  }

  switch (internal_ReflectHistogramSnapshot(cx, snapshot, h)) {
  case REFLECT_FAILURE:
    return false;
  case REFLECT_CORRUPT:
    JS_ReportErrorASCII(cx, "Histogram is corrupt");
    return false;
  case REFLECT_OK:
    args.rval().setObject(*snapshot);
    return true;
  default:
    MOZ_CRASH("unhandled reflection status");
  }
}

bool
internal_JSKeyedHistogram_Add(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JSObject *obj = JS_THIS_OBJECT(cx, vp);
  if (!obj) {
    return false;
  }

  KeyedHistogram* keyed = static_cast<KeyedHistogram*>(JS_GetPrivate(obj));
  if (!keyed) {
    return false;
  }

  JS::CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() < 1) {
    JS_ReportErrorASCII(cx, "Expected one argument");
    return false;
  }

  nsAutoJSString key;
  if (!args[0].isString() || !key.init(cx, args[0])) {
    JS_ReportErrorASCII(cx, "Not a string");
    return false;
  }

  const uint32_t type = keyed->GetHistogramType();

  // If we don't have an argument for the count histogram, assume an increment of 1.
  // Otherwise, make sure to run some sanity checks on the argument.
  int32_t value = 1;
  if ((type != base::CountHistogram::COUNT_HISTOGRAM) || (args.length() == 2)) {
    if (args.length() < 2) {
      JS_ReportErrorASCII(cx, "Expected two arguments for this histogram type");
      return false;
    }

    if (!(args[1].isNumber() || args[1].isBoolean())) {
      JS_ReportErrorASCII(cx, "Not a number");
      return false;
    }

    if (!JS::ToInt32(cx, args[1], &value)) {
      return false;
    }
  }

  {
    StaticMutexAutoLock locker(gTelemetryHistogramMutex);
    internal_Accumulate(*keyed, NS_ConvertUTF16toUTF8(key), value);
  }
  return true;
}

bool
internal_JSKeyedHistogram_Keys(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JSObject *obj = JS_THIS_OBJECT(cx, vp);
  if (!obj) {
    return false;
  }

  KeyedHistogram* keyed = static_cast<KeyedHistogram*>(JS_GetPrivate(obj));
  if (!keyed) {
    return false;
  }

  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  return NS_SUCCEEDED(keyed->GetJSKeys(cx, args));
}

bool
internal_JSKeyedHistogram_Snapshot(JSContext *cx, unsigned argc, JS::Value *vp)
{
  return internal_KeyedHistogram_SnapshotImpl(cx, argc, vp, false, false);
}

#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
bool
internal_JSKeyedHistogram_SubsessionSnapshot(JSContext *cx,
                                             unsigned argc, JS::Value *vp)
{
  return internal_KeyedHistogram_SnapshotImpl(cx, argc, vp, true, false);
}
#endif

#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
bool
internal_JSKeyedHistogram_SnapshotSubsessionAndClear(JSContext *cx,
                                                     unsigned argc,
                                                     JS::Value *vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  if (args.length() != 0) {
    JS_ReportErrorASCII(cx, "No key arguments supported for snapshotSubsessionAndClear");
  }

  return internal_KeyedHistogram_SnapshotImpl(cx, argc, vp, true, true);
}
#endif

bool
internal_JSKeyedHistogram_Clear(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JSObject *obj = JS_THIS_OBJECT(cx, vp);
  if (!obj) {
    return false;
  }

  KeyedHistogram* keyed = static_cast<KeyedHistogram*>(JS_GetPrivate(obj));
  if (!keyed) {
    return false;
  }

#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
  bool onlySubsession = false;
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

  if (args.length() >= 1) {
    if (!(args[0].isNumber() || args[0].isBoolean())) {
      JS_ReportErrorASCII(cx, "Not a boolean");
      return false;
    }

    onlySubsession = JS::ToBoolean(args[0]);
  }

  keyed->Clear(onlySubsession);
#else
  keyed->Clear(false);
#endif
  return true;
}

bool
internal_JSKeyedHistogram_Dataset(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  JSObject *obj = JS_THIS_OBJECT(cx, vp);
  if (!obj) {
    return false;
  }

  KeyedHistogram* keyed = static_cast<KeyedHistogram*>(JS_GetPrivate(obj));
  if (!keyed) {
    return false;
  }

  uint32_t dataset = nsITelemetry::DATASET_RELEASE_CHANNEL_OPTIN;
  nsresult rv = keyed->GetDataset(&dataset);;
  if (NS_FAILED(rv)) {
    return false;
  }

  args.rval().setNumber(dataset);
  return true;
}

// NOTE: Runs without protection from |gTelemetryHistogramMutex|.
// See comment at the top of this section.
nsresult
internal_WrapAndReturnKeyedHistogram(KeyedHistogram *h, JSContext *cx,
                                     JS::MutableHandle<JS::Value> ret)
{
  static const JSClass JSHistogram_class = {
    "JSKeyedHistogram",  /* name */
    JSCLASS_HAS_PRIVATE  /* flags */
  };

  JS::Rooted<JSObject*> obj(cx, JS_NewObject(cx, &JSHistogram_class));
  if (!obj)
    return NS_ERROR_FAILURE;
  // The 7 functions that are wrapped up here are eventually called
  // by the same thread that runs this function.
  if (!(JS_DefineFunction(cx, obj, "add", internal_JSKeyedHistogram_Add, 2, 0)
        && JS_DefineFunction(cx, obj, "snapshot",
                             internal_JSKeyedHistogram_Snapshot, 1, 0)
#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
        && JS_DefineFunction(cx, obj, "subsessionSnapshot",
                             internal_JSKeyedHistogram_SubsessionSnapshot, 1, 0)
        && JS_DefineFunction(cx, obj, "snapshotSubsessionAndClear",
                             internal_JSKeyedHistogram_SnapshotSubsessionAndClear, 0, 0)
#endif
        && JS_DefineFunction(cx, obj, "keys",
                             internal_JSKeyedHistogram_Keys, 0, 0)
        && JS_DefineFunction(cx, obj, "clear",
                             internal_JSKeyedHistogram_Clear, 0, 0)
        && JS_DefineFunction(cx, obj, "dataset",
                             internal_JSKeyedHistogram_Dataset, 0, 0))) {
    return NS_ERROR_FAILURE;
  }

  JS_SetPrivate(obj, h);
  ret.setObject(*obj);
  return NS_OK;
}

} // namespace


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// EXTERNALLY VISIBLE FUNCTIONS in namespace TelemetryHistogram::

// All of these functions are actually in namespace TelemetryHistogram::,
// but the ::TelemetryHistogram prefix is given explicitly.  This is
// because it is critical to see which calls from these functions are
// to another function in this interface.  Mis-identifying "inwards
// calls" from "calls to another function in this interface" will lead
// to deadlocking and/or races.  See comments at the top of the file
// for further (important!) details.

// Create and destroy the singleton StatisticsRecorder object.
void TelemetryHistogram::CreateStatisticsRecorder()
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  MOZ_ASSERT(!gStatisticsRecorder);
  gStatisticsRecorder = new base::StatisticsRecorder();
}

void TelemetryHistogram::DestroyStatisticsRecorder()
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  MOZ_ASSERT(gStatisticsRecorder);
  if (gStatisticsRecorder) {
    delete gStatisticsRecorder;
    gStatisticsRecorder = nullptr;
  }
}

void TelemetryHistogram::InitializeGlobalState(bool canRecordBase,
                                               bool canRecordExtended)
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  MOZ_ASSERT(!gInitDone, "TelemetryHistogram::InitializeGlobalState "
             "may only be called once");

  gCanRecordBase = canRecordBase;
  gCanRecordExtended = canRecordExtended;

  // gHistogramMap should have been pre-sized correctly at the
  // declaration point further up in this file.

  // Populate the static histogram name->id cache.
  // Note that the histogram names are statically allocated.
  for (uint32_t i = 0; i < mozilla::Telemetry::HistogramCount; i++) {
    CharPtrEntryType *entry = gHistogramMap.PutEntry(gHistograms[i].id());
    entry->mData = (mozilla::Telemetry::ID) i;
  }

#ifdef DEBUG
  gHistogramMap.MarkImmutable();
#endif

  mozilla::PodArrayZero(gCorruptHistograms);

  // Create registered keyed histograms
  for (size_t i = 0; i < mozilla::ArrayLength(gHistograms); ++i) {
    const HistogramInfo& h = gHistograms[i];
    if (!h.keyed) {
      continue;
    }

    const nsDependentCString id(h.id());
    const nsDependentCString expiration(h.expiration());
    gKeyedHistograms.Put(id, new KeyedHistogram(id, expiration, h.histogramType,
                                                h.min, h.max, h.bucketCount, h.dataset));
    if (XRE_IsParentProcess()) {
      // We must create registered child keyed histograms as well or else the
      // same code in TelemetrySession.jsm that fails without parent keyed
      // histograms will fail without child keyed histograms.
      nsCString contentId(id);
      contentId.AppendLiteral(CONTENT_HISTOGRAM_SUFFIX);
      gKeyedHistograms.Put(contentId,
                           new KeyedHistogram(id, expiration, h.histogramType,
                                              h.min, h.max, h.bucketCount, h.dataset));


      nsCString gpuId(id);
      gpuId.AppendLiteral(GPU_HISTOGRAM_SUFFIX);
      gKeyedHistograms.Put(gpuId,
                           new KeyedHistogram(id, expiration, h.histogramType,
                                              h.min, h.max, h.bucketCount, h.dataset));
    }
  }

  // Some Telemetry histograms depend on the value of C++ constants and hardcode
  // their values in Histograms.json.
  // We add static asserts here for those values to match so that future changes
  // don't go unnoticed.
  // TODO: Compare explicitly with gHistograms[<histogram id>].bucketCount here
  // once we can make gHistograms constexpr (requires VS2015).
  static_assert((JS::gcreason::NUM_TELEMETRY_REASONS == 100),
      "NUM_TELEMETRY_REASONS is assumed to be a fixed value in Histograms.json."
      " If this was an intentional change, update this assert with its value "
      "and update the n_values for the following in Histograms.json: "
      "GC_MINOR_REASON, GC_MINOR_REASON_LONG, GC_REASON_2");
  static_assert((mozilla::StartupTimeline::MAX_EVENT_ID == 16),
      "MAX_EVENT_ID is assumed to be a fixed value in Histograms.json.  If this"
      " was an intentional change, update this assert with its value and update"
      " the n_values for the following in Histograms.json:"
      " STARTUP_MEASUREMENT_ERRORS");

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  MOZ_ASSERT(obs);
  obs->AddObserver(new TelemetryShutdownObserver, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);

  gInitDone = true;
}

void TelemetryHistogram::DeInitializeGlobalState()
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  gCanRecordBase = false;
  gCanRecordExtended = false;
  gHistogramMap.Clear();
  gKeyedHistograms.Clear();
  gAddonMap.Clear();
  gAccumulations = nullptr;
  gKeyedAccumulations = nullptr;
  if (gIPCTimer) {
    NS_RELEASE(gIPCTimer);
  }
  gInitDone = false;
}

#ifdef DEBUG
bool TelemetryHistogram::GlobalStateHasBeenInitialized() {
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  return gInitDone;
}
#endif

bool
TelemetryHistogram::CanRecordBase() {
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  return internal_CanRecordBase();
}

void
TelemetryHistogram::SetCanRecordBase(bool b) {
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  gCanRecordBase = b;
}

bool
TelemetryHistogram::CanRecordExtended() {
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  return internal_CanRecordExtended();
}

void
TelemetryHistogram::SetCanRecordExtended(bool b) {
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  gCanRecordExtended = b;
}


void
TelemetryHistogram::InitHistogramRecordingEnabled()
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  const size_t length = mozilla::ArrayLength(kRecordingInitiallyDisabledIDs);
  for (size_t i = 0; i < length; i++) {
    internal_SetHistogramRecordingEnabled(kRecordingInitiallyDisabledIDs[i],
                                          false);
  }
}

void
TelemetryHistogram::SetHistogramRecordingEnabled(mozilla::Telemetry::ID aID,
                                                 bool aEnabled)
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  internal_SetHistogramRecordingEnabled(aID, aEnabled);
}


nsresult
TelemetryHistogram::SetHistogramRecordingEnabled(const nsACString &id,
                                                 bool aEnabled)
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  Histogram *h;
  nsresult rv = internal_GetHistogramByName(id, &h);
  if (NS_SUCCEEDED(rv)) {
    h->SetRecordingEnabled(aEnabled);
    return NS_OK;
  }

  KeyedHistogram* keyed = internal_GetKeyedHistogramById(id);
  if (keyed) {
    keyed->SetRecordingEnabled(aEnabled);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}


void
TelemetryHistogram::Accumulate(mozilla::Telemetry::ID aHistogram,
                               uint32_t aSample)
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  internal_Accumulate(aHistogram, aSample);
}

void
TelemetryHistogram::Accumulate(mozilla::Telemetry::ID aID,
                               const nsCString& aKey, uint32_t aSample)
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  internal_Accumulate(aID, aKey, aSample);
}

void
TelemetryHistogram::Accumulate(const char* name, uint32_t sample)
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  if (!internal_CanRecordBase()) {
    return;
  }
  mozilla::Telemetry::ID id;
  nsresult rv = internal_GetHistogramEnumId(name, &id);
  if (NS_FAILED(rv)) {
    return;
  }
  internal_Accumulate(id, sample);
}

void
TelemetryHistogram::Accumulate(const char* name,
                               const nsCString& key, uint32_t sample)
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  if (!internal_CanRecordBase()) {
    return;
  }
  mozilla::Telemetry::ID id;
  nsresult rv = internal_GetHistogramEnumId(name, &id);
  if (NS_SUCCEEDED(rv)) {
    internal_Accumulate(id, key, sample);
  }
}

void
TelemetryHistogram::AccumulateCategorical(mozilla::Telemetry::ID aId,
                                          const nsCString& label)
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  if (!internal_CanRecordBase()) {
    return;
  }
  uint32_t labelId = 0;
  if (NS_FAILED(gHistograms[aId].label_id(label.get(), &labelId))) {
    return;
  }
  internal_Accumulate(aId, labelId);
}

void
TelemetryHistogram::AccumulateChild(GeckoProcessType aProcessType,
                                    const nsTArray<Accumulation>& aAccumulations)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  if (!internal_CanRecordBase()) {
    return;
  }
  for (uint32_t i = 0; i < aAccumulations.Length(); ++i) {
    bool isValid = internal_IsHistogramEnumId(aAccumulations[i].mId);
    MOZ_ASSERT(isValid);
    if (!isValid) {
      continue;
    }
    internal_AccumulateChild(aProcessType, aAccumulations[i].mId, aAccumulations[i].mSample);
  }
}

void
TelemetryHistogram::AccumulateChildKeyed(GeckoProcessType aProcessType,
                                         const nsTArray<KeyedAccumulation>& aAccumulations)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  if (!internal_CanRecordBase()) {
    return;
  }
  for (uint32_t i = 0; i < aAccumulations.Length(); ++i) {
    bool isValid = internal_IsHistogramEnumId(aAccumulations[i].mId);
    MOZ_ASSERT(isValid);
    if (!isValid) {
      continue;
    }
    internal_AccumulateChildKeyed(aProcessType,
                                  aAccumulations[i].mId,
                                  aAccumulations[i].mKey,
                                  aAccumulations[i].mSample);
  }
}

nsresult
TelemetryHistogram::GetHistogramById(const nsACString &name, JSContext *cx,
                                     JS::MutableHandle<JS::Value> ret)
{
  Histogram *h = nullptr;
  {
    StaticMutexAutoLock locker(gTelemetryHistogramMutex);
    nsresult rv = internal_GetHistogramByName(name, &h);
    if (NS_FAILED(rv))
      return rv;
  }
  // Runs without protection from |gTelemetryHistogramMutex|
  return internal_WrapAndReturnHistogram(h, cx, ret);
}

nsresult
TelemetryHistogram::GetKeyedHistogramById(const nsACString &name,
                                          JSContext *cx,
                                          JS::MutableHandle<JS::Value> ret)
{
  KeyedHistogram* keyed = nullptr;
  {
    StaticMutexAutoLock locker(gTelemetryHistogramMutex);
    if (!gKeyedHistograms.Get(name, &keyed)) {
      return NS_ERROR_FAILURE;
    }
  }
  // Runs without protection from |gTelemetryHistogramMutex|
  return internal_WrapAndReturnKeyedHistogram(keyed, cx, ret);
}

const char*
TelemetryHistogram::GetHistogramName(mozilla::Telemetry::ID id)
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  const HistogramInfo& h = gHistograms[id];
  return h.id();
}

nsresult
TelemetryHistogram::HistogramFrom(const nsACString &name,
                                  const nsACString &existing_name,
                                  JSContext *cx,
                                  JS::MutableHandle<JS::Value> ret)
{
  Histogram* clone = nullptr;
  {
    StaticMutexAutoLock locker(gTelemetryHistogramMutex);
    mozilla::Telemetry::ID id;
    nsresult rv
      = internal_GetHistogramEnumId(PromiseFlatCString(existing_name).get(),
                                    &id);
    if (NS_FAILED(rv)) {
      return rv;
    }

    clone = internal_CloneHistogram(name, id);
    if (!clone) {
      return NS_ERROR_FAILURE;
    }
  }

  // Runs without protection from |gTelemetryHistogramMutex|
  return internal_WrapAndReturnHistogram(clone, cx, ret);
}

nsresult
TelemetryHistogram::CreateHistogramSnapshots(JSContext *cx,
                                             JS::MutableHandle<JS::Value> ret,
                                             bool subsession,
                                             bool clearSubsession)
{
  // Runs without protection from |gTelemetryHistogramMutex|
  JS::Rooted<JSObject*> root_obj(cx, JS_NewPlainObject(cx));
  if (!root_obj)
    return NS_ERROR_FAILURE;
  ret.setObject(*root_obj);

  // Include the GPU process in histogram snapshots only if we actually tried
  // to launch a process for it.
  bool includeGPUProcess = false;
  if (auto gpm = mozilla::gfx::GPUProcessManager::Get()) {
    includeGPUProcess = gpm->AttemptedGPUProcess();
  }

  // Ensure that all the HISTOGRAM_FLAG & HISTOGRAM_COUNT histograms have
  // been created, so that their values are snapshotted.
  for (size_t i = 0; i < mozilla::Telemetry::HistogramCount; ++i) {
    if (gHistograms[i].keyed) {
      continue;
    }
    const uint32_t type = gHistograms[i].histogramType;
    if (type == nsITelemetry::HISTOGRAM_FLAG ||
        type == nsITelemetry::HISTOGRAM_COUNT) {
      Histogram *h;
      mozilla::DebugOnly<nsresult> rv;
      mozilla::Telemetry::ID id = mozilla::Telemetry::ID(i);

      rv = internal_GetHistogramByEnumId(id, &h, GeckoProcessType_Default);
      MOZ_ASSERT(NS_SUCCEEDED(rv));

      rv = internal_GetHistogramByEnumId(id, &h, GeckoProcessType_Content);
      MOZ_ASSERT(NS_SUCCEEDED(rv));

      if (includeGPUProcess) {
        rv = internal_GetHistogramByEnumId(id, &h, GeckoProcessType_GPU);
        MOZ_ASSERT(NS_SUCCEEDED(rv));
      }
    }
  }

  StatisticsRecorder::Histograms hs;
  StatisticsRecorder::GetHistograms(&hs);

  // We identify corrupt histograms first, rather than interspersing it
  // in the loop below, to ensure that our corruption statistics don't
  // depend on histogram enumeration order.
  //
  // Of course, we hope that all of these corruption-statistics
  // histograms are not themselves corrupt...
  internal_IdentifyCorruptHistograms(hs);

  // OK, now we can actually reflect things.
  JS::Rooted<JSObject*> hobj(cx);
  for (HistogramIterator it = hs.begin(); it != hs.end(); ++it) {
    Histogram *h = *it;
    if (!internal_ShouldReflectHistogram(h) || internal_IsEmpty(h) ||
        internal_IsExpired(h)) {
      continue;
    }

    Histogram* original = h;
#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
    if (subsession) {
      h = internal_GetSubsessionHistogram(*h);
      if (!h) {
        continue;
      }
    }
#endif

    hobj = JS_NewPlainObject(cx);
    if (!hobj) {
      return NS_ERROR_FAILURE;
    }
    switch (internal_ReflectHistogramSnapshot(cx, hobj, h)) {
    case REFLECT_CORRUPT:
      // We can still hit this case even if ShouldReflectHistograms
      // returns true.  The histogram lies outside of our control
      // somehow; just skip it.
      continue;
    case REFLECT_FAILURE:
      return NS_ERROR_FAILURE;
    case REFLECT_OK:
      if (!JS_DefineProperty(cx, root_obj, original->histogram_name().c_str(),
                             hobj, JSPROP_ENUMERATE)) {
        return NS_ERROR_FAILURE;
      }
    }

#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
    if (subsession && clearSubsession) {
      h->Clear();
    }
#endif
  }
  return NS_OK;
}

nsresult
TelemetryHistogram::RegisteredHistograms(uint32_t aDataset, uint32_t *aCount,
                                         char*** aHistograms)
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  return internal_GetRegisteredHistogramIds(false,
                                            aDataset, aCount, aHistograms);
}

nsresult
TelemetryHistogram::RegisteredKeyedHistograms(uint32_t aDataset,
                                              uint32_t *aCount,
                                              char*** aHistograms)
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  return internal_GetRegisteredHistogramIds(true,
                                            aDataset, aCount, aHistograms);
}

nsresult
TelemetryHistogram::GetKeyedHistogramSnapshots(JSContext *cx,
                                               JS::MutableHandle<JS::Value> ret)
{
  // Runs without protection from |gTelemetryHistogramMutex|
  JS::Rooted<JSObject*> obj(cx, JS_NewPlainObject(cx));
  if (!obj) {
    return NS_ERROR_FAILURE;
  }

  for (auto iter = gKeyedHistograms.Iter(); !iter.Done(); iter.Next()) {
    JS::RootedObject snapshot(cx, JS_NewPlainObject(cx));
    if (!snapshot) {
      return NS_ERROR_FAILURE;
    }

    if (!NS_SUCCEEDED(iter.Data()->GetJSSnapshot(cx, snapshot, false, false))) {
      return NS_ERROR_FAILURE;
    }

    if (!JS_DefineProperty(cx, obj, PromiseFlatCString(iter.Key()).get(),
                           snapshot, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
  }

  ret.setObject(*obj);
  return NS_OK;
}

nsresult
TelemetryHistogram::RegisterAddonHistogram(const nsACString &id,
                                           const nsACString &name,
                                           uint32_t histogramType,
                                           uint32_t min, uint32_t max,
                                           uint32_t bucketCount,
                                           uint8_t optArgCount)
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  if (histogramType == nsITelemetry::HISTOGRAM_EXPONENTIAL ||
      histogramType == nsITelemetry::HISTOGRAM_LINEAR) {
    if (optArgCount != 3) {
      return NS_ERROR_INVALID_ARG;
    }

    // Sanity checks for histogram parameters.
    if (min >= max)
      return NS_ERROR_ILLEGAL_VALUE;

    if (bucketCount <= 2)
      return NS_ERROR_ILLEGAL_VALUE;

    if (min < 1)
      return NS_ERROR_ILLEGAL_VALUE;
  } else {
    min = 1;
    max = 2;
    bucketCount = 3;
  }

  AddonEntryType *addonEntry = gAddonMap.GetEntry(id);
  if (!addonEntry) {
    addonEntry = gAddonMap.PutEntry(id);
    if (MOZ_UNLIKELY(!addonEntry)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    addonEntry->mData = new AddonHistogramMapType();
  }

  AddonHistogramMapType *histogramMap = addonEntry->mData;
  AddonHistogramEntryType *histogramEntry = histogramMap->GetEntry(name);
  // Can't re-register the same histogram.
  if (histogramEntry) {
    return NS_ERROR_FAILURE;
  }

  histogramEntry = histogramMap->PutEntry(name);
  if (MOZ_UNLIKELY(!histogramEntry)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  AddonHistogramInfo &info = histogramEntry->mData;
  info.min = min;
  info.max = max;
  info.bucketCount = bucketCount;
  info.histogramType = histogramType;

  return NS_OK;
}

nsresult
TelemetryHistogram::GetAddonHistogram(const nsACString &id,
                                      const nsACString &name,
                                      JSContext *cx,
                                      JS::MutableHandle<JS::Value> ret)
{
  AddonHistogramInfo* info = nullptr;
  {
    StaticMutexAutoLock locker(gTelemetryHistogramMutex);
    AddonEntryType *addonEntry = gAddonMap.GetEntry(id);
    // The given id has not been registered.
    if (!addonEntry) {
      return NS_ERROR_INVALID_ARG;
    }

    AddonHistogramMapType *histogramMap = addonEntry->mData;
    AddonHistogramEntryType *histogramEntry = histogramMap->GetEntry(name);
    // The given histogram name has not been registered.
    if (!histogramEntry) {
      return NS_ERROR_INVALID_ARG;
    }

    info = &histogramEntry->mData;
    if (!info->h) {
      nsAutoCString actualName;
      internal_AddonHistogramName(id, name, actualName);
      if (!internal_CreateHistogramForAddon(actualName, *info)) {
        return NS_ERROR_FAILURE;
      }
    }
  }

  // Runs without protection from |gTelemetryHistogramMutex|
  return internal_WrapAndReturnHistogram(info->h, cx, ret);
}

nsresult
TelemetryHistogram::UnregisterAddonHistograms(const nsACString &id)
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  AddonEntryType *addonEntry = gAddonMap.GetEntry(id);
  if (addonEntry) {
    // Histogram's destructor is private, so this is the best we can do.
    // The histograms the addon created *will* stick around, but they
    // will be deleted if and when the addon registers histograms with
    // the same names.
    delete addonEntry->mData;
    gAddonMap.RemoveEntry(addonEntry);
  }

  return NS_OK;
}

nsresult
TelemetryHistogram::GetAddonHistogramSnapshots(JSContext *cx,
                                               JS::MutableHandle<JS::Value> ret)
{
  // Runs without protection from |gTelemetryHistogramMutex|
  JS::Rooted<JSObject*> obj(cx, JS_NewPlainObject(cx));
  if (!obj) {
    return NS_ERROR_FAILURE;
  }

  if (!gAddonMap.ReflectIntoJS(internal_AddonReflector, cx, obj)) {
    return NS_ERROR_FAILURE;
  }
  ret.setObject(*obj);
  return NS_OK;
}

size_t
TelemetryHistogram::GetMapShallowSizesOfExcludingThis(mozilla::MallocSizeOf
                                                      aMallocSizeOf)
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  return gAddonMap.ShallowSizeOfExcludingThis(aMallocSizeOf) +
         gHistogramMap.ShallowSizeOfExcludingThis(aMallocSizeOf);
}

size_t
TelemetryHistogram::GetHistogramSizesofIncludingThis(mozilla::MallocSizeOf
                                                     aMallocSizeOf)
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  StatisticsRecorder::Histograms hs;
  StatisticsRecorder::GetHistograms(&hs);
  size_t n = 0;
  for (HistogramIterator it = hs.begin(); it != hs.end(); ++it) {
    Histogram *h = *it;
    n += h->SizeOfIncludingThis(aMallocSizeOf);
  }
  return n;
}

// This method takes the lock only to double-buffer the batched telemetry.
// It releases the lock before calling out to IPC code which can (and does)
// Accumulate (which would deadlock)
//
// To ensure we don't loop IPCTimerFired->AccumulateChild->arm timer, we don't
// unset gIPCTimerArmed until the IPC completes
//
// This function must be called on the main thread, otherwise IPC will fail.
void
TelemetryHistogram::IPCTimerFired(nsITimer* aTimer, void* aClosure)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsTArray<Accumulation> accumulationsToSend;
  nsTArray<KeyedAccumulation> keyedAccumulationsToSend;
  {
    StaticMutexAutoLock locker(gTelemetryHistogramMutex);
    if (gAccumulations) {
      accumulationsToSend.SwapElements(*gAccumulations);
    }
    if (gKeyedAccumulations) {
      keyedAccumulationsToSend.SwapElements(*gKeyedAccumulations);
    }
  }

  switch (XRE_GetProcessType()) {
    case GeckoProcessType_Content: {
      mozilla::dom::ContentChild* contentChild = mozilla::dom::ContentChild::GetSingleton();
      mozilla::Unused << NS_WARN_IF(!contentChild);
      if (contentChild) {
        if (accumulationsToSend.Length()) {
          mozilla::Unused <<
            NS_WARN_IF(!contentChild->SendAccumulateChildHistogram(accumulationsToSend));
        }
        if (keyedAccumulationsToSend.Length()) {
          mozilla::Unused <<
            NS_WARN_IF(!contentChild->SendAccumulateChildKeyedHistogram(keyedAccumulationsToSend));
        }
      }
      break;
    }
    case GeckoProcessType_GPU: {
      if (mozilla::gfx::GPUParent* gpu = mozilla::gfx::GPUParent::GetSingleton()) {
        if (accumulationsToSend.Length()) {
          mozilla::Unused << gpu->SendAccumulateChildHistogram(accumulationsToSend);
        }
        if (keyedAccumulationsToSend.Length()) {
          mozilla::Unused << gpu->SendAccumulateChildKeyedHistogram(keyedAccumulationsToSend);
        }
      }
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported process type");
      break;
  }

  gIPCTimerArmed = false;
}
