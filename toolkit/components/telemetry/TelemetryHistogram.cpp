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
#include "nsPrintfCString.h"

#include "mozilla/dom/ToJSValue.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/Atomics.h"
#include "mozilla/JSONWriter.h"
#include "mozilla/StartupTimeline.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/Unused.h"

#include "TelemetryCommon.h"
#include "TelemetryHistogram.h"
#include "TelemetryScalar.h"
#include "ipc/TelemetryIPCAccumulator.h"

#include "base/histogram.h"

#include <limits>

using base::Histogram;
using base::BooleanHistogram;
using base::CountHistogram;
using base::FlagHistogram;
using base::LinearHistogram;
using mozilla::MakeTuple;
using mozilla::StaticMutex;
using mozilla::StaticMutexAutoLock;
using mozilla::Telemetry::HistogramAccumulation;
using mozilla::Telemetry::KeyedHistogramAccumulation;
using mozilla::Telemetry::HistogramID;
using mozilla::Telemetry::ProcessID;
using mozilla::Telemetry::HistogramCount;
using mozilla::Telemetry::Common::LogToBrowserConsole;
using mozilla::Telemetry::Common::RecordedProcessType;
using mozilla::Telemetry::Common::AutoHashtable;
using mozilla::Telemetry::Common::GetNameForProcessID;
using mozilla::Telemetry::Common::GetIDForProcessName;
using mozilla::Telemetry::Common::IsExpiredVersion;
using mozilla::Telemetry::Common::CanRecordDataset;
using mozilla::Telemetry::Common::CanRecordProduct;
using mozilla::Telemetry::Common::SupportedProduct;
using mozilla::Telemetry::Common::IsInDataset;
using mozilla::Telemetry::Common::ToJSString;

namespace TelemetryIPCAccumulator = mozilla::TelemetryIPCAccumulator;

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
//   |gTelemetryHistogramMutex|, except for GetKeyedHistogramSnapshots and
//   CreateHistogramSnapshots.
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

// This is a StaticMutex rather than a plain Mutex (1) so that
// it gets initialised in a thread-safe manner the first time
// it is used, and (2) because it is never de-initialised, and
// a normal Mutex would show up as a leak in BloatView.  StaticMutex
// also has the "OffTheBooks" property, so it won't show as a leak
// in BloatView.
static StaticMutex gTelemetryHistogramMutex;


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE TYPES

namespace {

typedef nsDataHashtable<nsCStringHashKey, HistogramID> StringToHistogramIdMap;

// Hardcoded probes
struct HistogramInfo {
  uint32_t min;
  uint32_t max;
  uint32_t bucketCount;
  uint32_t histogramType;
  uint32_t name_offset;
  uint32_t expiration_offset;
  uint32_t dataset;
  uint32_t label_index;
  uint32_t label_count;
  uint32_t key_index;
  uint32_t key_count;
  RecordedProcessType record_in_processes;
  bool keyed;
  SupportedProduct products;

  const char *name() const;
  const char *expiration() const;
  nsresult label_id(const char* label, uint32_t* labelId) const;
  bool allows_key(const nsACString& key) const;
};

// Structs used to keep information about the histograms for which a
// snapshot should be created.
struct HistogramSnapshotData {
  nsTArray<Histogram::Sample> mBucketRanges;
  nsTArray<Histogram::Count> mBucketCounts;
  int64_t mSampleSum; // Same type as Histogram::SampleSet::sum_
};

struct HistogramSnapshotInfo {
  HistogramSnapshotData data;
  HistogramID histogramID;
};

typedef mozilla::Vector<HistogramSnapshotInfo> HistogramSnapshotsArray;
typedef mozilla::Vector<HistogramSnapshotsArray> HistogramProcessSnapshotsArray;

// The following is used to handle snapshot information for keyed histograms.
typedef nsDataHashtable<nsCStringHashKey, HistogramSnapshotData> KeyedHistogramSnapshotData;

struct KeyedHistogramSnapshotInfo {
  KeyedHistogramSnapshotData data;
  HistogramID histogramId;
};

typedef mozilla::Vector<KeyedHistogramSnapshotInfo> KeyedHistogramSnapshotsArray;
typedef mozilla::Vector<KeyedHistogramSnapshotsArray> KeyedHistogramProcessSnapshotsArray;

class KeyedHistogram {
public:
  KeyedHistogram(HistogramID id, const HistogramInfo& info);
  ~KeyedHistogram();
  nsresult GetHistogram(const nsCString& name, Histogram** histogram);
  Histogram* GetHistogram(const nsCString& name);
  uint32_t GetHistogramType() const { return mHistogramInfo.histogramType; }
  nsresult GetJSKeys(JSContext* cx, JS::CallArgs& args);
  // Note: unlike other methods, GetJSSnapshot is thread safe.
  nsresult GetJSSnapshot(JSContext* cx, JS::Handle<JSObject*> obj,
                         bool clearSubsession);
  nsresult GetSnapshot(const StaticMutexAutoLock& aLock,
                       KeyedHistogramSnapshotData& aSnapshot, bool aClearSubsession);

  nsresult Add(const nsCString& key, uint32_t aSample, ProcessID aProcessType);
  void Clear();

  HistogramID GetHistogramID() const { return mId; }

  bool IsEmpty() const { return mHistogramMap.IsEmpty(); }

private:
  typedef nsBaseHashtableET<nsCStringHashKey, Histogram*> KeyedHistogramEntry;
  typedef AutoHashtable<KeyedHistogramEntry> KeyedHistogramMapType;
  KeyedHistogramMapType mHistogramMap;

  const HistogramID mId;
  const HistogramInfo& mHistogramInfo;
};

} // namespace


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE STATE, SHARED BY ALL THREADS

namespace {

// Set to true once this global state has been initialized
bool gInitDone = false;

// Whether we are collecting the base, opt-out, Histogram data.
bool gCanRecordBase = false;
// Whether we are collecting the extended, opt-in, Histogram data.
bool gCanRecordExtended = false;

// The storage for actual Histogram instances.
// We use separate ones for plain and keyed histograms.
Histogram** gHistogramStorage;
// Keyed histograms internally map string keys to individual Histogram instances.
KeyedHistogram** gKeyedHistogramStorage;

// Cache of histogram name to a histogram id.
StringToHistogramIdMap gNameToHistogramIDMap(HistogramCount);

// To simplify logic below we use a single histogram instance for all expired histograms.
Histogram* gExpiredHistogram = nullptr;

// This tracks whether recording is enabled for specific histograms.
// To utilize C++ initialization rules, we invert the meaning to "disabled".
bool gHistogramRecordingDisabled[HistogramCount] = {};

// This is for gHistogramInfos, gHistogramStringTable
#include "TelemetryHistogramData.inc"

} // namespace


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE CONSTANTS

namespace {

// List of histogram IDs which should have recording disabled initially.
const HistogramID kRecordingInitiallyDisabledIDs[] = {
  mozilla::Telemetry::FX_REFRESH_DRIVER_SYNC_SCROLL_FRAME_DELAY_MS,

  // The array must not be empty. Leave these item here.
  mozilla::Telemetry::TELEMETRY_TEST_COUNT_INIT_NO_RECORD,
  mozilla::Telemetry::TELEMETRY_TEST_KEYED_COUNT_INIT_NO_RECORD
};

} // namespace

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// The core storage access functions.
// They wrap access to the histogram storage and lookup caches.

namespace {

size_t internal_KeyedHistogramStorageIndex(HistogramID aHistogramId,
                                           ProcessID aProcessId)
{
  return aHistogramId * size_t(ProcessID::Count) + size_t(aProcessId);
}

size_t internal_HistogramStorageIndex(const StaticMutexAutoLock& aLock,
                                      HistogramID aHistogramId,
                                      ProcessID aProcessId)
{
  static_assert(
    HistogramCount <
      std::numeric_limits<size_t>::max() / size_t(ProcessID::Count),
        "Too many histograms and processes to store in a 1D array.");

  return aHistogramId * size_t(ProcessID::Count) + size_t(aProcessId);
}

Histogram* internal_GetHistogramFromStorage(const StaticMutexAutoLock& aLock,
                                            HistogramID aHistogramId,
                                            ProcessID aProcessId)
{
  size_t index = internal_HistogramStorageIndex(aLock, aHistogramId, aProcessId);
  return gHistogramStorage[index];
}

void internal_SetHistogramInStorage(const StaticMutexAutoLock& aLock,
                                    HistogramID aHistogramId,
                                    ProcessID aProcessId,
                                    Histogram* aHistogram)
{
  MOZ_ASSERT(XRE_IsParentProcess(),
    "Histograms are stored only in the parent process.");

  size_t index = internal_HistogramStorageIndex(aLock, aHistogramId, aProcessId);
  MOZ_ASSERT(!gHistogramStorage[index],
    "Mustn't overwrite storage without clearing it first.");
  gHistogramStorage[index] = aHistogram;
}

KeyedHistogram* internal_GetKeyedHistogramFromStorage(HistogramID aHistogramId,
                                                      ProcessID aProcessId)
{
  size_t index = internal_KeyedHistogramStorageIndex(aHistogramId, aProcessId);
  return gKeyedHistogramStorage[index];
}

void internal_SetKeyedHistogramInStorage(HistogramID aHistogramId,
                                         ProcessID aProcessId,
                                         KeyedHistogram* aKeyedHistogram)
{
  MOZ_ASSERT(XRE_IsParentProcess(),
    "Keyed Histograms are stored only in the parent process.");

  size_t index = internal_KeyedHistogramStorageIndex(aHistogramId, aProcessId);
  MOZ_ASSERT(!gKeyedHistogramStorage[index],
    "Mustn't overwrite storage without clearing it first");
  gKeyedHistogramStorage[index] = aKeyedHistogram;
}

// Factory function for histogram instances.
Histogram*
internal_CreateHistogramInstance(const HistogramInfo& info, int bucketsOffset);

bool
internal_IsHistogramEnumId(HistogramID aID)
{
  static_assert(((HistogramID)-1 > 0), "ID should be unsigned.");
  return aID < HistogramCount;
}

// Look up a plain histogram by id.
Histogram*
internal_GetHistogramById(const StaticMutexAutoLock& aLock,
                          HistogramID histogramId,
                          ProcessID processId,
                          bool instantiate = true)
{
  MOZ_ASSERT(internal_IsHistogramEnumId(histogramId));
  MOZ_ASSERT(!gHistogramInfos[histogramId].keyed);
  MOZ_ASSERT(processId < ProcessID::Count);

  Histogram* h = internal_GetHistogramFromStorage(aLock, histogramId, processId);
  if (h || !instantiate) {
    return h;
  }

  const HistogramInfo& info = gHistogramInfos[histogramId];
  const int bucketsOffset = gHistogramBucketLowerBoundIndex[histogramId];
  h = internal_CreateHistogramInstance(info, bucketsOffset);
  MOZ_ASSERT(h);
  internal_SetHistogramInStorage(aLock, histogramId, processId, h);
  return h;
}

// Look up a keyed histogram by id.
KeyedHistogram*
internal_GetKeyedHistogramById(HistogramID histogramId, ProcessID processId,
                               bool instantiate = true)
{
  MOZ_ASSERT(internal_IsHistogramEnumId(histogramId));
  MOZ_ASSERT(gHistogramInfos[histogramId].keyed);
  MOZ_ASSERT(processId < ProcessID::Count);

  KeyedHistogram* kh = internal_GetKeyedHistogramFromStorage(histogramId,
                                                             processId);
  if (kh || !instantiate) {
    return kh;
  }

  const HistogramInfo& info = gHistogramInfos[histogramId];
  kh = new KeyedHistogram(histogramId, info);
  internal_SetKeyedHistogramInStorage(histogramId, processId, kh);

  return kh;
}

// Look up a histogram id from a histogram name.
nsresult
internal_GetHistogramIdByName(const StaticMutexAutoLock& aLock,
                              const nsACString& name,
                              HistogramID* id)
{
  const bool found = gNameToHistogramIDMap.Get(name, id);
  if (!found) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  return NS_OK;
}

// Clear a histogram from storage.
void
internal_ClearHistogramById(const StaticMutexAutoLock& aLock,
                            HistogramID histogramId,
                            ProcessID processId)
{
  size_t index = internal_HistogramStorageIndex(aLock, histogramId, processId);
  if (gHistogramStorage[index] == gExpiredHistogram) {
    // We keep gExpiredHistogram until TelemetryHistogram::DeInitializeGlobalState
    return;
  }
  delete gHistogramStorage[index];
  gHistogramStorage[index] = nullptr;
}

}

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
internal_AttemptedGPUProcess() {
  // Check if it was tried to launch a process.
  bool attemptedGPUProcess = false;
  if (auto gpm = mozilla::gfx::GPUProcessManager::Get()) {
    attemptedGPUProcess = gpm->AttemptedGPUProcess();
  }
  return attemptedGPUProcess;
}

// Note: this is completely unrelated to mozilla::IsEmpty.
bool
internal_IsEmpty(const StaticMutexAutoLock& aLock, const Histogram *h)
{
  return h->is_empty();
}

bool
internal_IsExpired(const StaticMutexAutoLock& aLock, Histogram* h)
{
  return h == gExpiredHistogram;
}

void
internal_SetHistogramRecordingEnabled(const StaticMutexAutoLock& aLock,
                                      HistogramID id,
                                      bool aEnabled)
{
  MOZ_ASSERT(internal_IsHistogramEnumId(id));
  gHistogramRecordingDisabled[id] = !aEnabled;
}

bool
internal_IsRecordingEnabled(HistogramID id)
{
  MOZ_ASSERT(internal_IsHistogramEnumId(id));
  return !gHistogramRecordingDisabled[id];
}

const char *
HistogramInfo::name() const
{
  return &gHistogramStringTable[this->name_offset];
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

bool
HistogramInfo::allows_key(const nsACString& key) const
{
  MOZ_ASSERT(this->keyed);

  // If we didn't specify a list of allowed keys, just return true.
  if (this->key_count == 0) {
    return true;
  }

  // Otherwise, check if |key| is in the list of allowed keys.
  for (uint32_t i = 0; i < this->key_count; ++i) {
    // gHistogramKeyTable contains the indices of the key strings in the
    // gHistogramStringTable. They are stored in-order and consecutively,
    // from the offset key_index to (key_index + key_count).
    uint32_t string_offset = gHistogramKeyTable[this->key_index + i];
    const char* const str = &gHistogramStringTable[string_offset];
    if (key.EqualsASCII(str)) {
      return true;
    }
  }

  // |key| was not found.
  return false;
}

} // namespace


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE: Histogram Get, Add, Clone, Clear functions

namespace {

nsresult
internal_CheckHistogramArguments(const HistogramInfo& info)
{
  if (info.histogramType != nsITelemetry::HISTOGRAM_BOOLEAN
      && info.histogramType != nsITelemetry::HISTOGRAM_FLAG
      && info.histogramType != nsITelemetry::HISTOGRAM_COUNT) {
    // Sanity checks for histogram parameters.
    if (info.min >= info.max) {
      return NS_ERROR_ILLEGAL_VALUE;
    }

    if (info.bucketCount <= 2) {
      return NS_ERROR_ILLEGAL_VALUE;
    }

    if (info.min < 1) {
      return NS_ERROR_ILLEGAL_VALUE;
    }
  }

  return NS_OK;
}

Histogram*
internal_CreateHistogramInstance(const HistogramInfo& passedInfo, int bucketsOffset)
{
  if (NS_FAILED(internal_CheckHistogramArguments(passedInfo))) {
    MOZ_ASSERT(false, "Failed histogram argument checks.");
    return nullptr;
  }

  // To keep the code simple we map all the calls to expired histograms to the same histogram instance.
  // We create that instance lazily when needed.
  const bool isExpired = IsExpiredVersion(passedInfo.expiration());
  HistogramInfo info = passedInfo;
  const int* buckets = &gHistogramBucketLowerBounds[bucketsOffset];

  if (isExpired) {
    if (gExpiredHistogram) {
      return gExpiredHistogram;
    }

    // The first values in gHistogramBucketLowerBounds are reserved for
    // expired histograms.
    buckets = gHistogramBucketLowerBounds;
    info.min = 1;
    info.max = 2;
    info.bucketCount = 3;
    info.histogramType = nsITelemetry::HISTOGRAM_LINEAR;
  }

  Histogram::Flags flags = Histogram::kNoFlags;
  Histogram* h = nullptr;
  switch (info.histogramType) {
  case nsITelemetry::HISTOGRAM_EXPONENTIAL:
    h = Histogram::FactoryGet(info.min, info.max, info.bucketCount, flags, buckets);
    break;
  case nsITelemetry::HISTOGRAM_LINEAR:
  case nsITelemetry::HISTOGRAM_CATEGORICAL:
    h = LinearHistogram::FactoryGet(info.min, info.max, info.bucketCount, flags, buckets);
    break;
  case nsITelemetry::HISTOGRAM_BOOLEAN:
    h = BooleanHistogram::FactoryGet(flags, buckets);
    break;
  case nsITelemetry::HISTOGRAM_FLAG:
    h = FlagHistogram::FactoryGet(flags, buckets);
    break;
  case nsITelemetry::HISTOGRAM_COUNT:
    h = CountHistogram::FactoryGet(flags, buckets);
    break;
  default:
    MOZ_ASSERT(false, "Invalid histogram type");
    return nullptr;
  }

  if (isExpired) {
    gExpiredHistogram = h;
  }

  return h;
}

nsresult
internal_HistogramAdd(const StaticMutexAutoLock& aLock,
                      Histogram& histogram,
                      const HistogramID id,
                      uint32_t value,
                      ProcessID aProcessType)
{
  // Check if we are allowed to record the data.
  bool canRecordDataset = CanRecordDataset(gHistogramInfos[id].dataset,
                                           internal_CanRecordBase(),
                                           internal_CanRecordExtended());
  // If `histogram` is a non-parent-process histogram, then recording-enabled
  // has been checked in its owner process.
  if (!canRecordDataset ||
    (aProcessType == ProcessID::Parent && !internal_IsRecordingEnabled(id))) {
    return NS_OK;
  }

  // Don't record if the current platform is not enabled
  if (!CanRecordProduct(gHistogramInfos[id].products)) {
    return NS_OK;
  }

  // The internal representation of a base::Histogram's buckets uses `int`.
  // Clamp large values of `value` to be INT_MAX so they continue to be treated
  // as large values (instead of negative ones).
  if (value > INT_MAX) {
    TelemetryScalar::Add(
      mozilla::Telemetry::ScalarID::TELEMETRY_ACCUMULATE_CLAMPED_VALUES,
      NS_ConvertASCIItoUTF16(gHistogramInfos[id].name()), 1);
    value = INT_MAX;
  }

  histogram.Add(value);

  return NS_OK;
}

} // namespace

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE: Histogram reflection helpers

namespace {

/**
 * Copy histograms and samples to Mozilla-friendly structures.
 * Please note that this version does not make use of JS contexts.
 *
 * @param {StaticMutexAutoLock} the proof we hold the mutex.
 * @param {Histogram} the histogram to reflect.
 * @return {nsresult} NS_ERROR_FAILURE if we fail to allocate memory for the snapshot.
 */
nsresult
internal_GetHistogramAndSamples(const StaticMutexAutoLock& aLock,
                                const Histogram *h,
                                HistogramSnapshotData& aSnapshot)
{
  MOZ_ASSERT(h);

  // Convert the ranges of the buckets to a nsTArray.
  const size_t bucketCount = h->bucket_count();
  for (size_t i = 0; i < bucketCount; i++) {
    if (!aSnapshot.mBucketRanges.AppendElement(h->ranges(i))) {
      return NS_ERROR_FAILURE;
    }
  }

  // Get a snapshot of the samples.
  Histogram::SampleSet ss;
  h->SnapshotSample(&ss);

  // Get the number of samples in each bucket.
  for (size_t i = 0; i < bucketCount; i++) {
    if (!aSnapshot.mBucketCounts.AppendElement(ss.counts(i))) {
      return NS_ERROR_FAILURE;
    }
  }

  // Finally, save the |sum|. We don't need to reflect declared_min, declared_max and
  // histogram_type as they are in gHistogramInfo.
  aSnapshot.mSampleSum = ss.sum();
  return NS_OK;
}

nsresult
internal_ReflectHistogramAndSamples(JSContext *cx,
                                    JS::Handle<JSObject*> obj,
                                    const HistogramInfo& aHistogramInfo,
                                    const HistogramSnapshotData& aSnapshot)
{
  if (!(JS_DefineProperty(cx, obj, "min",
                          aHistogramInfo.min, JSPROP_ENUMERATE)
        && JS_DefineProperty(cx, obj, "max",
                             aHistogramInfo.max, JSPROP_ENUMERATE)
        && JS_DefineProperty(cx, obj, "histogram_type",
                             aHistogramInfo.histogramType, JSPROP_ENUMERATE)
        && JS_DefineProperty(cx, obj, "sum",
                             double(aSnapshot.mSampleSum), JSPROP_ENUMERATE))) {
    return NS_ERROR_FAILURE;
  }

  // Don't rely on the bucket counts from "aHistogramInfo": it may
  // differ from the length of aSnapshot.mBucketCounts due to expired
  // histograms.
  const size_t count = aSnapshot.mBucketCounts.Length();
  MOZ_ASSERT(count == aSnapshot.mBucketRanges.Length(),
             "The number of buckets and the number of counts must match.");

  // Create the "ranges" property and add it to the final object.
  JS::Rooted<JSObject*> rarray(cx, JS_NewArrayObject(cx, count));
  if (!rarray
      || !JS_DefineProperty(cx, obj, "ranges", rarray, JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }

  // Fill the "ranges" property.
  for (size_t i = 0; i < count; i++) {
    if (!JS_DefineElement(cx, rarray, i, aSnapshot.mBucketRanges[i], JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
  }

  JS::Rooted<JSObject*> counts_array(cx, JS_NewArrayObject(cx, count));
  if (!counts_array
      || !JS_DefineProperty(cx, obj, "counts", counts_array, JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }

  // Fill the "counts" property.
  for (size_t i = 0; i < count; i++) {
    if (!JS_DefineElement(cx, counts_array, i, aSnapshot.mBucketCounts[i], JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

bool
internal_ShouldReflectHistogram(const StaticMutexAutoLock& aLock, Histogram* h, HistogramID id)
{
  // Only flag histograms are serialized when they are empty.
  // This has historical reasons, changing this will require downstream changes.
  // The cheaper path here is to just deprecate flag histograms in favor
  // of scalars.
  uint32_t type = gHistogramInfos[id].histogramType;
  if (internal_IsEmpty(aLock, h) && type != nsITelemetry::HISTOGRAM_FLAG) {
    return false;
  }

  // Don't reflect the histogram if it's not allowed in this product.
  if (!CanRecordProduct(gHistogramInfos[id].products)) {
    return false;
  }

  return true;
}

/**
 * Helper function to get a snapshot of the histograms.
 *
 * @param {aLock} the lock proof.
 * @param {aDataset} the dataset for which the snapshot is being requested.
 * @param {aClearSubsession} whether or not to clear the data after
 *        taking the snapshot.
 * @param {aIncludeGPU} whether or not to include data for the GPU.
 * @param {aOutSnapshot} the container in which the snapshot data will be stored.
 * @return {nsresult} NS_OK if the snapshot was successfully taken or
 *         NS_ERROR_OUT_OF_MEMORY if it failed to allocate memory.
 */
nsresult
internal_GetHistogramsSnapshot(const StaticMutexAutoLock& aLock,
                               unsigned int aDataset,
                               bool aClearSubsession,
                               bool aIncludeGPU,
                               HistogramProcessSnapshotsArray& aOutSnapshot)
{
  if (!aOutSnapshot.resize(static_cast<uint32_t>(ProcessID::Count))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (uint32_t process = 0; process < static_cast<uint32_t>(ProcessID::Count); ++process) {
    HistogramSnapshotsArray& hArray = aOutSnapshot[process];

    for (size_t i = 0; i < HistogramCount; ++i) {
      const HistogramInfo& info = gHistogramInfos[i];
      if (info.keyed) {
        continue;
      }

      HistogramID id = HistogramID(i);

      if (!CanRecordInProcess(info.record_in_processes, ProcessID(process)) ||
          ((ProcessID(process) == ProcessID::Gpu) && !aIncludeGPU)) {
        continue;
      }

      if (!IsInDataset(info.dataset, aDataset)) {
        continue;
      }

      bool shouldInstantiate =
        info.histogramType == nsITelemetry::HISTOGRAM_FLAG;
      Histogram* h = internal_GetHistogramById(aLock, id, ProcessID(process),
                                               shouldInstantiate);
      if (!h || internal_IsExpired(aLock, h) || !internal_ShouldReflectHistogram(aLock, h, id)) {
        continue;
      }

      HistogramSnapshotData snapshotData;
      if (NS_FAILED(internal_GetHistogramAndSamples(aLock, h, snapshotData))) {
        continue;
      }

      if (!hArray.emplaceBack(HistogramSnapshotInfo{snapshotData, id})) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      if (aClearSubsession) {
        h->Clear();
      }
    }
  }
  return NS_OK;
}

} // namespace

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE: class KeyedHistogram and internal_ReflectKeyedHistogram

namespace {

nsresult
internal_ReflectKeyedHistogram(const KeyedHistogramSnapshotData& aSnapshot,
                               const HistogramInfo& info,
                               JSContext* aCx, JS::Handle<JSObject*> aObj)
{
  for (auto iter = aSnapshot.ConstIter(); !iter.Done(); iter.Next()) {
    HistogramSnapshotData& keyData = iter.Data();

    JS::RootedObject histogramSnapshot(aCx, JS_NewPlainObject(aCx));
    if (!histogramSnapshot) {
      return NS_ERROR_FAILURE;
    }

    if (NS_FAILED(internal_ReflectHistogramAndSamples(aCx, histogramSnapshot,
                                                      info,
                                                      keyData))) {
      return NS_ERROR_FAILURE;
    }

    const NS_ConvertUTF8toUTF16 key(iter.Key());
    if (!JS_DefineUCProperty(aCx, aObj, key.Data(), key.Length(),
                             histogramSnapshot, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

KeyedHistogram::KeyedHistogram(HistogramID id, const HistogramInfo& info)
  : mHistogramMap()
  , mId(id)
  , mHistogramInfo(info)
{
}

KeyedHistogram::~KeyedHistogram()
{
  for (auto iter = mHistogramMap.Iter(); !iter.Done(); iter.Next()) {
    Histogram* h = iter.Get()->mData;
    if (h == gExpiredHistogram) {
      continue;
    }
    delete h;
  }
  mHistogramMap.Clear();
}

nsresult
KeyedHistogram::GetHistogram(const nsCString& key, Histogram** histogram)
{
  KeyedHistogramEntry* entry = mHistogramMap.GetEntry(key);
  if (entry) {
    *histogram = entry->mData;
    return NS_OK;
  }

  int bucketsOffset = gHistogramBucketLowerBoundIndex[mId];
  Histogram* h = internal_CreateHistogramInstance(mHistogramInfo, bucketsOffset);
  if (!h) {
    return NS_ERROR_FAILURE;
  }

  h->ClearFlags(Histogram::kUmaTargetedHistogramFlag);
  *histogram = h;

  entry = mHistogramMap.PutEntry(key);
  if (MOZ_UNLIKELY(!entry)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  entry->mData = h;
  return NS_OK;
}

Histogram*
KeyedHistogram::GetHistogram(const nsCString& key)
{
  Histogram* h = nullptr;
  if (NS_FAILED(GetHistogram(key, &h))) {
    return nullptr;
  }
  return h;
}

nsresult
KeyedHistogram::Add(const nsCString& key, uint32_t sample,
                    ProcessID aProcessType)
{
  bool canRecordDataset = CanRecordDataset(mHistogramInfo.dataset,
                                           internal_CanRecordBase(),
                                           internal_CanRecordExtended());
  // If `histogram` is a non-parent-process histogram, then recording-enabled
  // has been checked in its owner process.
  if (!canRecordDataset ||
    (aProcessType == ProcessID::Parent && !internal_IsRecordingEnabled(mId))) {
    return NS_OK;
  }

  // Don't record if the current platform is not enabled
  if (!CanRecordProduct(gHistogramInfos[mId].products)) {
    return NS_OK;
  }

  Histogram* histogram = GetHistogram(key);
  MOZ_ASSERT(histogram);
  if (!histogram) {
    return NS_ERROR_FAILURE;
  }

  // The internal representation of a base::Histogram's buckets uses `int`.
  // Clamp large values of `sample` to be INT_MAX so they continue to be treated
  // as large values (instead of negative ones).
  if (sample > INT_MAX) {
    TelemetryScalar::Add(
      mozilla::Telemetry::ScalarID::TELEMETRY_ACCUMULATE_CLAMPED_VALUES,
      NS_ConvertASCIItoUTF16(mHistogramInfo.name()), 1);
    sample = INT_MAX;
  }

  histogram->Add(sample);
  return NS_OK;
}

void
KeyedHistogram::Clear()
{
  MOZ_ASSERT(XRE_IsParentProcess());
  if (!XRE_IsParentProcess()) {
    return;
  }

  for (auto iter = mHistogramMap.Iter(); !iter.Done(); iter.Next()) {
    Histogram* h = iter.Get()->mData;
    if (h == gExpiredHistogram) {
      continue;
    }
    delete h;
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
    jsKey.setString(ToJSString(cx, iter.Get()->GetKey()));
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

nsresult
KeyedHistogram::GetJSSnapshot(JSContext* cx, JS::Handle<JSObject*> obj, bool clearSubsession)
{
  // Get a snapshot of the data.
  KeyedHistogramSnapshotData dataSnapshot;
  {
    StaticMutexAutoLock locker(gTelemetryHistogramMutex);
    MOZ_ASSERT(internal_IsHistogramEnumId(mId));

    // Take a snapshot of the data here, protected by the lock, and then,
    // outside of the lock protection, mirror it to a JS structure.
    if (NS_FAILED(GetSnapshot(locker, dataSnapshot, clearSubsession))) {
      return NS_ERROR_FAILURE;
    }
  }

  // Now that we have a copy of the data, mirror it to JS.
  return internal_ReflectKeyedHistogram(dataSnapshot, gHistogramInfos[mId], cx, obj);
}

nsresult
KeyedHistogram::GetSnapshot(const StaticMutexAutoLock& aLock,
                            KeyedHistogramSnapshotData& aSnapshot, bool aClearSubsession)
{
  // Snapshot every key.
  for (auto iter = mHistogramMap.ConstIter(); !iter.Done(); iter.Next()) {
    Histogram* keyData = iter.Get()->mData;
    if (!keyData) {
      return NS_ERROR_FAILURE;
    }

    HistogramSnapshotData keySnapshot;
    if (NS_FAILED(internal_GetHistogramAndSamples(aLock, keyData, keySnapshot))) {
      return NS_ERROR_FAILURE;
    }

    // Append to the final snapshot.
    aSnapshot.Put(iter.Get()->GetKey(), mozilla::Move(keySnapshot));
  }

  if (aClearSubsession) {
    Clear();
  }

  return NS_OK;
}


/**
 * Helper function to get a snapshot of the keyed histograms.
 *
 * @param {aLock} the lock proof.
 * @param {aDataset} the dataset for which the snapshot is being requested.
 * @param {aClearSubsession} whether or not to clear the data after
 *        taking the snapshot.
 * @param {aIncludeGPU} whether or not to include data for the GPU.
 * @param {aOutSnapshot} the container in which the snapshot data will be stored.
 * @param {aSkipEmpty} whether or not to skip empty keyed histograms from the
 *        snapshot. Can't always assume "true" for consistency with the other
 *        callers.
 * @return {nsresult} NS_OK if the snapshot was successfully taken or
 *         NS_ERROR_OUT_OF_MEMORY if it failed to allocate memory.
 */
nsresult
internal_GetKeyedHistogramsSnapshot(const StaticMutexAutoLock& aLock,
                                    unsigned int aDataset,
                                    bool aClearSubsession,
                                    bool aIncludeGPU,
                                    KeyedHistogramProcessSnapshotsArray& aOutSnapshot,
                                    bool aSkipEmpty = false)
{
  if (!aOutSnapshot.resize(static_cast<uint32_t>(ProcessID::Count))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (uint32_t process = 0; process < static_cast<uint32_t>(ProcessID::Count); ++process) {
    KeyedHistogramSnapshotsArray& hArray = aOutSnapshot[process];

    for (size_t i = 0; i < HistogramCount; ++i) {
      HistogramID id = HistogramID(i);
      const HistogramInfo& info = gHistogramInfos[id];
      if (!info.keyed) {
        continue;
      }

      if (!CanRecordInProcess(info.record_in_processes, ProcessID(process)) ||
        ((ProcessID(process) == ProcessID::Gpu) && !aIncludeGPU)) {
        continue;
      }

      if (!IsInDataset(info.dataset, aDataset)) {
        continue;
      }

      KeyedHistogram* keyed = internal_GetKeyedHistogramById(id,
                                                             ProcessID(process),
                                                             /* instantiate = */ false);
      if (!keyed || (aSkipEmpty && keyed->IsEmpty())) {
        continue;
      }

      // Take a snapshot of the keyed histogram data!
      KeyedHistogramSnapshotData snapshot;
      if (!NS_SUCCEEDED(keyed->GetSnapshot(aLock, snapshot, aClearSubsession))) {
        return NS_ERROR_FAILURE;
      }

      if (!hArray.emplaceBack(KeyedHistogramSnapshotInfo{mozilla::Move(snapshot), id})) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }
  return NS_OK;
}

} // namespace

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE: thread-unsafe helpers for the external interface

namespace {

bool
internal_RemoteAccumulate(const StaticMutexAutoLock& aLock, HistogramID aId, uint32_t aSample)
{
  if (XRE_IsParentProcess()) {
    return false;
  }

  if (!internal_IsRecordingEnabled(aId)) {
    return true;
  }

  TelemetryIPCAccumulator::AccumulateChildHistogram(aId, aSample);
  return true;
}

bool
internal_RemoteAccumulate(const StaticMutexAutoLock& aLock, HistogramID aId,
                          const nsCString& aKey, uint32_t aSample)
{
  if (XRE_IsParentProcess()) {
    return false;
  }

  if (!internal_IsRecordingEnabled(aId)) {
    return true;
  }

  TelemetryIPCAccumulator::AccumulateChildKeyedHistogram(aId, aKey, aSample);
  return true;
}

void internal_Accumulate(const StaticMutexAutoLock& aLock, HistogramID aId, uint32_t aSample)
{
  if (!internal_CanRecordBase() ||
      internal_RemoteAccumulate(aLock, aId, aSample)) {
    return;
  }

  Histogram *h = internal_GetHistogramById(aLock, aId, ProcessID::Parent);
  MOZ_ASSERT(h);
  internal_HistogramAdd(aLock, *h, aId, aSample, ProcessID::Parent);
}

void
internal_Accumulate(const StaticMutexAutoLock& aLock, HistogramID aId,
                    const nsCString& aKey, uint32_t aSample)
{
  if (!gInitDone || !internal_CanRecordBase() ||
      internal_RemoteAccumulate(aLock, aId, aKey, aSample)) {
    return;
  }

  KeyedHistogram* keyed = internal_GetKeyedHistogramById(aId, ProcessID::Parent);
  MOZ_ASSERT(keyed);
  keyed->Add(aKey, aSample, ProcessID::Parent);
}

void
internal_AccumulateChild(const StaticMutexAutoLock& aLock,
                         ProcessID aProcessType,
                         HistogramID aId,
                         uint32_t aSample)
{
  if (!internal_CanRecordBase()) {
    return;
  }

  if (Histogram* h = internal_GetHistogramById(aLock, aId, aProcessType)) {
    internal_HistogramAdd(aLock, *h, aId, aSample, aProcessType);
  } else {
    NS_WARNING("Failed GetHistogramById for CHILD");
  }
}

void
internal_AccumulateChildKeyed(const StaticMutexAutoLock& aLock, ProcessID aProcessType,
                              HistogramID aId, const nsCString& aKey, uint32_t aSample)
{
  if (!gInitDone || !internal_CanRecordBase()) {
    return;
  }

  KeyedHistogram* keyed = internal_GetKeyedHistogramById(aId, aProcessType);
  MOZ_ASSERT(keyed);
  keyed->Add(aKey, aSample, aProcessType);
}

void
internal_ClearHistogram(const StaticMutexAutoLock& aLock, HistogramID id)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  if (!XRE_IsParentProcess()) {
    return;
  }

  // Handle keyed histograms.
  if (gHistogramInfos[id].keyed) {
    for (uint32_t process = 0; process < static_cast<uint32_t>(ProcessID::Count); ++process) {
      KeyedHistogram* kh = internal_GetKeyedHistogramById(id, static_cast<ProcessID>(process), /* instantiate = */ false);
      if (kh) {
        kh->Clear();
      }
    }
  }

  // Now reset the histograms instances for all processes.
  for (uint32_t process = 0; process < static_cast<uint32_t>(ProcessID::Count); ++process) {
    internal_ClearHistogramById(aLock, id, static_cast<ProcessID>(process));
  }
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

void internal_JSHistogram_finalize(JSFreeOp*, JSObject*);

static const JSClassOps sJSHistogramClassOps = {
  nullptr, /* addProperty */
  nullptr, /* delProperty */
  nullptr, /* enumerate */
  nullptr, /* newEnumerate */
  nullptr, /* resolve */
  nullptr, /* mayResolve */
  internal_JSHistogram_finalize
};

static const JSClass sJSHistogramClass = {
  "JSHistogram",  /* name */
  JSCLASS_HAS_PRIVATE | JSCLASS_FOREGROUND_FINALIZE,  /* flags */
  &sJSHistogramClassOps
};

struct JSHistogramData {
  HistogramID histogramId;
};

bool
internal_JSHistogram_CoerceValue(JSContext* aCx, JS::Handle<JS::Value> aElement, HistogramID aId,
                              uint32_t aHistogramType, uint32_t& aValue)
{
  if (aElement.isString()) {
    // Strings only allowed for categorical histograms
    if (aHistogramType != nsITelemetry::HISTOGRAM_CATEGORICAL) {
      LogToBrowserConsole(nsIScriptError::errorFlag,
          NS_LITERAL_STRING("String argument only allowed for categorical histogram"));
      return false;
    }

    // Label is given by the string argument
    nsAutoJSString label;
    if (!label.init(aCx, aElement)) {
      LogToBrowserConsole(nsIScriptError::errorFlag, NS_LITERAL_STRING("Invalid string parameter"));
      return false;
    }

    // Get the label id for accumulation
    nsresult rv = gHistogramInfos[aId].label_id(NS_ConvertUTF16toUTF8(label).get(), &aValue);
    if (NS_FAILED(rv)) {
      LogToBrowserConsole(nsIScriptError::errorFlag, NS_LITERAL_STRING("Invalid string label"));
      return false;
    }
  } else if ( !(aElement.isNumber() || aElement.isBoolean()) ) {
    LogToBrowserConsole(nsIScriptError::errorFlag, NS_LITERAL_STRING("Argument not a number"));
    return false;
  } else if ( aElement.isNumber() && aElement.toNumber() > UINT32_MAX ) {
    // Clamp large numerical arguments to aValue's acceptable values.
    // JS::ToUint32 will take aElement modulo 2^32 before returning it, which
    // may result in a smaller final value.
    aValue = UINT32_MAX;
#ifdef DEBUG
    LogToBrowserConsole(nsIScriptError::errorFlag, NS_LITERAL_STRING("Clamped large numeric value"));
#endif
  } else if (!JS::ToUint32(aCx, aElement, &aValue)) {
    LogToBrowserConsole(nsIScriptError::errorFlag, NS_LITERAL_STRING("Failed to convert element to UInt32"));
    return false;
  }

  // If we're here then all type checks have passed and aValue contains the coerced integer
  return true;
}

bool
internal_JSHistogram_GetValueArray(JSContext* aCx, JS::CallArgs& args, uint32_t aHistogramType, HistogramID aId,
                                    bool isKeyed, nsTArray<uint32_t>& aArray)
{
  // This function populates aArray with the values extracted from args. Handles keyed and non-keyed histograms,
  // and single and array of values. Also performs sanity checks on the arguments.
  // Returns true upon successful population, false otherwise.

  uint32_t firstArgIndex = 0;
  if (isKeyed) {
    firstArgIndex = 1;
  }

  // Special case of no argument (or only key) and count histogram
  if (args.length() == firstArgIndex) {
    if (!(aHistogramType == nsITelemetry::HISTOGRAM_COUNT)) {
      LogToBrowserConsole(nsIScriptError::errorFlag,
          NS_LITERAL_STRING("Need at least one argument for non count type histogram"));
      return false;
    }

    aArray.AppendElement(1);
    return true;
  }

  if (args[firstArgIndex].isObject() && !args[firstArgIndex].isString()) {
    JS::Rooted<JSObject*> arrayObj(aCx, &args[firstArgIndex].toObject());

    bool isArray = false;
    JS_IsArrayObject(aCx, arrayObj, &isArray);

    if (!isArray) {
      LogToBrowserConsole(nsIScriptError::errorFlag, NS_LITERAL_STRING("The argument to accumulate can't be a non-array object"));
      return false;
    }

    uint32_t arrayLength = 0;
    if (!JS_GetArrayLength(aCx, arrayObj, &arrayLength)) {
      LogToBrowserConsole(nsIScriptError::errorFlag, NS_LITERAL_STRING("Failed while trying to get array length"));
      return false;
    }

    for (uint32_t arrayIdx = 0; arrayIdx < arrayLength; arrayIdx++) {
      JS::Rooted<JS::Value> element(aCx);

      if (!JS_GetElement(aCx, arrayObj, arrayIdx, &element)) {
        nsPrintfCString msg("Failed while trying to get element at index %d", arrayIdx);
        LogToBrowserConsole(nsIScriptError::errorFlag, NS_ConvertUTF8toUTF16(msg));
        return false;
      }

      uint32_t value = 0;
      if (!internal_JSHistogram_CoerceValue(aCx, element, aId, aHistogramType, value)) {
        nsPrintfCString msg("Element at index %d failed type checks", arrayIdx);
        LogToBrowserConsole(nsIScriptError::errorFlag, NS_ConvertUTF8toUTF16(msg));
        return false;
      }
      aArray.AppendElement(value);
    }

    return true;
  }

  uint32_t value = 0;
  if (!internal_JSHistogram_CoerceValue(aCx, args[firstArgIndex], aId, aHistogramType, value)) {
    return false;
  }
  aArray.AppendElement(value);
  return true;
}

bool
internal_JSHistogram_Add(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = CallArgsFromVp(argc, vp);

  if (!args.thisv().isObject() ||
      JS_GetClass(&args.thisv().toObject()) != &sJSHistogramClass) {
    JS_ReportErrorASCII(cx, "Wrong JS class, expected JSHistogram class");
    return false;
  }

  JSObject* obj = &args.thisv().toObject();
  JSHistogramData* data = static_cast<JSHistogramData*>(JS_GetPrivate(obj));
  MOZ_ASSERT(data);
  HistogramID id = data->histogramId;
  MOZ_ASSERT(internal_IsHistogramEnumId(id));
  uint32_t type = gHistogramInfos[id].histogramType;


  // This function should always return |undefined| and never fail but
  // rather report failures using the console.
  args.rval().setUndefined();

  nsTArray<uint32_t> values;
  if (!internal_JSHistogram_GetValueArray(cx, args, type, id, false, values)) {
    // Either GetValueArray or CoerceValue utility function will have printed a meaningful
    // error message, so we simply return true
    return true;
  }

  {
    StaticMutexAutoLock locker(gTelemetryHistogramMutex);
    for (uint32_t aValue: values) {
      internal_Accumulate(locker, id, aValue);
    }
  }
  return true;
}

bool
internal_JSHistogram_Snapshot(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

  if (!args.thisv().isObject() ||
      JS_GetClass(&args.thisv().toObject()) != &sJSHistogramClass) {
    JS_ReportErrorASCII(cx, "Wrong JS class, expected JSHistogram class");
    return false;
  }

  JSObject* obj = &args.thisv().toObject();
  JSHistogramData* data = static_cast<JSHistogramData*>(JS_GetPrivate(obj));
  MOZ_ASSERT(data);
  HistogramID id = data->histogramId;

  HistogramSnapshotData dataSnapshot;
  {
    StaticMutexAutoLock locker(gTelemetryHistogramMutex);
    MOZ_ASSERT(internal_IsHistogramEnumId(id));

    // This is not good standard behavior given that we have histogram instances
    // covering multiple processes.
    // However, changing this requires some broader changes to callers.
    Histogram* h = internal_GetHistogramById(locker, id, ProcessID::Parent);
    // Take a snapshot of the data here, protected by the lock, and then,
    // outside of the lock protection, mirror it to a JS structure
    if (NS_FAILED(internal_GetHistogramAndSamples(locker, h, dataSnapshot))) {
      return false;
    }
  }

  JS::Rooted<JSObject*> snapshot(cx, JS_NewPlainObject(cx));
  if (!snapshot) {
    return false;
  }

  if (NS_FAILED(internal_ReflectHistogramAndSamples(cx,
                                                    snapshot,
                                                    gHistogramInfos[id],
                                                    dataSnapshot))) {
    return false;
  }

  args.rval().setObject(*snapshot);
  return true;
}

bool
internal_JSHistogram_Clear(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

  if (!args.thisv().isObject() ||
      JS_GetClass(&args.thisv().toObject()) != &sJSHistogramClass) {
    JS_ReportErrorASCII(cx, "Wrong JS class, expected JSHistogram class");
    return false;
  }

  JSObject* obj = &args.thisv().toObject();
  JSHistogramData* data = static_cast<JSHistogramData*>(JS_GetPrivate(obj));
  MOZ_ASSERT(data);

  // This function should always return |undefined| and never fail but
  // rather report failures using the console.
  args.rval().setUndefined();

  HistogramID id = data->histogramId;
  {
    StaticMutexAutoLock locker(gTelemetryHistogramMutex);

    MOZ_ASSERT(internal_IsHistogramEnumId(id));
    internal_ClearHistogram(locker, id);
  }

  return true;
}

// NOTE: Runs without protection from |gTelemetryHistogramMutex|.
// See comment at the top of this section.
nsresult
internal_WrapAndReturnHistogram(HistogramID id, JSContext *cx,
                                JS::MutableHandle<JS::Value> ret)
{
  JS::Rooted<JSObject*> obj(cx, JS_NewObject(cx, &sJSHistogramClass));
  if (!obj) {
    return NS_ERROR_FAILURE;
  }

  // The 3 functions that are wrapped up here are eventually called
  // by the same thread that runs this function.
  if (!(JS_DefineFunction(cx, obj, "add", internal_JSHistogram_Add, 1, 0)
        && JS_DefineFunction(cx, obj, "snapshot",
                             internal_JSHistogram_Snapshot, 0, 0)
        && JS_DefineFunction(cx, obj, "clear", internal_JSHistogram_Clear, 0, 0))) {
    return NS_ERROR_FAILURE;
  }

  JSHistogramData* data = new JSHistogramData{id};
  JS_SetPrivate(obj, data);
  ret.setObject(*obj);

  return NS_OK;
}

void
internal_JSHistogram_finalize(JSFreeOp*, JSObject* obj)
{
  if (!obj ||
      JS_GetClass(obj) != &sJSHistogramClass) {
    MOZ_ASSERT_UNREACHABLE("Should have the right JS class.");
    return;
  }

  JSHistogramData* data = static_cast<JSHistogramData*>(JS_GetPrivate(obj));
  MOZ_ASSERT(data);
  delete data;
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
//   internal_JSKeyedHistogram_Clear
//   internal_WrapAndReturnKeyedHistogram
//
// Same comments as above, at the JSHistogram_* section, regarding
// deadlock avoidance, apply.

namespace {

void internal_JSKeyedHistogram_finalize(JSFreeOp*, JSObject*);

static const JSClassOps sJSKeyedHistogramClassOps = {
  nullptr, /* addProperty */
  nullptr, /* delProperty */
  nullptr, /* enumerate */
  nullptr, /* newEnumerate */
  nullptr, /* resolve */
  nullptr, /* mayResolve */
  internal_JSKeyedHistogram_finalize
};

static const JSClass sJSKeyedHistogramClass = {
  "JSKeyedHistogram",  /* name */
  JSCLASS_HAS_PRIVATE | JSCLASS_FOREGROUND_FINALIZE,  /* flags */
  &sJSKeyedHistogramClassOps
};

bool
internal_KeyedHistogram_SnapshotImpl(JSContext *cx, unsigned argc,
                                     JS::Value *vp,
                                     bool clearSubsession)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

  if (!args.thisv().isObject() ||
      JS_GetClass(&args.thisv().toObject()) != &sJSKeyedHistogramClass) {
    JS_ReportErrorASCII(cx, "Wrong JS class, expected JSKeyedHistogram class");
    return false;
  }

  JSObject *obj = &args.thisv().toObject();
  JSHistogramData* data = static_cast<JSHistogramData*>(JS_GetPrivate(obj));
  MOZ_ASSERT(data);
  HistogramID id = data->histogramId;
  MOZ_ASSERT(internal_IsHistogramEnumId(id));

  // This function should always return |undefined| and never fail but
  // rather report failures using the console.
  args.rval().setUndefined();

  // This is not good standard behavior given that we have histogram instances
  // covering multiple processes.
  // However, changing this requires some broader changes to callers.
  KeyedHistogram* keyed = internal_GetKeyedHistogramById(id, ProcessID::Parent, /* instantiate = */ true);
  if (!keyed) {
    JS_ReportErrorASCII(cx, "Failed to look up keyed histogram");
    return false;
  }

  // No argument was passed, so snapshot all the keys.
  if (args.length() == 0) {
    JS::RootedObject snapshot(cx, JS_NewPlainObject(cx));
    if (!snapshot) {
      JS_ReportErrorASCII(cx, "Failed to create object");
      return false;
    }

    if (!NS_SUCCEEDED(keyed->GetJSSnapshot(cx, snapshot, clearSubsession))) {
      JS_ReportErrorASCII(cx, "Failed to reflect keyed histograms");
      return false;
    }

    args.rval().setObject(*snapshot);
    return true;
  }

  // One argument was passed. If it's a string, use it as a key
  // and just snapshot the data for that key.
  nsAutoJSString key;
  if (!args[0].isString() || !key.init(cx, args[0])) {
    JS_ReportErrorASCII(cx, "Not a string");
    return false;
  }

  HistogramSnapshotData dataSnapshot;
  {
    StaticMutexAutoLock locker(gTelemetryHistogramMutex);

    // Get data for the key we're looking for.
    Histogram* h = nullptr;
    nsresult rv = keyed->GetHistogram(NS_ConvertUTF16toUTF8(key), &h);
    if (NS_FAILED(rv)) {
      return false;
    }

    // Take a snapshot of the data here, protected by the lock, and then,
    // outside of the lock protection, mirror it to a JS structure
    if (NS_FAILED(internal_GetHistogramAndSamples(locker, h, dataSnapshot))) {
      return false;
    }
  }

  JS::RootedObject snapshot(cx, JS_NewPlainObject(cx));
  if (!snapshot) {
    return false;
  }

  if (NS_FAILED(internal_ReflectHistogramAndSamples(cx,
                                                    snapshot,
                                                    gHistogramInfos[id],
                                                    dataSnapshot))) {
    JS_ReportErrorASCII(cx, "Failed to reflect histogram");
    return false;
  }

  args.rval().setObject(*snapshot);
  return true;
}

bool
internal_JSKeyedHistogram_Add(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

  if (!args.thisv().isObject() ||
      JS_GetClass(&args.thisv().toObject()) != &sJSKeyedHistogramClass) {
    JS_ReportErrorASCII(cx, "Wrong JS class, expected JSKeyedHistogram class");
    return false;
  }

  JSObject *obj = &args.thisv().toObject();
  JSHistogramData* data = static_cast<JSHistogramData*>(JS_GetPrivate(obj));
  MOZ_ASSERT(data);
  HistogramID id = data->histogramId;
  MOZ_ASSERT(internal_IsHistogramEnumId(id));

  // This function should always return |undefined| and never fail but
  // rather report failures using the console.
  args.rval().setUndefined();
  if (args.length() < 1) {
    LogToBrowserConsole(nsIScriptError::errorFlag, NS_LITERAL_STRING("Expected one argument"));
    return true;
  }

  nsAutoJSString key;
  if (!args[0].isString() || !key.init(cx, args[0])) {
    LogToBrowserConsole(nsIScriptError::errorFlag, NS_LITERAL_STRING("Not a string"));
    return true;
  }

  // Check if we're allowed to record in the provided key, for this histogram.
  if (!gHistogramInfos[id].allows_key(NS_ConvertUTF16toUTF8(key))) {
    nsPrintfCString msg("%s - key '%s' not allowed for this keyed histogram",
                        gHistogramInfos[id].name(),
                        NS_ConvertUTF16toUTF8(key).get());
    LogToBrowserConsole(nsIScriptError::errorFlag, NS_ConvertUTF8toUTF16(msg));
    TelemetryScalar::Add(
      mozilla::Telemetry::ScalarID::TELEMETRY_ACCUMULATE_UNKNOWN_HISTOGRAM_KEYS,
      NS_ConvertASCIItoUTF16(gHistogramInfos[id].name()), 1);
    return true;
  }

  const uint32_t type = gHistogramInfos[id].histogramType;

  nsTArray<uint32_t> values;
  if (!internal_JSHistogram_GetValueArray(cx, args, type, id, true, values)) {
    // Either GetValueArray or CoerceValue utility function will have printed a meaningful
    // error message so we simple return true
    return true;
  }

  {
    StaticMutexAutoLock locker(gTelemetryHistogramMutex);
    for (uint32_t aValue: values) {
      internal_Accumulate(locker, id, NS_ConvertUTF16toUTF8(key), aValue);
    }
  }
  return true;
}

bool
internal_JSKeyedHistogram_Keys(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

  if (!args.thisv().isObject() ||
      JS_GetClass(&args.thisv().toObject()) != &sJSKeyedHistogramClass) {
    JS_ReportErrorASCII(cx, "Wrong JS class, expected JSKeyedHistogram class");
    return false;
  }

  JSObject *obj = &args.thisv().toObject();
  JSHistogramData* data = static_cast<JSHistogramData*>(JS_GetPrivate(obj));
  MOZ_ASSERT(data);
  HistogramID id = data->histogramId;

  KeyedHistogram* keyed = nullptr;
  {
    StaticMutexAutoLock locker(gTelemetryHistogramMutex);
    MOZ_ASSERT(internal_IsHistogramEnumId(id));

    // This is not good standard behavior given that we have histogram instances
    // covering multiple processes.
    // However, changing this requires some broader changes to callers.
    keyed = internal_GetKeyedHistogramById(id, ProcessID::Parent);
  }

  MOZ_ASSERT(keyed);
  if (!keyed) {
    return false;
  }

  return NS_SUCCEEDED(keyed->GetJSKeys(cx, args));
}

bool
internal_JSKeyedHistogram_Snapshot(JSContext *cx, unsigned argc, JS::Value *vp)
{
  return internal_KeyedHistogram_SnapshotImpl(cx, argc, vp, false);
}

bool
internal_JSKeyedHistogram_Clear(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

  if (!args.thisv().isObject() ||
      JS_GetClass(&args.thisv().toObject()) != &sJSKeyedHistogramClass) {
    JS_ReportErrorASCII(cx, "Wrong JS class, expected JSKeyedHistogram class");
    return false;
  }

  JSObject *obj = &args.thisv().toObject();
  JSHistogramData* data = static_cast<JSHistogramData*>(JS_GetPrivate(obj));
  MOZ_ASSERT(data);
  HistogramID id = data->histogramId;

  // This function should always return |undefined| and never fail but
  // rather report failures using the console.
  args.rval().setUndefined();

  KeyedHistogram* keyed = nullptr;
  {
    MOZ_ASSERT(internal_IsHistogramEnumId(id));
    StaticMutexAutoLock locker(gTelemetryHistogramMutex);

    // This is not good standard behavior given that we have histogram instances
    // covering multiple processes.
    // However, changing this requires some broader changes to callers.
    keyed = internal_GetKeyedHistogramById(id, ProcessID::Parent, /* instantiate = */ false);

    if (!keyed) {
      return true;
    }

    keyed->Clear();
  }

  return true;
}

// NOTE: Runs without protection from |gTelemetryHistogramMutex|.
// See comment at the top of this section.
nsresult
internal_WrapAndReturnKeyedHistogram(HistogramID id, JSContext *cx,
                                     JS::MutableHandle<JS::Value> ret)
{
  JS::Rooted<JSObject*> obj(cx, JS_NewObject(cx, &sJSKeyedHistogramClass));
  if (!obj)
    return NS_ERROR_FAILURE;
  // The 6 functions that are wrapped up here are eventually called
  // by the same thread that runs this function.
  if (!(JS_DefineFunction(cx, obj, "add", internal_JSKeyedHistogram_Add, 2, 0)
        && JS_DefineFunction(cx, obj, "snapshot",
                             internal_JSKeyedHistogram_Snapshot, 1, 0)
        && JS_DefineFunction(cx, obj, "keys",
                             internal_JSKeyedHistogram_Keys, 0, 0)
        && JS_DefineFunction(cx, obj, "clear",
                             internal_JSKeyedHistogram_Clear, 0, 0))) {
    return NS_ERROR_FAILURE;
  }

  JSHistogramData* data = new JSHistogramData{id};
  JS_SetPrivate(obj, data);
  ret.setObject(*obj);

  return NS_OK;
}

void
internal_JSKeyedHistogram_finalize(JSFreeOp*, JSObject* obj)
{
  if (!obj ||
      JS_GetClass(obj) != &sJSKeyedHistogramClass) {
    MOZ_ASSERT_UNREACHABLE("Should have the right JS class.");
    return;
  }

  JSHistogramData* data = static_cast<JSHistogramData*>(JS_GetPrivate(obj));
  MOZ_ASSERT(data);
  delete data;
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

void TelemetryHistogram::InitializeGlobalState(bool canRecordBase,
                                               bool canRecordExtended)
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  MOZ_ASSERT(!gInitDone, "TelemetryHistogram::InitializeGlobalState "
             "may only be called once");

  gCanRecordBase = canRecordBase;
  gCanRecordExtended = canRecordExtended;

  if (XRE_IsParentProcess()) {
    gHistogramStorage =
      new Histogram*[HistogramCount * size_t(ProcessID::Count)] {};
    gKeyedHistogramStorage =
      new KeyedHistogram*[HistogramCount * size_t(ProcessID::Count)] {};
  }

  // gNameToHistogramIDMap should have been pre-sized correctly at the
  // declaration point further up in this file.

  // Populate the static histogram name->id cache.
  // Note that the histogram names come from a static table so we can wrap them
  // in a literal string to avoid allocations when it gets copied.
  for (uint32_t i = 0; i < HistogramCount; i++) {
    auto name = gHistogramInfos[i].name();

    // Make sure the name pointer is in a valid region. See bug 1428612.
    MOZ_DIAGNOSTIC_ASSERT(name >= gHistogramStringTable);
    MOZ_DIAGNOSTIC_ASSERT(
        uintptr_t(name) < (uintptr_t(gHistogramStringTable) + sizeof(gHistogramStringTable)));

    nsCString wrappedName;
    wrappedName.AssignLiteral(name, strlen(name));
    gNameToHistogramIDMap.Put(wrappedName, HistogramID(i));
  }

#ifdef DEBUG
  gNameToHistogramIDMap.MarkImmutable();
#endif

    // Some Telemetry histograms depend on the value of C++ constants and hardcode
    // their values in Histograms.json.
    // We add static asserts here for those values to match so that future changes
    // don't go unnoticed.
    static_assert((JS::gcreason::NUM_TELEMETRY_REASONS + 1) ==
                        gHistogramInfos[mozilla::Telemetry::GC_MINOR_REASON].bucketCount &&
                  (JS::gcreason::NUM_TELEMETRY_REASONS + 1) ==
                        gHistogramInfos[mozilla::Telemetry::GC_MINOR_REASON_LONG].bucketCount &&
                  (JS::gcreason::NUM_TELEMETRY_REASONS + 1) ==
                        gHistogramInfos[mozilla::Telemetry::GC_REASON_2].bucketCount,
                  "NUM_TELEMETRY_REASONS is assumed to be a fixed value in Histograms.json."
                  " If this was an intentional change, update the n_values for the "
                  "following in Histograms.json: GC_MINOR_REASON, GC_MINOR_REASON_LONG, "
                  "GC_REASON_2");

    static_assert((mozilla::StartupTimeline::MAX_EVENT_ID + 1) ==
                        gHistogramInfos[mozilla::Telemetry::STARTUP_MEASUREMENT_ERRORS].bucketCount,
                  "MAX_EVENT_ID is assumed to be a fixed value in Histograms.json.  If this"
                  " was an intentional change, update the n_values for the following in "
                  "Histograms.json: STARTUP_MEASUREMENT_ERRORS");

  gInitDone = true;
}

void TelemetryHistogram::DeInitializeGlobalState()
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  gCanRecordBase = false;
  gCanRecordExtended = false;
  gNameToHistogramIDMap.Clear();
  gInitDone = false;

  // FactoryGet `new`s Histograms for us, but requires us to manually delete.
  if (XRE_IsParentProcess()) {
    for (size_t i = 0; i < HistogramCount * size_t(ProcessID::Count); ++i) {
      if (i < HistogramCount * size_t(ProcessID::Count)) {
        delete gKeyedHistogramStorage[i];
      }
      if (gHistogramStorage[i] != gExpiredHistogram) {
        delete gHistogramStorage[i];
      }
    }
    delete[] gHistogramStorage;
    delete[] gKeyedHistogramStorage;
  }
  delete gExpiredHistogram;
  gExpiredHistogram = nullptr;
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
  auto processType = XRE_GetProcessType();
  for (size_t i = 0; i < HistogramCount; ++i) {
    const HistogramInfo& h = gHistogramInfos[i];
    mozilla::Telemetry::HistogramID id = mozilla::Telemetry::HistogramID(i);
    bool canRecordInProcess = CanRecordInProcess(h.record_in_processes, processType);
    bool canRecordProduct = CanRecordProduct(h.products);
    internal_SetHistogramRecordingEnabled(locker, id, canRecordInProcess && canRecordProduct);
  }

  for (auto recordingInitiallyDisabledID : kRecordingInitiallyDisabledIDs) {
    internal_SetHistogramRecordingEnabled(locker,
                                          recordingInitiallyDisabledID,
                                          false);
  }
}

void
TelemetryHistogram::SetHistogramRecordingEnabled(HistogramID aID,
                                                 bool aEnabled)
{
  if (NS_WARN_IF(!internal_IsHistogramEnumId(aID))) {
    MOZ_ASSERT_UNREACHABLE("Histogram usage requires valid ids.");
    return;
  }

  const HistogramInfo& h = gHistogramInfos[aID];
  if (!CanRecordInProcess(h.record_in_processes, XRE_GetProcessType())) {
    // Don't permit record_in_process-disabled recording to be re-enabled.
    return;
  }

  if (!CanRecordProduct(h.products)) {
    // Don't permit products-disabled recording to be re-enabled.
    return;
  }

  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  internal_SetHistogramRecordingEnabled(locker, aID, aEnabled);
}


nsresult
TelemetryHistogram::SetHistogramRecordingEnabled(const nsACString& name,
                                                 bool aEnabled)
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  HistogramID id;
  if (NS_FAILED(internal_GetHistogramIdByName(locker, name, &id))) {
    return NS_ERROR_FAILURE;
  }

  const HistogramInfo& hi = gHistogramInfos[id];
  if (CanRecordInProcess(hi.record_in_processes, XRE_GetProcessType())) {
    internal_SetHistogramRecordingEnabled(locker, id, aEnabled);
  }
  return NS_OK;
}


void
TelemetryHistogram::Accumulate(HistogramID aID,
                               uint32_t aSample)
{
  if (NS_WARN_IF(!internal_IsHistogramEnumId(aID))) {
    MOZ_ASSERT_UNREACHABLE("Histogram usage requires valid ids.");
    return;
  }

  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  internal_Accumulate(locker, aID, aSample);
}

void
TelemetryHistogram::Accumulate(HistogramID aID, const nsTArray<uint32_t>& aSamples)
{
  if (NS_WARN_IF(!internal_IsHistogramEnumId(aID))) {
    MOZ_ASSERT_UNREACHABLE("Histogram usage requires valid ids.");
    return;
  }

  MOZ_ASSERT(!gHistogramInfos[aID].keyed, "Cannot accumulate into a keyed histogram. No key given.");

  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  for(uint32_t sample: aSamples){
    internal_Accumulate(locker, aID, sample);
  }
}

void
TelemetryHistogram::Accumulate(HistogramID aID,
                               const nsCString& aKey, uint32_t aSample)
{
  if (NS_WARN_IF(!internal_IsHistogramEnumId(aID))) {
    MOZ_ASSERT_UNREACHABLE("Histogram usage requires valid ids.");
    return;
  }

  // Check if we're allowed to record in the provided key, for this histogram.
  if (!gHistogramInfos[aID].allows_key(aKey)) {
    nsPrintfCString msg("%s - key '%s' not allowed for this keyed histogram",
                        gHistogramInfos[aID].name(),
                        aKey.get());
    LogToBrowserConsole(nsIScriptError::errorFlag, NS_ConvertUTF8toUTF16(msg));
    TelemetryScalar::Add(
      mozilla::Telemetry::ScalarID::TELEMETRY_ACCUMULATE_UNKNOWN_HISTOGRAM_KEYS,
      NS_ConvertASCIItoUTF16(gHistogramInfos[aID].name()), 1);
    return;
  }

  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  internal_Accumulate(locker, aID, aKey, aSample);
}

void
TelemetryHistogram::Accumulate(HistogramID aID, const nsCString& aKey,
                              const nsTArray<uint32_t>& aSamples)
{
  if (NS_WARN_IF(!internal_IsHistogramEnumId(aID))) {
    MOZ_ASSERT_UNREACHABLE("Histogram usage requires valid ids");
    return;
  }

  // Check that this histogram is keyed
  MOZ_ASSERT(gHistogramInfos[aID].keyed, "Cannot accumulate into a non-keyed histogram using a key.");

  // Check if we're allowed to record in the provided key, for this histogram.
  if (!gHistogramInfos[aID].allows_key(aKey)) {
    nsPrintfCString msg("%s - key '%s' not allowed for this keyed histogram",
                        gHistogramInfos[aID].name(),
                        aKey.get());
    LogToBrowserConsole(nsIScriptError::errorFlag, NS_ConvertUTF8toUTF16(msg));
    TelemetryScalar::Add(
      mozilla::Telemetry::ScalarID::TELEMETRY_ACCUMULATE_UNKNOWN_HISTOGRAM_KEYS,
      NS_ConvertASCIItoUTF16(gHistogramInfos[aID].name()), 1);
    return;
  }

  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  for(uint32_t sample: aSamples){
    internal_Accumulate(locker, aID, aKey, sample);
  }
}

void
TelemetryHistogram::Accumulate(const char* name, uint32_t sample)
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  if (!internal_CanRecordBase()) {
    return;
  }
  HistogramID id;
  nsresult rv = internal_GetHistogramIdByName(locker, nsDependentCString(name), &id);
  if (NS_FAILED(rv)) {
    return;
  }
  internal_Accumulate(locker, id, sample);
}

void
TelemetryHistogram::Accumulate(const char* name,
                               const nsCString& key, uint32_t sample)
{
  bool keyNotAllowed = false;

  {
    StaticMutexAutoLock locker(gTelemetryHistogramMutex);
    if (!internal_CanRecordBase()) {
      return;
    }
    HistogramID id;
    nsresult rv = internal_GetHistogramIdByName(locker, nsDependentCString(name), &id);
    if (NS_SUCCEEDED(rv)) {
      // Check if we're allowed to record in the provided key, for this histogram.
      if (gHistogramInfos[id].allows_key(key)) {
        internal_Accumulate(locker, id, key, sample);
        return;
      }
      // We're holding |gTelemetryHistogramMutex|, so we can't print a message
      // here.
      keyNotAllowed = true;
    }
   }

  if (keyNotAllowed) {
    LogToBrowserConsole(nsIScriptError::errorFlag,
                        NS_LITERAL_STRING("Key not allowed for this keyed histogram"));
    TelemetryScalar::Add(
      mozilla::Telemetry::ScalarID::TELEMETRY_ACCUMULATE_UNKNOWN_HISTOGRAM_KEYS,
      NS_ConvertASCIItoUTF16(name), 1);
  }
}

void
TelemetryHistogram::AccumulateCategorical(HistogramID aId,
                                          const nsCString& label)
{
  if (NS_WARN_IF(!internal_IsHistogramEnumId(aId))) {
    MOZ_ASSERT_UNREACHABLE("Histogram usage requires valid ids.");
    return;
  }

  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  if (!internal_CanRecordBase()) {
    return;
  }
  uint32_t labelId = 0;
  if (NS_FAILED(gHistogramInfos[aId].label_id(label.get(), &labelId))) {
    return;
  }
  internal_Accumulate(locker, aId, labelId);
}

void
TelemetryHistogram::AccumulateCategorical(HistogramID aId, const nsTArray<nsCString>& aLabels)
{
  if (NS_WARN_IF(!internal_IsHistogramEnumId(aId))) {
    MOZ_ASSERT_UNREACHABLE("Histogram usage requires valid ids.");
    return;
  }

  if (!internal_CanRecordBase()) {
    return;
  }

  // We use two loops, one for getting label_ids and another one for actually accumulating
  // the values. This ensures that in the case of an invalid label in the array, no values
  // are accumulated. In any call to this API, either all or (in case of error) none of the
  // values will be accumulated.

  nsTArray<uint32_t> intSamples(aLabels.Length());
  for (const nsCString& label: aLabels){
    uint32_t labelId = 0;
    if (NS_FAILED(gHistogramInfos[aId].label_id(label.get(), &labelId))) {
      return;
    }
    intSamples.AppendElement(labelId);
  }

  StaticMutexAutoLock locker(gTelemetryHistogramMutex);

  for (uint32_t sample: intSamples){
    internal_Accumulate(locker, aId, sample);
  }
}

void
TelemetryHistogram::AccumulateChild(ProcessID aProcessType,
                                    const nsTArray<HistogramAccumulation>& aAccumulations)
{
  MOZ_ASSERT(XRE_IsParentProcess());

  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  if (!internal_CanRecordBase()) {
    return;
  }
  for (uint32_t i = 0; i < aAccumulations.Length(); ++i) {
    if (NS_WARN_IF(!internal_IsHistogramEnumId(aAccumulations[i].mId))) {
      MOZ_ASSERT_UNREACHABLE("Histogram usage requires valid ids.");
      continue;
    }
    internal_AccumulateChild(locker,
                             aProcessType,
                             aAccumulations[i].mId,
                             aAccumulations[i].mSample);
  }
}

void
TelemetryHistogram::AccumulateChildKeyed(ProcessID aProcessType,
                                         const nsTArray<KeyedHistogramAccumulation>& aAccumulations)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  if (!internal_CanRecordBase()) {
    return;
  }
  for (uint32_t i = 0; i < aAccumulations.Length(); ++i) {
    if (NS_WARN_IF(!internal_IsHistogramEnumId(aAccumulations[i].mId))) {
      MOZ_ASSERT_UNREACHABLE("Histogram usage requires valid ids.");
      continue;
    }
    internal_AccumulateChildKeyed(locker,
                                  aProcessType,
                                  aAccumulations[i].mId,
                                  aAccumulations[i].mKey,
                                  aAccumulations[i].mSample);
  }
}

nsresult
TelemetryHistogram::GetHistogramById(const nsACString &name, JSContext *cx,
                                     JS::MutableHandle<JS::Value> ret)
{
  HistogramID id;
  {
    StaticMutexAutoLock locker(gTelemetryHistogramMutex);
    nsresult rv = internal_GetHistogramIdByName(locker, name, &id);
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }

    if (gHistogramInfos[id].keyed) {
      return NS_ERROR_FAILURE;
    }
  }
  // Runs without protection from |gTelemetryHistogramMutex|
  return internal_WrapAndReturnHistogram(id, cx, ret);
}

nsresult
TelemetryHistogram::GetKeyedHistogramById(const nsACString &name,
                                          JSContext *cx,
                                          JS::MutableHandle<JS::Value> ret)
{
  HistogramID id;
  {
    StaticMutexAutoLock locker(gTelemetryHistogramMutex);
    nsresult rv = internal_GetHistogramIdByName(locker, name, &id);
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }

    if (!gHistogramInfos[id].keyed) {
      return NS_ERROR_FAILURE;
    }
  }
  // Runs without protection from |gTelemetryHistogramMutex|
  return internal_WrapAndReturnKeyedHistogram(id, cx, ret);
}

const char*
TelemetryHistogram::GetHistogramName(HistogramID id)
{
  if (NS_WARN_IF(!internal_IsHistogramEnumId(id))) {
    MOZ_ASSERT_UNREACHABLE("Histogram usage requires valid ids.");
    return nullptr;
  }

  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  const HistogramInfo& h = gHistogramInfos[id];
  return h.name();
}

nsresult
TelemetryHistogram::CreateHistogramSnapshots(JSContext* aCx,
                                             JS::MutableHandleValue aResult,
                                             unsigned int aDataset,
                                             bool aClearSubsession)
{
  // Runs without protection from |gTelemetryHistogramMutex|
  JS::Rooted<JSObject*> root_obj(aCx, JS_NewPlainObject(aCx));
  if (!root_obj) {
    return NS_ERROR_FAILURE;
  }
  aResult.setObject(*root_obj);

  // Include the GPU process in histogram snapshots only if we actually tried
  // to launch a process for it.
  bool includeGPUProcess = internal_AttemptedGPUProcess();

  HistogramProcessSnapshotsArray processHistArray;
  {
    StaticMutexAutoLock locker(gTelemetryHistogramMutex);
    nsresult rv = internal_GetHistogramsSnapshot(locker,
                                                 aDataset,
                                                 aClearSubsession,
                                                 includeGPUProcess,
                                                 processHistArray);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // Make the JS calls on the stashed histograms for every process
  for (uint32_t process = 0; process < processHistArray.length(); ++process) {
    JS::Rooted<JSObject*> processObject(aCx, JS_NewPlainObject(aCx));
    if (!processObject) {
      return NS_ERROR_FAILURE;
    }
    if (!JS_DefineProperty(aCx, root_obj,
                           GetNameForProcessID(ProcessID(process)),
                           processObject, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }

    for (const HistogramSnapshotInfo& hData : processHistArray[process]) {
      HistogramID id = hData.histogramID;

      JS::Rooted<JSObject*> hobj(aCx, JS_NewPlainObject(aCx));
      if (!hobj) {
        return NS_ERROR_FAILURE;
      }

      if (NS_FAILED(internal_ReflectHistogramAndSamples(aCx,
                                                        hobj,
                                                        gHistogramInfos[id],
                                                        hData.data))) {
        return NS_ERROR_FAILURE;
      }

      if (!JS_DefineProperty(aCx, processObject, gHistogramInfos[id].name(),
                             hobj, JSPROP_ENUMERATE)) {
          return NS_ERROR_FAILURE;
      }
    }
  }
  return NS_OK;
}

nsresult
TelemetryHistogram::GetKeyedHistogramSnapshots(JSContext* aCx,
                                               JS::MutableHandleValue aResult,
                                               unsigned int aDataset,
                                               bool aClearSubsession)
{
  // Runs without protection from |gTelemetryHistogramMutex|
  JS::Rooted<JSObject*> obj(aCx, JS_NewPlainObject(aCx));
  if (!obj) {
    return NS_ERROR_FAILURE;
  }
  aResult.setObject(*obj);

  // Include the GPU process in histogram snapshots only if we actually tried
  // to launch a process for it.
  bool includeGPUProcess = internal_AttemptedGPUProcess();

  // Get a snapshot of all the data while holding the mutex.
  KeyedHistogramProcessSnapshotsArray processHistArray;
  {
    StaticMutexAutoLock locker(gTelemetryHistogramMutex);
    nsresult rv = internal_GetKeyedHistogramsSnapshot(locker,
                                                      aDataset,
                                                      aClearSubsession,
                                                      includeGPUProcess,
                                                      processHistArray);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // Mirror the snapshot data to JS, now that we released the mutex.
  for (uint32_t process = 0; process < processHistArray.length(); ++process) {
    JS::Rooted<JSObject*> processObject(aCx, JS_NewPlainObject(aCx));
    if (!processObject) {
      return NS_ERROR_FAILURE;
    }
    if (!JS_DefineProperty(aCx, obj, GetNameForProcessID(ProcessID(process)),
                           processObject, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }

    for (const KeyedHistogramSnapshotInfo& hData : processHistArray[process]) {
      const HistogramInfo& info = gHistogramInfos[hData.histogramId];

      JS::RootedObject snapshot(aCx, JS_NewPlainObject(aCx));
      if (!snapshot) {
        return NS_ERROR_FAILURE;
      }

      if (!NS_SUCCEEDED(internal_ReflectKeyedHistogram(hData.data, info, aCx, snapshot))) {
        return NS_ERROR_FAILURE;
      }

      if (!JS_DefineProperty(aCx, processObject, info.name(),
                             snapshot, JSPROP_ENUMERATE)) {
        return NS_ERROR_FAILURE;
      }
    }
  }
  return NS_OK;
}

size_t
TelemetryHistogram::GetMapShallowSizesOfExcludingThis(mozilla::MallocSizeOf
                                                      aMallocSizeOf)
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  return gNameToHistogramIDMap.ShallowSizeOfExcludingThis(aMallocSizeOf);
}

size_t
TelemetryHistogram::GetHistogramSizesofIncludingThis(mozilla::MallocSizeOf
                                                     aMallocSizeOf)
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  // TODO
  return 0;
}

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE: GeckoView specific helpers

namespace base {
class PersistedSampleSet : public Histogram::SampleSet
{
public:
  explicit PersistedSampleSet(const nsTArray<Histogram::Count>& aCounts,
                              int64_t aSampleSum);
};

PersistedSampleSet::PersistedSampleSet(const nsTArray<Histogram::Count>& aCounts,
                                       int64_t aSampleSum)
{
  // Initialize the data in the base class. See Histogram::SampleSet
  // for the fields documentation.
  const size_t numCounts = aCounts.Length();
  counts_.SetLength(numCounts);

  for (size_t i = 0; i < numCounts; i++) {
    counts_[i] = aCounts[i];
    redundant_count_ += aCounts[i];
  }
  sum_ = aSampleSum;
};
} // base (from ipc/chromium/src/base)

namespace {
/**
 * Helper function to write histogram properties to JSON.
 * Please note that this needs to be called between
 * StartObjectProperty/EndObject calls that mark the histogram's
 * JSON creation.
 */
void
internal_ReflectHistogramToJSON(const HistogramSnapshotData& aSnapshot,
                                mozilla::JSONWriter& aWriter)
{
  aWriter.IntProperty("sum", aSnapshot.mSampleSum);

  // Fill the "counts" property.
  aWriter.StartArrayProperty("counts");
  for (size_t i = 0; i < aSnapshot.mBucketCounts.Length(); i++) {
    aWriter.IntElement(aSnapshot.mBucketCounts[i]);
  }
  aWriter.EndArray();
}

bool
internal_CanRecordHistogram(const HistogramID id,
                            ProcessID aProcessType)
{
  // Check if we are allowed to record the data.
  if (!CanRecordDataset(gHistogramInfos[id].dataset,
                        internal_CanRecordBase(),
                        internal_CanRecordExtended())) {
    return false;
  }

  // Check if we're allowed to record in the given process.
  if (aProcessType == ProcessID::Parent && !internal_IsRecordingEnabled(id)) {
    return false;
  }

  if (aProcessType != ProcessID::Parent
      && !CanRecordInProcess(gHistogramInfos[id].record_in_processes, aProcessType)) {
    return false;
  }

  // Don't record if the current platform is not enabled
  if (!CanRecordProduct(gHistogramInfos[id].products)) {
    return false;
  }

  return true;
}

nsresult
internal_ParseHistogramData(JSContext* aCx, JS::HandleId aEntryId,
                            JS::HandleObject aContainerObj, nsACString& aOutName,
                            nsTArray<Histogram::Count>& aOutCountArray, int64_t& aOutSum)
{
  // Get the histogram name.
  nsAutoJSString histogramName;
  if (!histogramName.init(aCx, aEntryId)) {
    JS_ClearPendingException(aCx);
    return NS_ERROR_FAILURE;
  }

  aOutName = NS_ConvertUTF16toUTF8(histogramName);

  // Get the data for this histogram.
  JS::RootedValue histogramData(aCx);
  if (!JS_GetPropertyById(aCx, aContainerObj, aEntryId, &histogramData)) {
    JS_ClearPendingException(aCx);
    return NS_ERROR_FAILURE;
  }

  if (!histogramData.isObject()) {
    // Histogram data need to be an object. If that's not the case, skip it
    // and try to load the rest of the data.
    return NS_ERROR_FAILURE;
  }

  // Get the "sum" property.
  JS::RootedValue sumValue(aCx);
  JS::RootedObject histogramObj(aCx, &histogramData.toObject());
  if (!JS_GetProperty(aCx, histogramObj, "sum", &sumValue)) {
    JS_ClearPendingException(aCx);
    return NS_ERROR_FAILURE;
  }

  if (!JS::ToInt64(aCx, sumValue, &aOutSum)) {
    JS_ClearPendingException(aCx);
    return NS_ERROR_FAILURE;
  }

  // Get the "counts" array.
  JS::RootedValue countsArray(aCx);
  bool countsIsArray = false;
  if (!JS_GetProperty(aCx, histogramObj, "counts", &countsArray)
      || !JS_IsArrayObject(aCx, countsArray, &countsIsArray)) {
    JS_ClearPendingException(aCx);
    return NS_ERROR_FAILURE;
  }

  if (!countsIsArray) {
    // The "counts" property needs to be an array. If this is not the case,
    // skip this histogram.
    return NS_ERROR_FAILURE;
  }

  // Get the length of the array.
  uint32_t countsLen = 0;
  JS::RootedObject countsArrayObj(aCx, &countsArray.toObject());
  if (!JS_GetArrayLength(aCx, countsArrayObj, &countsLen)) {
    JS_ClearPendingException(aCx);
    return NS_ERROR_FAILURE;
  }

  // Parse the "counts" in the array.
  for (uint32_t arrayIdx = 0; arrayIdx < countsLen; arrayIdx++) {
    JS::RootedValue elementValue(aCx);
    int countAsInt = 0;
    if (!JS_GetElement(aCx, countsArrayObj, arrayIdx, &elementValue)
        || !JS::ToInt32(aCx, elementValue, &countAsInt)) {
      JS_ClearPendingException(aCx);
      return NS_ERROR_FAILURE;
    }
    aOutCountArray.AppendElement(countAsInt);
  }

  return NS_OK;
}

} // Anonymous namespace

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PUBLIC: GeckoView serialization/deserialization functions.

nsresult
TelemetryHistogram::SerializeHistograms(mozilla::JSONWriter& aWriter)
{
  MOZ_ASSERT(XRE_IsParentProcess(), "Only save histograms in the parent process");
  if (!XRE_IsParentProcess()) {
    return NS_ERROR_FAILURE;
  }

  // Include the GPU process in histogram snapshots only if we actually tried
  // to launch a process for it.
  bool includeGPUProcess = internal_AttemptedGPUProcess();

  // Take a snapshot of the histograms.
  HistogramProcessSnapshotsArray processHistArray;
  {
    StaticMutexAutoLock locker(gTelemetryHistogramMutex);
    // We always request the "opt-in"/"prerelease" dataset: we internally
    // record the right subset, so this will only return "prerelease" if
    // it was recorded.
    if (NS_FAILED(internal_GetHistogramsSnapshot(locker,
                                                 nsITelemetry::DATASET_RELEASE_CHANNEL_OPTIN,
                                                 false /* aClearSubsession */,
                                                 includeGPUProcess,
                                                 processHistArray))) {
      return NS_ERROR_FAILURE;
    }
  }



  // Make the JSON calls on the stashed histograms for every process
  for (uint32_t process = 0; process < processHistArray.length(); ++process) {
    aWriter.StartObjectProperty(GetNameForProcessID(ProcessID(process)));

    for (const HistogramSnapshotInfo& hData : processHistArray[process]) {
      HistogramID id = hData.histogramID;

      aWriter.StartObjectProperty(gHistogramInfos[id].name());
      internal_ReflectHistogramToJSON(hData.data, aWriter);
      aWriter.EndObject();
    }
    aWriter.EndObject();
  }

  return NS_OK;
}

nsresult
TelemetryHistogram::SerializeKeyedHistograms(mozilla::JSONWriter& aWriter)
{
  MOZ_ASSERT(XRE_IsParentProcess(), "Only save keyed histograms in the parent process");
  if (!XRE_IsParentProcess()) {
    return NS_ERROR_FAILURE;
  }

  // Include the GPU process in histogram snapshots only if we actually tried
  // to launch a process for it.
  bool includeGPUProcess = internal_AttemptedGPUProcess();

  // Take a snapshot of the keyed histograms.
  KeyedHistogramProcessSnapshotsArray processHistArray;
  {
    StaticMutexAutoLock locker(gTelemetryHistogramMutex);
    // We always request the "opt-in"/"prerelease" dataset: we internally
    // record the right subset, so this will only return "prerelease" if
    // it was recorded.
    if (NS_FAILED(internal_GetKeyedHistogramsSnapshot(locker,
                                                      nsITelemetry::DATASET_RELEASE_CHANNEL_OPTIN,
                                                      false /* aClearSubsession */,
                                                      includeGPUProcess,
                                                      processHistArray,
                                                      true /* aSkipEmpty */))) {
      return NS_ERROR_FAILURE;
    }
  }

  // Serialize the keyed histograms for every process.
  for (uint32_t process = 0; process < processHistArray.length(); ++process) {
    aWriter.StartObjectProperty(GetNameForProcessID(ProcessID(process)));

    const KeyedHistogramSnapshotsArray& hArray = processHistArray[process];
    for (size_t i = 0; i < hArray.length(); ++i) {
      const KeyedHistogramSnapshotInfo& hData = hArray[i];
      HistogramID id = hData.histogramId;
      const HistogramInfo& info = gHistogramInfos[id];

      aWriter.StartObjectProperty(info.name());

      // Each key is a new object with a "sum" and a "counts" property.
      for (auto iter = hData.data.ConstIter(); !iter.Done(); iter.Next()) {
        HistogramSnapshotData& keyData = iter.Data();
        aWriter.StartObjectProperty(PromiseFlatCString(iter.Key()).get());
        internal_ReflectHistogramToJSON(keyData, aWriter);
        aWriter.EndObject();
      }

      aWriter.EndObject();
    }
    aWriter.EndObject();
  }

  return NS_OK;
}

nsresult
TelemetryHistogram::DeserializeHistograms(JSContext* aCx, JS::HandleValue aData)
{
  MOZ_ASSERT(XRE_IsParentProcess(), "Only load histograms in the parent process");
  if (!XRE_IsParentProcess()) {
    return NS_ERROR_FAILURE;
  }

  // Telemetry is disabled. This should never happen, but let's leave this check
  // for consistency with other histogram updates routines.
  if (!internal_CanRecordBase()) {
    return NS_OK;
  }

  typedef mozilla::Tuple<nsCString, nsTArray<Histogram::Count>, int64_t>
    PersistedHistogramTuple;
  typedef mozilla::Vector<PersistedHistogramTuple> PersistedHistogramArray;
  typedef mozilla::Vector<PersistedHistogramArray> PersistedHistogramStorage;

  // Before updating the histograms, we need to get the data out of the JS
  // wrappers. We can't hold the histogram mutex while handling JS stuff.
  // Build a <histogram name, value> map.
  JS::RootedObject histogramDataObj(aCx, &aData.toObject());
  JS::Rooted<JS::IdVector> processes(aCx, JS::IdVector(aCx));
  if (!JS_Enumerate(aCx, histogramDataObj, &processes)) {
    // We can't even enumerate the processes in the loaded data, so
    // there is nothing we could recover from the persistence file. Bail out.
    JS_ClearPendingException(aCx);
    return NS_ERROR_FAILURE;
  }

  // Make sure we have enough storage for all the processes.
  PersistedHistogramStorage histogramsToUpdate;
  if (!histogramsToUpdate.resize(static_cast<uint32_t>(ProcessID::Count))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // The following block of code attempts to extract as much data as possible
  // from the serialized JSON, even in case of light data corruptions: if, for example,
  // the data for a single process is corrupted or is in an unexpected form, we press on
  // and attempt to load the data for the other processes.
  JS::RootedId process(aCx);
  for (auto& processVal : processes) {
    // This is required as JS API calls require an Handle<jsid> and not a
    // plain jsid.
    process = processVal;
    // Get the process name.
    nsAutoJSString processNameJS;
    if (!processNameJS.init(aCx, process)) {
      JS_ClearPendingException(aCx);
      continue;
    }

    // Make sure it's valid. Note that this is safe to call outside
    // of a locked section.
    NS_ConvertUTF16toUTF8 processName(processNameJS);
    ProcessID processID = GetIDForProcessName(processName.get());
    if (processID == ProcessID::Count) {
      NS_WARNING(nsPrintfCString("Failed to get process ID for %s", processName.get()).get());
      continue;
    }

    // And its probes.
    JS::RootedValue processData(aCx);
    if (!JS_GetPropertyById(aCx, histogramDataObj, process, &processData)) {
      JS_ClearPendingException(aCx);
      continue;
    }

    if (!processData.isObject()) {
      // |processData| should be an object containing histograms. If this is
      // not the case, silently skip and try to load the data for the other
      // processes.
      continue;
    }

    // Iterate through each histogram.
    JS::RootedObject processDataObj(aCx, &processData.toObject());
    JS::Rooted<JS::IdVector> histograms(aCx, JS::IdVector(aCx));
    if (!JS_Enumerate(aCx, processDataObj, &histograms)) {
      JS_ClearPendingException(aCx);
      continue;
    }

    // Get a reference to the deserialized data for this process.
    PersistedHistogramArray& deserializedProcessData =
      histogramsToUpdate[static_cast<uint32_t>(processID)];

    JS::RootedId histogram(aCx);
    for (auto& histogramVal : histograms) {
      histogram = histogramVal;

      int64_t sum = 0;
      nsTArray<Histogram::Count> deserializedCounts;
      nsCString histogramName;
      if (NS_FAILED(internal_ParseHistogramData(aCx, histogram, processDataObj,
                                                histogramName, deserializedCounts, sum))) {
        continue;
      }

      // Finally append the deserialized data to the storage.
      if (!deserializedProcessData.emplaceBack(
        MakeTuple(mozilla::Move(histogramName), mozilla::Move(deserializedCounts), sum))) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  // Update the histogram storage.
  {
    StaticMutexAutoLock locker(gTelemetryHistogramMutex);

    for (uint32_t process = 0; process < histogramsToUpdate.length(); ++process) {
      PersistedHistogramArray& processArray = histogramsToUpdate[process];

      for (auto& histogramData : processArray) {
        // Attempt to get the corresponding ID for the deserialized histogram name.
        HistogramID id;
        if (NS_FAILED(internal_GetHistogramIdByName(locker, mozilla::Get<0>(histogramData), &id))) {
          continue;
        }

        ProcessID procID = static_cast<ProcessID>(process);
        if (!internal_CanRecordHistogram(id, procID)) {
          // We're not allowed to record this, so don't try to restore it.
          continue;
        }

        // Get the Histogram instance: this will instantiate it if it doesn't exist.
        Histogram* h = internal_GetHistogramById(locker, id, procID);
        MOZ_ASSERT(h);

        if (!h || internal_IsExpired(locker, h)) {
          // Don't restore expired histograms.
          continue;
        }

        // Make sure that histogram counts have matching sizes. If not,
        // |AddSampleSet| will fail and crash.
        size_t numCounts = mozilla::Get<1>(histogramData).Length();
        if (h->bucket_count() != numCounts) {
          MOZ_ASSERT(false,
                     "The number of restored buckets does not match with the on in the definition");
          continue;
        }

        // Update the data for the histogram.
        h->AddSampleSet(base::PersistedSampleSet(mozilla::Move(mozilla::Get<1>(histogramData)),
                                                 mozilla::Get<2>(histogramData)));
      }
    }
  }

  return NS_OK;
}

nsresult
TelemetryHistogram::DeserializeKeyedHistograms(JSContext* aCx, JS::HandleValue aData)
{
  MOZ_ASSERT(XRE_IsParentProcess(), "Only load keyed histograms in the parent process");
  if (!XRE_IsParentProcess()) {
    return NS_ERROR_FAILURE;
  }

  // Telemetry is disabled. This should never happen, but let's leave this check
  // for consistency with other histogram updates routines.
  if (!internal_CanRecordBase()) {
    return NS_OK;
  }

  typedef mozilla::Tuple<nsCString, nsCString, nsTArray<Histogram::Count>, int64_t>
    PersistedKeyedHistogramTuple;
  typedef mozilla::Vector<PersistedKeyedHistogramTuple> PersistedKeyedHistogramArray;
  typedef mozilla::Vector<PersistedKeyedHistogramArray> PersistedKeyedHistogramStorage;

  // Before updating the histograms, we need to get the data out of the JS
  // wrappers. We can't hold the histogram mutex while handling JS stuff.
  // Build a <histogram name, value> map.
  JS::RootedObject histogramDataObj(aCx, &aData.toObject());
  JS::Rooted<JS::IdVector> processes(aCx, JS::IdVector(aCx));
  if (!JS_Enumerate(aCx, histogramDataObj, &processes)) {
    // We can't even enumerate the processes in the loaded data, so
    // there is nothing we could recover from the persistence file. Bail out.
    JS_ClearPendingException(aCx);
    return NS_ERROR_FAILURE;
  }

  // Make sure we have enough storage for all the processes.
  PersistedKeyedHistogramStorage histogramsToUpdate;
  if (!histogramsToUpdate.resize(static_cast<uint32_t>(ProcessID::Count))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // The following block of code attempts to extract as much data as possible
  // from the serialized JSON, even in case of light data corruptions: if, for example,
  // the data for a single process is corrupted or is in an unexpected form, we press on
  // and attempt to load the data for the other processes.
  JS::RootedId process(aCx);
  for (auto& processVal : processes) {
    // This is required as JS API calls require an Handle<jsid> and not a
    // plain jsid.
    process = processVal;
    // Get the process name.
    nsAutoJSString processNameJS;
    if (!processNameJS.init(aCx, process)) {
      JS_ClearPendingException(aCx);
      continue;
    }

    // Make sure it's valid. Note that this is safe to call outside
    // of a locked section.
    NS_ConvertUTF16toUTF8 processName(processNameJS);
    ProcessID processID = GetIDForProcessName(processName.get());
    if (processID == ProcessID::Count) {
      NS_WARNING(nsPrintfCString("Failed to get process ID for %s", processName.get()).get());
      continue;
    }

    // And its probes.
    JS::RootedValue processData(aCx);
    if (!JS_GetPropertyById(aCx, histogramDataObj, process, &processData)) {
      JS_ClearPendingException(aCx);
      continue;
    }

    if (!processData.isObject()) {
      // |processData| should be an object containing histograms. If this is
      // not the case, silently skip and try to load the data for the other
      // processes.
      continue;
    }

    // Iterate through each keyed histogram.
    JS::RootedObject processDataObj(aCx, &processData.toObject());
    JS::Rooted<JS::IdVector> histograms(aCx, JS::IdVector(aCx));
    if (!JS_Enumerate(aCx, processDataObj, &histograms)) {
      JS_ClearPendingException(aCx);
      continue;
    }

    // Get a reference to the deserialized data for this process.
    PersistedKeyedHistogramArray& deserializedProcessData =
      histogramsToUpdate[static_cast<uint32_t>(processID)];

    JS::RootedId histogram(aCx);
    for (auto& histogramVal : histograms) {
      histogram = histogramVal;
      // Get the histogram name.
      nsAutoJSString histogramName;
      if (!histogramName.init(aCx, histogram)) {
        JS_ClearPendingException(aCx);
        continue;
      }

      // Get the data for this histogram.
      JS::RootedValue histogramData(aCx);
      if (!JS_GetPropertyById(aCx, processDataObj, histogram, &histogramData)) {
        JS_ClearPendingException(aCx);
        continue;
      }

      // Iterate through each key in the histogram.
      JS::RootedObject keysDataObj(aCx, &histogramData.toObject());
      JS::Rooted<JS::IdVector> keys(aCx, JS::IdVector(aCx));
      if (!JS_Enumerate(aCx, keysDataObj, &keys)) {
        JS_ClearPendingException(aCx);
        continue;
      }

      JS::RootedId key(aCx);
      for (auto& keyVal : keys) {
        key = keyVal;

        int64_t sum = 0;
        nsTArray<Histogram::Count> deserializedCounts;
        nsCString keyName;
        if (NS_FAILED(internal_ParseHistogramData(aCx, key, keysDataObj, keyName,
                                                  deserializedCounts, sum))) {
          continue;
        }

        // Finally append the deserialized data to the storage.
        if (!deserializedProcessData.emplaceBack(
          MakeTuple(nsCString(NS_ConvertUTF16toUTF8(histogramName)), mozilla::Move(keyName),
                    mozilla::Move(deserializedCounts), sum))) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
      }
    }
  }

  // Update the keyed histogram storage.
  {
    StaticMutexAutoLock locker(gTelemetryHistogramMutex);

    for (uint32_t process = 0; process < histogramsToUpdate.length(); ++process) {
      PersistedKeyedHistogramArray& processArray = histogramsToUpdate[process];

      for (auto& histogramData : processArray) {
        // Attempt to get the corresponding ID for the deserialized histogram name.
        HistogramID id;
        if (NS_FAILED(internal_GetHistogramIdByName(locker, mozilla::Get<0>(histogramData), &id))) {
          continue;
        }

        ProcessID procID = static_cast<ProcessID>(process);
        if (!internal_CanRecordHistogram(id, procID)) {
          // We're not allowed to record this, so don't try to restore it.
          continue;
        }

        KeyedHistogram* keyed = internal_GetKeyedHistogramById(id, procID);
        MOZ_ASSERT(keyed);

        if (!keyed) {
          // Don't restore if we don't have a destination storage.
          continue;
        }

        // Get data for the key we're looking for.
        Histogram* h = nullptr;
        if (NS_FAILED(keyed->GetHistogram(mozilla::Get<1>(histogramData), &h))) {
          continue;
        }
        MOZ_ASSERT(h);

        if (!h || internal_IsExpired(locker, h)) {
          // Don't restore expired histograms.
          continue;
        }

        // Make sure that histogram counts have matching sizes. If not,
        // |AddSampleSet| will fail and crash.
        size_t numCounts = mozilla::Get<2>(histogramData).Length();
        if (h->bucket_count() != numCounts) {
          MOZ_ASSERT(false,
                     "The number of restored buckets does not match with the on in the definition");
          continue;
        }

        // Update the data for the histogram.
        h->AddSampleSet(base::PersistedSampleSet(mozilla::Move(mozilla::Get<2>(histogramData)),
                                                 mozilla::Get<3>(histogramData)));
      }
    }
  }

  return NS_OK;
}
