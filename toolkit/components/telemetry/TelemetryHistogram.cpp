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

#include "TelemetryCommon.h"
#include "TelemetryHistogram.h"

#include "base/histogram.h"

using base::Histogram;
using base::StatisticsRecorder;
using base::BooleanHistogram;
using base::CountHistogram;
using base::FlagHistogram;
using base::LinearHistogram;


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

} // namespace


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE CONSTANTS

// List of histogram IDs which should have recording disabled initially.
const mozilla::Telemetry::ID kRecordingInitiallyDisabledIDs[] = {
  mozilla::Telemetry::FX_REFRESH_DRIVER_SYNC_SCROLL_FRAME_DELAY_MS,

  // The array must not be empty. Leave these item here.
  mozilla::Telemetry::TELEMETRY_TEST_COUNT_INIT_NO_RECORD,
  mozilla::Telemetry::TELEMETRY_TEST_KEYED_COUNT_INIT_NO_RECORD
};


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE: Misc small helpers

namespace {

bool
IsHistogramEnumId(mozilla::Telemetry::ID aID)
{
  static_assert(((mozilla::Telemetry::ID)-1 > 0), "ID should be unsigned.");
  return aID < mozilla::Telemetry::HistogramCount;
}

bool
IsExpired(const char *expiration)
{
  static mozilla::Version current_version = mozilla::Version(MOZ_APP_VERSION);
  MOZ_ASSERT(expiration);
  return strcmp(expiration, "never") && strcmp(expiration, "default") &&
    (mozilla::Version(expiration) <= current_version);
}

bool
IsInDataset(uint32_t dataset, uint32_t containingDataset)
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
CanRecordDataset(uint32_t dataset)
{
  // If we are extended telemetry is enabled, we are allowed to record
  // regardless of the dataset.
  if (TelemetryHistogram::CanRecordExtended()) {
    return true;
  }

  // If base telemetry data is enabled and we're trying to record base
  // telemetry, allow it.
  if (TelemetryHistogram::CanRecordBase() &&
      IsInDataset(dataset, nsITelemetry::DATASET_RELEASE_CHANNEL_OPTOUT)) {
      return true;
  }

  // We're not recording extended telemetry or this is not the base
  // dataset. Bail out.
  return false;
}

bool
IsValidHistogramName(const nsACString& name)
{
  return !FindInReadable(NS_LITERAL_CSTRING(KEYED_HISTOGRAM_NAME_SEPARATOR), name);
}

// Note: this is completely unrelated to mozilla::IsEmpty.
bool
IsEmpty(const Histogram *h)
{
  Histogram::SampleSet ss;
  h->SnapshotSample(&ss);

  mozilla::OffTheBooksMutexAutoLock locker(ss.mutex());
  return ss.counts(locker, 0) == 0 && ss.sum(locker) == 0;
}

bool
IsExpired(const Histogram *histogram)
{
  return histogram->histogram_name() == EXPIRED_ID;
}

nsresult
GetRegisteredHistogramIds(bool keyed, uint32_t dataset, uint32_t *aCount,
                          char*** aHistograms)
{
  nsTArray<char*> collection;

  for (size_t i = 0; i < mozilla::ArrayLength(gHistograms); ++i) {
    const HistogramInfo& h = gHistograms[i];
    if (IsExpired(h.expiration()) || h.keyed != keyed ||
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

} // namespace


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE: Histogram Get, Add, Clone, Clear functions

namespace {

nsresult
CheckHistogramArguments(uint32_t histogramType, uint32_t min, uint32_t max,
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
HistogramGet(const char *name, const char *expiration, uint32_t histogramType,
             uint32_t min, uint32_t max, uint32_t bucketCount, bool haveOptArgs,
             Histogram **result)
{
  nsresult rv = CheckHistogramArguments(histogramType, min, max, bucketCount, haveOptArgs);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (IsExpired(expiration)) {
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
GetHistogramEnumId(const char *name, mozilla::Telemetry::ID *id)
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
GetHistogramByEnumId(mozilla::Telemetry::ID id, Histogram **ret)
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

  nsresult rv = HistogramGet(p.id(), p.expiration(), p.histogramType,
                             p.min, p.max, p.bucketCount, true, &h);
  if (NS_FAILED(rv))
    return rv;

#ifdef DEBUG
  // Check that the C++ Histogram code computes the same ranges as the
  // Python histogram code.
  if (!IsExpired(p.expiration())) {
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
GetHistogramByName(const nsACString &name, Histogram **ret)
{
  mozilla::Telemetry::ID id;
  nsresult rv = GetHistogramEnumId(PromiseFlatCString(name).get(), &id);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = GetHistogramByEnumId(id, ret);
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
CloneHistogram(const nsACString& newName, mozilla::Telemetry::ID existingId,
               Histogram& existing)
{
  const HistogramInfo &info = gHistograms[existingId];
  Histogram *clone = nullptr;
  nsresult rv;

  rv = HistogramGet(PromiseFlatCString(newName).get(), info.expiration(),
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
CloneHistogram(const nsACString& newName, mozilla::Telemetry::ID existingId)
{
  Histogram *existing = nullptr;
  nsresult rv = GetHistogramByEnumId(existingId, &existing);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return CloneHistogram(newName, existingId, *existing);
}

#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
Histogram*
GetSubsessionHistogram(Histogram& existing)
{
  mozilla::Telemetry::ID id;
  nsresult rv = GetHistogramEnumId(existing.histogram_name().c_str(), &id);
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

  subsession[id] = CloneHistogram(subsessionName, id, existing);
  return subsession[id];
}
#endif

nsresult
HistogramAdd(Histogram& histogram, int32_t value, uint32_t dataset)
{
  // Check if we are allowed to record the data.
  if (!CanRecordDataset(dataset) || !histogram.IsRecordingEnabled()) {
    return NS_OK;
  }

#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
  if (Histogram* subsession = GetSubsessionHistogram(histogram)) {
    subsession->Add(value);
  }
#endif

  // It is safe to add to the histogram now: the subsession histogram was already
  // cloned from this so we won't add the sample twice.
  histogram.Add(value);

  return NS_OK;
}

nsresult
HistogramAdd(Histogram& histogram, int32_t value)
{
  uint32_t dataset = nsITelemetry::DATASET_RELEASE_CHANNEL_OPTIN;
  // We only really care about the dataset of the histogram if we are not recording
  // extended telemetry. Otherwise, we always record histogram data.
  if (!TelemetryHistogram::CanRecordExtended()) {
    mozilla::Telemetry::ID id;
    nsresult rv = GetHistogramEnumId(histogram.histogram_name().c_str(), &id);
    if (NS_FAILED(rv)) {
      // If we can't look up the dataset, it might be because the histogram was added
      // at runtime. Since we're not recording extended telemetry, bail out.
      return NS_OK;
    }
    dataset = gHistograms[id].dataset;
  }

  return HistogramAdd(histogram, value, dataset);
}

void
HistogramClear(Histogram& aHistogram, bool onlySubsession)
{
  if (!onlySubsession) {
    aHistogram.Clear();
  }

#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
  if (Histogram* subsession = GetSubsessionHistogram(aHistogram)) {
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

void
IdentifyCorruptHistograms(StatisticsRecorder::Histograms &hs)
{
  for (HistogramIterator it = hs.begin(); it != hs.end(); ++it) {
    Histogram *h = *it;

    mozilla::Telemetry::ID id;
    nsresult rv = ::GetHistogramEnumId(h->histogram_name().c_str(), &id);
    // This histogram isn't a static histogram, just ignore it.
    if (NS_FAILED(rv)) {
      continue;
    }

    if (gCorruptHistograms[id]) {
      continue;
    }

    Histogram::SampleSet ss;
    h->SnapshotSample(&ss);

    Histogram::Inconsistencies check;
    {
      mozilla::OffTheBooksMutexAutoLock locker(ss.mutex());
      check = h->FindCorruption(ss, locker);
    }

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
      TelemetryHistogram::Accumulate(corruptID, 1);
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
FillRanges(JSContext *cx, JS::Handle<JSObject*> array, Histogram *h)
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
ReflectHistogramAndSamples(JSContext *cx, JS::Handle<JSObject*> obj, Histogram *h,
                           const Histogram::SampleSet &ss)
{
  mozilla::OffTheBooksMutexAutoLock locker(ss.mutex());

  // We don't want to reflect corrupt histograms.
  if (h->FindCorruption(ss, locker) != Histogram::NO_INCONSISTENCIES) {
    return REFLECT_CORRUPT;
  }

  if (!(JS_DefineProperty(cx, obj, "min",
                          h->declared_min(), JSPROP_ENUMERATE)
        && JS_DefineProperty(cx, obj, "max",
                             h->declared_max(), JSPROP_ENUMERATE)
        && JS_DefineProperty(cx, obj, "histogram_type",
                             h->histogram_type(), JSPROP_ENUMERATE)
        && JS_DefineProperty(cx, obj, "sum",
                             double(ss.sum(locker)), JSPROP_ENUMERATE))) {
    return REFLECT_FAILURE;
  }

  const size_t count = h->bucket_count();
  JS::Rooted<JSObject*> rarray(cx, JS_NewArrayObject(cx, count));
  if (!rarray) {
    return REFLECT_FAILURE;
  }
  if (!(FillRanges(cx, rarray, h)
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
                          ss.counts(locker, i), JSPROP_ENUMERATE)) {
      return REFLECT_FAILURE;
    }
  }

  return REFLECT_OK;
}

enum reflectStatus
ReflectHistogramSnapshot(JSContext *cx, JS::Handle<JSObject*> obj, Histogram *h)
{
  Histogram::SampleSet ss;
  h->SnapshotSample(&ss);
  return ReflectHistogramAndSamples(cx, obj, h, ss);
}

bool
ShouldReflectHistogram(Histogram *h)
{
  const char *name = h->histogram_name().c_str();
  mozilla::Telemetry::ID id;
  nsresult rv = ::GetHistogramEnumId(name, &id);
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

}


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
  nsresult rv = HistogramGet(histogramName.get(), mExpiration.get(),
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
  if (!CanRecordDataset(mDataset)) {
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

  if (ReflectHistogramSnapshot(cx, histogramSnapshot, entry->mData) != REFLECT_OK) {
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
GetKeyedHistogramById(const nsACString &name)
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

namespace {

bool
JSHistogram_Add(JSContext *cx, unsigned argc, JS::Value *vp)
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

  if (TelemetryHistogram::CanRecordBase()) {
    HistogramAdd(*h, value);
  }

  return true;
}

bool
JSHistogram_Snapshot(JSContext *cx, unsigned argc, JS::Value *vp)
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

  switch (ReflectHistogramSnapshot(cx, snapshot, h)) {
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
JSHistogram_Clear(JSContext *cx, unsigned argc, JS::Value *vp)
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
    HistogramClear(*h, onlySubsession);
  }

  return true;
}

bool
JSHistogram_Dataset(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  JSObject *obj = JS_THIS_OBJECT(cx, vp);
  if (!obj) {
    return false;
  }

  Histogram *h = static_cast<Histogram*>(JS_GetPrivate(obj));
  mozilla::Telemetry::ID id;
  nsresult rv = ::GetHistogramEnumId(h->histogram_name().c_str(), &id);
  if (NS_SUCCEEDED(rv)) {
    args.rval().setNumber(gHistograms[id].dataset);
    return true;
  }

  return false;
}

nsresult
WrapAndReturnHistogram(Histogram *h, JSContext *cx, JS::MutableHandle<JS::Value> ret)
{
  static const JSClass JSHistogram_class = {
    "JSHistogram",  /* name */
    JSCLASS_HAS_PRIVATE  /* flags */
  };

  JS::Rooted<JSObject*> obj(cx, JS_NewObject(cx, &JSHistogram_class));
  if (!obj)
    return NS_ERROR_FAILURE;
  if (!(JS_DefineFunction(cx, obj, "add", JSHistogram_Add, 1, 0)
        && JS_DefineFunction(cx, obj, "snapshot", JSHistogram_Snapshot, 0, 0)
        && JS_DefineFunction(cx, obj, "clear", JSHistogram_Clear, 0, 0)
        && JS_DefineFunction(cx, obj, "dataset", JSHistogram_Dataset, 0, 0))) {
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

namespace {

bool
KeyedHistogram_SnapshotImpl(JSContext *cx, unsigned argc, JS::Value *vp,
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

  switch (ReflectHistogramSnapshot(cx, snapshot, h)) {
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
JSKeyedHistogram_Add(JSContext *cx, unsigned argc, JS::Value *vp)
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
JSKeyedHistogram_Keys(JSContext *cx, unsigned argc, JS::Value *vp)
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
JSKeyedHistogram_Snapshot(JSContext *cx, unsigned argc, JS::Value *vp)
{
  return KeyedHistogram_SnapshotImpl(cx, argc, vp, false, false);
}

#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
bool
JSKeyedHistogram_SubsessionSnapshot(JSContext *cx, unsigned argc, JS::Value *vp)
{
  return KeyedHistogram_SnapshotImpl(cx, argc, vp, true, false);
}
#endif

#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
bool
JSKeyedHistogram_SnapshotSubsessionAndClear(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  if (args.length() != 0) {
    JS_ReportError(cx, "No key arguments supported for snapshotSubsessionAndClear");
  }

  return KeyedHistogram_SnapshotImpl(cx, argc, vp, true, true);
}
#endif

bool
JSKeyedHistogram_Clear(JSContext *cx, unsigned argc, JS::Value *vp)
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
JSKeyedHistogram_Dataset(JSContext *cx, unsigned argc, JS::Value *vp)
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

nsresult
WrapAndReturnKeyedHistogram(KeyedHistogram *h, JSContext *cx, JS::MutableHandle<JS::Value> ret)
{
  static const JSClass JSHistogram_class = {
    "JSKeyedHistogram",  /* name */
    JSCLASS_HAS_PRIVATE  /* flags */
  };

  JS::Rooted<JSObject*> obj(cx, JS_NewObject(cx, &JSHistogram_class));
  if (!obj)
    return NS_ERROR_FAILURE;
  if (!(JS_DefineFunction(cx, obj, "add", JSKeyedHistogram_Add, 2, 0)
        && JS_DefineFunction(cx, obj, "snapshot", JSKeyedHistogram_Snapshot, 1, 0)
#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
        && JS_DefineFunction(cx, obj, "subsessionSnapshot", JSKeyedHistogram_SubsessionSnapshot, 1, 0)
        && JS_DefineFunction(cx, obj, "snapshotSubsessionAndClear", JSKeyedHistogram_SnapshotSubsessionAndClear, 0, 0)
#endif
        && JS_DefineFunction(cx, obj, "keys", JSKeyedHistogram_Keys, 0, 0)
        && JS_DefineFunction(cx, obj, "clear", JSKeyedHistogram_Clear, 0, 0)
        && JS_DefineFunction(cx, obj, "dataset", JSKeyedHistogram_Dataset, 0, 0))) {
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
AddonHistogramName(const nsACString &id, const nsACString &name,
                   nsACString &ret)
{
  ret.Append(id);
  ret.Append(':');
  ret.Append(name);
}

bool
CreateHistogramForAddon(const nsACString &name, AddonHistogramInfo &info)
{
  Histogram *h;
  nsresult rv = HistogramGet(PromiseFlatCString(name).get(), "never",
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
AddonHistogramReflector(AddonHistogramEntryType *entry,
                        JSContext *cx, JS::Handle<JSObject*> obj)
{
  AddonHistogramInfo &info = entry->mData;

  // Never even accessed the histogram.
  if (!info.h) {
    // Have to force creation of HISTOGRAM_FLAG histograms.
    if (info.histogramType != nsITelemetry::HISTOGRAM_FLAG)
      return true;

    if (!CreateHistogramForAddon(entry->GetKey(), info)) {
      return false;
    }
  }

  if (IsEmpty(info.h)) {
    return true;
  }

  JS::Rooted<JSObject*> snapshot(cx, JS_NewPlainObject(cx));
  if (!snapshot) {
    // Just consider this to be skippable.
    return true;
  }
  switch (ReflectHistogramSnapshot(cx, snapshot, info.h)) {
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
AddonReflector(AddonEntryType *entry, JSContext *cx, JS::Handle<JSObject*> obj)
{
  const nsACString &addonId = entry->GetKey();
  JS::Rooted<JSObject*> subobj(cx, JS_NewPlainObject(cx));
  if (!subobj) {
    return false;
  }

  AddonHistogramMapType *map = entry->mData;
  if (!(map->ReflectIntoJS(AddonHistogramReflector, cx, subobj)
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
// EXTERNALLY VISIBLE FUNCTIONS

namespace TelemetryHistogram {

void InitializeGlobalState(bool canRecordBase, bool canRecordExtended)
{
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

void DeInitializeGlobalState()
{
  gCanRecordBase = false;
  gCanRecordExtended = false;
  gHistogramMap.Clear();
  gKeyedHistograms.Clear();
  gAddonMap.Clear();
  gInitDone = false;
}

#ifdef DEBUG
bool GlobalStateHasBeenInitialized() {
  return gInitDone;
}
#endif

bool
CanRecordBase() {
  return gCanRecordBase;
}

void
SetCanRecordBase(bool b) {
  gCanRecordBase = b;
}

bool
CanRecordExtended() {
  return gCanRecordExtended;
}

void
SetCanRecordExtended(bool b) {
  gCanRecordExtended = b;
}


void
InitHistogramRecordingEnabled()
{
  const size_t length = mozilla::ArrayLength(kRecordingInitiallyDisabledIDs);
  for (size_t i = 0; i < length; i++) {
    SetHistogramRecordingEnabled(kRecordingInitiallyDisabledIDs[i], false);
  }
}

void
SetHistogramRecordingEnabled(mozilla::Telemetry::ID aID, bool aEnabled)
{
  if (!IsHistogramEnumId(aID)) {
    MOZ_ASSERT(false, "Telemetry::SetHistogramRecordingEnabled(...) must be used with an enum id");
    return;
  }

  if (gHistograms[aID].keyed) {
    const nsDependentCString id(gHistograms[aID].id());
    KeyedHistogram* keyed = ::GetKeyedHistogramById(id);
    if (keyed) {
      keyed->SetRecordingEnabled(aEnabled);
      return;
    }
  } else {
    Histogram *h;
    nsresult rv = GetHistogramByEnumId(aID, &h);
    if (NS_SUCCEEDED(rv)) {
      h->SetRecordingEnabled(aEnabled);
      return;
    }
  }

  MOZ_ASSERT(false, "Telemetry::SetHistogramRecordingEnabled(...) id not found");
}


nsresult
SetHistogramRecordingEnabled(const nsACString &id, bool aEnabled)
{
  Histogram *h;
  nsresult rv = GetHistogramByName(id, &h);
  if (NS_SUCCEEDED(rv)) {
    h->SetRecordingEnabled(aEnabled);
    return NS_OK;
  }

  KeyedHistogram* keyed = ::GetKeyedHistogramById(id);
  if (keyed) {
    keyed->SetRecordingEnabled(aEnabled);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}


void
Accumulate(mozilla::Telemetry::ID aHistogram, uint32_t aSample)
{
  if (!CanRecordBase()) {
    return;
  }
  Histogram *h;
  nsresult rv = GetHistogramByEnumId(aHistogram, &h);
  if (NS_SUCCEEDED(rv)) {
    HistogramAdd(*h, aSample, gHistograms[aHistogram].dataset);
  }
}

void
Accumulate(mozilla::Telemetry::ID aID, const nsCString& aKey, uint32_t aSample)
{
  if (!gInitDone || !CanRecordBase()) {
    return;
  }
  const HistogramInfo& th = gHistograms[aID];
  KeyedHistogram* keyed
     = ::GetKeyedHistogramById(nsDependentCString(th.id()));
  MOZ_ASSERT(keyed);
  keyed->Add(aKey, aSample);

}

void
Accumulate(const char* name, uint32_t sample)
{
  if (!CanRecordBase()) {
    return;
  }
  mozilla::Telemetry::ID id;
  nsresult rv = ::GetHistogramEnumId(name, &id);
  if (NS_FAILED(rv)) {
    return;
  }

  Histogram *h;
  rv = GetHistogramByEnumId(id, &h);
  if (NS_SUCCEEDED(rv)) {
    HistogramAdd(*h, sample, gHistograms[id].dataset);
  }
}

void
Accumulate(const char* name, const nsCString& key, uint32_t sample)
{
  if (!CanRecordBase()) {
    return;
  }
  mozilla::Telemetry::ID id;
  nsresult rv = ::GetHistogramEnumId(name, &id);
  if (NS_SUCCEEDED(rv)) {
    Accumulate(id, key, sample);
  }
}

void
ClearHistogram(mozilla::Telemetry::ID aId)
{
  if (!TelemetryHistogram::CanRecordBase()) {
    return;
  }

  Histogram *h;
  nsresult rv = ::GetHistogramByEnumId(aId, &h);
  if (NS_SUCCEEDED(rv) && h) {
    ::HistogramClear(*h, false);
  }
}

nsresult
GetHistogramById(const nsACString &name, JSContext *cx,
                 JS::MutableHandle<JS::Value> ret)
{
  Histogram *h;
  nsresult rv = GetHistogramByName(name, &h);
  if (NS_FAILED(rv))
    return rv;

  return WrapAndReturnHistogram(h, cx, ret);
}

nsresult
GetKeyedHistogramById(const nsACString &name, JSContext *cx,
                      JS::MutableHandle<JS::Value> ret)
{
  KeyedHistogram* keyed = nullptr;
  if (!gKeyedHistograms.Get(name, &keyed)) {
    return NS_ERROR_FAILURE;
  }

  return WrapAndReturnKeyedHistogram(keyed, cx, ret);

}

const char*
GetHistogramName(mozilla::Telemetry::ID id)
{
  const HistogramInfo& h = gHistograms[id];
  return h.id();
}

nsresult
NewHistogram(const nsACString &name, const nsACString &expiration,
             uint32_t histogramType, uint32_t min, uint32_t max,
             uint32_t bucketCount, JSContext *cx,
             uint8_t optArgCount, JS::MutableHandle<JS::Value> ret)
{
  if (!IsValidHistogramName(name)) {
    return NS_ERROR_INVALID_ARG;
  }

  Histogram *h;
  nsresult rv = HistogramGet(PromiseFlatCString(name).get(),
                             PromiseFlatCString(expiration).get(),
                             histogramType, min, max, bucketCount,
                             optArgCount == 3, &h);
  if (NS_FAILED(rv))
    return rv;
  h->ClearFlags(Histogram::kUmaTargetedHistogramFlag);
  return WrapAndReturnHistogram(h, cx, ret);

}

nsresult
NewKeyedHistogram(const nsACString &name, const nsACString &expiration,
                  uint32_t histogramType, uint32_t min, uint32_t max,
                  uint32_t bucketCount, JSContext *cx,
                  uint8_t optArgCount, JS::MutableHandle<JS::Value> ret)
{
  if (!IsValidHistogramName(name)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv = CheckHistogramArguments(histogramType, min, max, bucketCount, optArgCount == 3);
  if (NS_FAILED(rv)) {
    return rv;
  }

  KeyedHistogram* keyed = new KeyedHistogram(name, expiration, histogramType,
                                             min, max, bucketCount,
                                             nsITelemetry::DATASET_RELEASE_CHANNEL_OPTIN);
  if (MOZ_UNLIKELY(!gKeyedHistograms.Put(name, keyed, mozilla::fallible))) {
    delete keyed;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return WrapAndReturnKeyedHistogram(keyed, cx, ret);

}

nsresult
HistogramFrom(const nsACString &name, const nsACString &existing_name,
              JSContext *cx, JS::MutableHandle<JS::Value> ret)
{
   mozilla::Telemetry::ID id;
  nsresult rv = ::GetHistogramEnumId(PromiseFlatCString(existing_name).get(), &id);
  if (NS_FAILED(rv)) {
    return rv;
  }

  Histogram* clone = CloneHistogram(name, id);
  if (!clone) {
    return NS_ERROR_FAILURE;
  }

  return WrapAndReturnHistogram(clone, cx, ret);
}

nsresult
CreateHistogramSnapshots(JSContext *cx,
                         JS::MutableHandle<JS::Value> ret,
                         bool subsession,
                         bool clearSubsession)
{
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
         = GetHistogramByEnumId(mozilla::Telemetry::ID(i), &h);
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
  IdentifyCorruptHistograms(hs);

  // OK, now we can actually reflect things.
  JS::Rooted<JSObject*> hobj(cx);
  for (HistogramIterator it = hs.begin(); it != hs.end(); ++it) {
    Histogram *h = *it;
    if (!ShouldReflectHistogram(h) || IsEmpty(h) || IsExpired(h)) {
      continue;
    }

    Histogram* original = h;
#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
    if (subsession) {
      h = GetSubsessionHistogram(*h);
      if (!h) {
        continue;
      }
    }
#endif

    hobj = JS_NewPlainObject(cx);
    if (!hobj) {
      return NS_ERROR_FAILURE;
    }
    switch (ReflectHistogramSnapshot(cx, hobj, h)) {
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
RegisteredHistograms(uint32_t aDataset, uint32_t *aCount,
                     char*** aHistograms)
{
  return GetRegisteredHistogramIds(false, aDataset, aCount, aHistograms);
}

nsresult
RegisteredKeyedHistograms(uint32_t aDataset, uint32_t *aCount,
                          char*** aHistograms)
{
  return GetRegisteredHistogramIds(true, aDataset, aCount, aHistograms);
}

nsresult
GetKeyedHistogramSnapshots(JSContext *cx, JS::MutableHandle<JS::Value> ret)
{
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
RegisterAddonHistogram(const nsACString &id, const nsACString &name,
                       uint32_t histogramType, uint32_t min, uint32_t max,
                       uint32_t bucketCount, uint8_t optArgCount)
{
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
GetAddonHistogram(const nsACString &id, const nsACString &name,
                  JSContext *cx, JS::MutableHandle<JS::Value> ret)
{
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

  AddonHistogramInfo &info = histogramEntry->mData;
  if (!info.h) {
    nsAutoCString actualName;
    AddonHistogramName(id, name, actualName);
    if (!::CreateHistogramForAddon(actualName, info)) {
      return NS_ERROR_FAILURE;
    }
  }
  return WrapAndReturnHistogram(info.h, cx, ret);
}

nsresult
UnregisterAddonHistograms(const nsACString &id)
{
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
GetAddonHistogramSnapshots(JSContext *cx, JS::MutableHandle<JS::Value> ret)
{
  JS::Rooted<JSObject*> obj(cx, JS_NewPlainObject(cx));
  if (!obj) {
    return NS_ERROR_FAILURE;
  }

  if (!gAddonMap.ReflectIntoJS(AddonReflector, cx, obj)) {
    return NS_ERROR_FAILURE;
  }
  ret.setObject(*obj);
  return NS_OK;
}

size_t
GetMapShallowSizesOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf)
{
  return gAddonMap.ShallowSizeOfExcludingThis(aMallocSizeOf) +
         gHistogramMap.ShallowSizeOfExcludingThis(aMallocSizeOf);
}

size_t
GetHistogramSizesofIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
{
  StatisticsRecorder::Histograms hs;
  StatisticsRecorder::GetHistograms(&hs);
  size_t n = 0;
  for (HistogramIterator it = hs.begin(); it != hs.end(); ++it) {
    Histogram *h = *it;
    n += h->SizeOfIncludingThis(aMallocSizeOf);
  }
  return n;
}

} // namespace TelemetryHistogram
