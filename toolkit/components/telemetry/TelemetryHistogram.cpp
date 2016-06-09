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
#include "nsVersionComparator.h"

#include "mozilla/dom/ToJSValue.h"
#include "mozilla/StartupTimeline.h"
#include "mozilla/StaticMutex.h"

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

namespace {

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
  bool keyed;

  const char *id() const;
  const char *expiration() const;
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

typedef AutoHashtable<AddonHistogramEntryType> AddonHistogramMapType;

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

bool gCanRecordBase;
bool gCanRecordExtended;

HistogramMapType gHistogramMap(mozilla::Telemetry::HistogramCount);

KeyedHistogramMapType gKeyedHistograms;

bool gCorruptHistograms[mozilla::Telemetry::HistogramCount];

// This is for gHistograms, gHistogramStringTable
#include "TelemetryHistogramData.inc"

AddonMapType gAddonMap;

// The singleton StatisticsRecorder object for this process.
base::StatisticsRecorder* gStatisticsRecorder = nullptr;

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

bool
internal_IsExpired(const char *expiration)
{
  static mozilla::Version current_version = mozilla::Version(MOZ_APP_VERSION);
  MOZ_ASSERT(expiration);
  return strcmp(expiration, "never") && strcmp(expiration, "default") &&
    (mozilla::Version(expiration) <= current_version);
}

bool
internal_IsInDataset(uint32_t dataset, uint32_t containingDataset)
{
  if (dataset == containingDataset) {
    return true;
  }

  // The "optin on release channel" dataset is a superset of the
  // "optout on release channel one".
  if (containingDataset == nsITelemetry::DATASET_RELEASE_CHANNEL_OPTIN
      && dataset == nsITelemetry::DATASET_RELEASE_CHANNEL_OPTOUT) {
    return true;
  }

  return false;
}

bool
internal_CanRecordDataset(uint32_t dataset)
{
  // If we are extended telemetry is enabled, we are allowed to record
  // regardless of the dataset.
  if (internal_CanRecordExtended()) {
    return true;
  }

  // If base telemetry data is enabled and we're trying to record base
  // telemetry, allow it.
  if (internal_CanRecordBase() &&
      internal_IsInDataset(dataset,
                           nsITelemetry::DATASET_RELEASE_CHANNEL_OPTOUT)) {
      return true;
  }

  // We're not recording extended telemetry or this is not the base
  // dataset. Bail out.
  return false;
}

bool
internal_IsValidHistogramName(const nsACString& name)
{
  return !FindInReadable(NS_LITERAL_CSTRING(KEYED_HISTOGRAM_NAME_SEPARATOR), name);
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
    if (internal_IsExpired(h.expiration()) || h.keyed != keyed ||
        !internal_IsInDataset(h.dataset, dataset)) {
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

  if (internal_IsExpired(expiration)) {
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

nsresult
internal_GetHistogramEnumId(const char *name, mozilla::Telemetry::ID *id)
{
  if (!gInitDone) {
    return NS_ERROR_FAILURE;
  }

  CharPtrEntryType *entry = gHistogramMap.GetEntry(name);
  if (!entry) {
    return NS_ERROR_INVALID_ARG;
  }
  *id = entry->mData;
  return NS_OK;
}

// O(1) histogram lookup by numeric id
nsresult
internal_GetHistogramByEnumId(mozilla::Telemetry::ID id, Histogram **ret)
{
  static Histogram* knownHistograms[mozilla::Telemetry::HistogramCount] = {0};
  Histogram *h = knownHistograms[id];
  if (h) {
    *ret = h;
    return NS_OK;
  }

  const HistogramInfo &p = gHistograms[id];
  if (p.keyed) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = internal_HistogramGet(p.id(), p.expiration(), p.histogramType,
                                      p.min, p.max, p.bucketCount, true, &h);
  if (NS_FAILED(rv))
    return rv;

#ifdef DEBUG
  // Check that the C++ Histogram code computes the same ranges as the
  // Python histogram code.
  if (!internal_IsExpired(p.expiration())) {
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

  *ret = knownHistograms[id] = h;
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

  rv = internal_GetHistogramByEnumId(id, ret);
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
  nsresult rv = internal_GetHistogramByEnumId(existingId, &existing);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return internal_CloneHistogram(newName, existingId, *existing);
}

#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
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
  if (subsession[id]) {
    return subsession[id];
  }

  NS_NAMED_LITERAL_CSTRING(prefix, SUBSESSION_HISTOGRAM_PREFIX);
  nsDependentCString existingName(gHistograms[id].id());
  if (StringBeginsWith(existingName, prefix)) {
    return nullptr;
  }

  nsCString subsessionName(prefix);
  subsessionName.Append(existingName);

  subsession[id] = internal_CloneHistogram(subsessionName, id, existing);
  return subsession[id];
}
#endif

nsresult
internal_HistogramAdd(Histogram& histogram, int32_t value, uint32_t dataset)
{
  // Check if we are allowed to record the data.
  if (!internal_CanRecordDataset(dataset) || !histogram.IsRecordingEnabled()) {
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
  if (!internal_CanRecordDataset(mDataset)) {
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
  if (!obj) {
    return false;
  }

  Histogram *h = static_cast<Histogram*>(JS_GetPrivate(obj));
  MOZ_ASSERT(h);
  Histogram::ClassType type = h->histogram_type();

  JS::CallArgs args = CallArgsFromVp(argc, vp);

  // If we don't have an argument for the count histogram, assume an increment of 1.
  // Otherwise, make sure to run some sanity checks on the argument.
  int32_t value = 1;
  if ((type != base::CountHistogram::COUNT_HISTOGRAM) || args.length()) {
    if (!args.length()) {
      JS_ReportError(cx, "Expected one argument");
      return false;
    }

    if (!(args[0].isNumber() || args[0].isBoolean())) {
      JS_ReportError(cx, "Not a number");
      return false;
    }

    if (!JS::ToInt32(cx, args[0], &value)) {
      return false;
    }
  }

  if (internal_CanRecordBase()) {
    internal_HistogramAdd(*h, value);
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
    JS_ReportError(cx, "Histogram is corrupt");
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
      JS_ReportError(cx, "Not a boolean");
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
      JS_ReportError(cx, "Failed to create object");
      return false;
    }

    if (!NS_SUCCEEDED(keyed->GetJSSnapshot(cx, snapshot, subsession, clearSubsession))) {
      JS_ReportError(cx, "Failed to reflect keyed histograms");
      return false;
    }

    args.rval().setObject(*snapshot);
    return true;
  }

  nsAutoJSString key;
  if (!args[0].isString() || !key.init(cx, args[0])) {
    JS_ReportError(cx, "Not a string");
    return false;
  }

  Histogram* h = nullptr;
  nsresult rv = keyed->GetHistogram(NS_ConvertUTF16toUTF8(key), &h, subsession);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, "Failed to get histogram");
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
    JS_ReportError(cx, "Histogram is corrupt");
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
    JS_ReportError(cx, "Expected one argument");
    return false;
  }

  nsAutoJSString key;
  if (!args[0].isString() || !key.init(cx, args[0])) {
    JS_ReportError(cx, "Not a string");
    return false;
  }

  const uint32_t type = keyed->GetHistogramType();

  // If we don't have an argument for the count histogram, assume an increment of 1.
  // Otherwise, make sure to run some sanity checks on the argument.
  int32_t value = 1;
  if ((type != base::CountHistogram::COUNT_HISTOGRAM) || (args.length() == 2)) {
    if (args.length() < 2) {
      JS_ReportError(cx, "Expected two arguments for this histogram type");
      return false;
    }

    if (!(args[1].isNumber() || args[1].isBoolean())) {
      JS_ReportError(cx, "Not a number");
      return false;
    }

    if (!JS::ToInt32(cx, args[1], &value)) {
      return false;
    }
  }

  keyed->Add(NS_ConvertUTF16toUTF8(key), value);
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
    JS_ReportError(cx, "No key arguments supported for snapshotSubsessionAndClear");
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
      JS_ReportError(cx, "Not a boolean");
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
    nsresult rv = internal_GetHistogramByEnumId(aID, &h);
    if (NS_SUCCEEDED(rv)) {
      h->SetRecordingEnabled(aEnabled);
      return;
    }
  }

  MOZ_ASSERT(false, "Telemetry::SetHistogramRecordingEnabled(...) id not found");
}

void internal_Accumulate(mozilla::Telemetry::ID aHistogram, uint32_t aSample)
{
  if (!internal_CanRecordBase()) {
    return;
  }
  Histogram *h;
  nsresult rv = internal_GetHistogramByEnumId(aHistogram, &h);
  if (NS_SUCCEEDED(rv)) {
    internal_HistogramAdd(*h, aSample, gHistograms[aHistogram].dataset);
  }
}

void
internal_Accumulate(mozilla::Telemetry::ID aID,
                    const nsCString& aKey, uint32_t aSample)
{
  if (!gInitDone || !internal_CanRecordBase()) {
    return;
  }
  const HistogramInfo& th = gHistograms[aID];
  KeyedHistogram* keyed
     = internal_GetKeyedHistogramById(nsDependentCString(th.id()));
  MOZ_ASSERT(keyed);
  keyed->Add(aKey, aSample);
}

} // namespace


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// EXTERNALLY VISIBLE FUNCTIONS in namespace TelemetryHistogram::

// This is a StaticMutex rather than a plain Mutex (1) so that
// it gets initialised in a thread-safe manner the first time
// it is used, and (2) because it is never de-initialised, and
// a normal Mutex would show up as a leak in BloatView.  StaticMutex
// also has the "OffTheBooks" property, so it won't show as a leak
// in BloatView.
static StaticMutex gTelemetryHistogramMutex;

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

  Histogram *h;
  rv = internal_GetHistogramByEnumId(id, &h);
  if (NS_SUCCEEDED(rv)) {
    internal_HistogramAdd(*h, sample, gHistograms[id].dataset);
  }
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
TelemetryHistogram::ClearHistogram(mozilla::Telemetry::ID aId)
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  if (!internal_CanRecordBase()) {
    return;
  }

  Histogram *h;
  nsresult rv = internal_GetHistogramByEnumId(aId, &h);
  if (NS_SUCCEEDED(rv) && h) {
    internal_HistogramClear(*h, false);
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
TelemetryHistogram::NewHistogram(const nsACString &name,
                                 const nsACString &expiration,
                                 uint32_t histogramType,
                                 uint32_t min, uint32_t max,
                                 uint32_t bucketCount, JSContext *cx,
                                 uint8_t optArgCount,
                                 JS::MutableHandle<JS::Value> ret)
{
  Histogram *h = nullptr;
  {
    StaticMutexAutoLock locker(gTelemetryHistogramMutex);
    if (!internal_IsValidHistogramName(name)) {
      return NS_ERROR_INVALID_ARG;
    }

    nsresult rv = internal_HistogramGet(PromiseFlatCString(name).get(),
                                        PromiseFlatCString(expiration).get(),
                                        histogramType, min, max, bucketCount,
                                        optArgCount == 3, &h);
    if (NS_FAILED(rv))
      return rv;
    h->ClearFlags(Histogram::kUmaTargetedHistogramFlag);
  }

  // Runs without protection from |gTelemetryHistogramMutex|
  return internal_WrapAndReturnHistogram(h, cx, ret);
}

nsresult
TelemetryHistogram::NewKeyedHistogram(const nsACString &name,
                                      const nsACString &expiration,
                                      uint32_t histogramType,
                                      uint32_t min, uint32_t max,
                                      uint32_t bucketCount, JSContext *cx,
                                      uint8_t optArgCount,
                                      JS::MutableHandle<JS::Value> ret)
{
  KeyedHistogram* keyed = nullptr;
  {
    StaticMutexAutoLock locker(gTelemetryHistogramMutex);
    if (!internal_IsValidHistogramName(name)) {
      return NS_ERROR_INVALID_ARG;
    }

    nsresult rv
      = internal_CheckHistogramArguments(histogramType, min, max,
                                         bucketCount, optArgCount == 3);
    if (NS_FAILED(rv)) {
      return rv;
    }

    keyed = new KeyedHistogram(name, expiration, histogramType,
                               min, max, bucketCount,
                               nsITelemetry::DATASET_RELEASE_CHANNEL_OPTIN);
    if (MOZ_UNLIKELY(!gKeyedHistograms.Put(name, keyed, mozilla::fallible))) {
      delete keyed;
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  // Runs without protection from |gTelemetryHistogramMutex|
  return internal_WrapAndReturnKeyedHistogram(keyed, cx, ret);
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
      mozilla::DebugOnly<nsresult> rv
         = internal_GetHistogramByEnumId(mozilla::Telemetry::ID(i), &h);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
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
