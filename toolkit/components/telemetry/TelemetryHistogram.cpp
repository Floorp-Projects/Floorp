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

#include "mozilla/dom/ToJSValue.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/Atomics.h"
#include "mozilla/StartupTimeline.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/Unused.h"

#include "TelemetryCommon.h"
#include "TelemetryHistogram.h"
#include "ipc/TelemetryIPCAccumulator.h"

#include "base/histogram.h"

using base::Histogram;
using base::BooleanHistogram;
using base::CountHistogram;
using base::FlagHistogram;
using base::LinearHistogram;
using mozilla::StaticMutex;
using mozilla::StaticMutexAutoLock;
using mozilla::Telemetry::Accumulation;
using mozilla::Telemetry::KeyedAccumulation;
using mozilla::Telemetry::HistogramID;
using mozilla::Telemetry::ProcessID;
using mozilla::Telemetry::HistogramCount;
using mozilla::Telemetry::Common::LogToBrowserConsole;
using mozilla::Telemetry::Common::RecordedProcessType;
using mozilla::Telemetry::Common::AutoHashtable;
using mozilla::Telemetry::Common::IsExpiredVersion;
using mozilla::Telemetry::Common::CanRecordDataset;
using mozilla::Telemetry::Common::IsInDataset;

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
  RecordedProcessType record_in_processes;
  bool keyed;

  const char *name() const;
  const char *expiration() const;
  nsresult label_id(const char* label, uint32_t* labelId) const;
};

enum reflectStatus {
  REFLECT_OK,
  REFLECT_FAILURE
};

enum class SessionType {
  Session = 0,
  Subsession = 1,
  Count,
};

class KeyedHistogram {
public:
  KeyedHistogram(HistogramID id, const HistogramInfo& info);
  nsresult GetHistogram(const nsCString& name, Histogram** histogram, bool subsession);
  Histogram* GetHistogram(const nsCString& name, bool subsession);
  uint32_t GetHistogramType() const { return mHistogramInfo.histogramType; }
  nsresult GetJSKeys(JSContext* cx, JS::CallArgs& args);
  nsresult GetJSSnapshot(JSContext* cx, JS::Handle<JSObject*> obj,
                         bool subsession, bool clearSubsession);

  nsresult Add(const nsCString& key, uint32_t aSample);
  void Clear(bool subsession);

  HistogramID GetHistogramID() const { return mId; }

private:
  typedef nsBaseHashtableET<nsCStringHashKey, Histogram*> KeyedHistogramEntry;
  typedef AutoHashtable<KeyedHistogramEntry> KeyedHistogramMapType;
  KeyedHistogramMapType mHistogramMap;
#if !defined(MOZ_WIDGET_ANDROID)
  KeyedHistogramMapType mSubsessionMap;
#endif

  static bool ReflectKeyedHistogram(KeyedHistogramEntry* entry,
                                    JSContext* cx,
                                    JS::Handle<JSObject*> obj);

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
Histogram* gHistogramStorage[HistogramCount][uint32_t(ProcessID::Count)][uint32_t(SessionType::Count)] = {};
// Keyed histograms internally map string keys to individual Histogram instances.
// KeyedHistogram keeps track of session & subsession histograms internally.
KeyedHistogram* gKeyedHistogramStorage[HistogramCount][uint32_t(ProcessID::Count)] = {};

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

// Factory function for histogram instances.
Histogram*
internal_CreateHistogramInstance(const HistogramInfo& info);

bool
internal_IsHistogramEnumId(HistogramID aID)
{
  static_assert(((HistogramID)-1 > 0), "ID should be unsigned.");
  return aID < HistogramCount;
}

// Look up a plain histogram by id.
Histogram*
internal_GetHistogramById(HistogramID histogramId, ProcessID processId, SessionType sessionType,
                          bool instantiate = true)
{
  MOZ_ASSERT(internal_IsHistogramEnumId(histogramId));
  MOZ_ASSERT(!gHistogramInfos[histogramId].keyed);
  MOZ_ASSERT(processId < ProcessID::Count);
  MOZ_ASSERT(sessionType < SessionType::Count);

  Histogram* h = gHistogramStorage[histogramId][uint32_t(processId)][uint32_t(sessionType)];
  if (h || !instantiate) {
    return h;
  }

  const HistogramInfo& info = gHistogramInfos[histogramId];
  h = internal_CreateHistogramInstance(info);
  MOZ_ASSERT(h);
  gHistogramStorage[histogramId][uint32_t(processId)][uint32_t(sessionType)] = h;
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

  KeyedHistogram* kh = gKeyedHistogramStorage[histogramId][uint32_t(processId)];
  if (kh || !instantiate) {
    return kh;
  }

  const HistogramInfo& info = gHistogramInfos[histogramId];
  kh = new KeyedHistogram(histogramId, info);
  gKeyedHistogramStorage[histogramId][uint32_t(processId)] = kh;

  return kh;
}

// Look up a histogram id from a histogram name.
nsresult
internal_GetHistogramIdByName(const nsACString& name, HistogramID* id)
{
  const bool found = gNameToHistogramIDMap.Get(name, id);
  if (!found) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  return NS_OK;
}

// Clear a histogram from storage.
void
internal_ClearHistogramById(HistogramID histogramId, ProcessID processId, SessionType sessionType)
{
  delete gHistogramStorage[histogramId][uint32_t(processId)][uint32_t(sessionType)];
  gHistogramStorage[histogramId][uint32_t(processId)][uint32_t(sessionType)] = nullptr;
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

// Note: this is completely unrelated to mozilla::IsEmpty.
bool
internal_IsEmpty(const Histogram *h)
{
  Histogram::SampleSet ss;
  h->SnapshotSample(&ss);
  return ss.counts(0) == 0 && ss.sum() == 0;
}

bool
internal_IsExpired(Histogram* h)
{
  return h == gExpiredHistogram;
}

void
internal_SetHistogramRecordingEnabled(HistogramID id, bool aEnabled)
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

nsresult
internal_GetRegisteredHistogramIds(bool keyed, uint32_t dataset,
                                   uint32_t *aCount, char*** aHistograms)
{
  nsTArray<char*> collection;

  for (const auto & h : gHistogramInfos) {
    if (IsExpiredVersion(h.expiration()) ||
        h.keyed != keyed ||
        !IsInDataset(h.dataset, dataset)) {
      continue;
    }

    const char* id = h.name();
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
internal_CreateHistogramInstance(const HistogramInfo& passedInfo)
{
  if (NS_FAILED(internal_CheckHistogramArguments(passedInfo))) {
    MOZ_ASSERT(false, "Failed histogram argument checks.");
    return nullptr;
  }

  // To keep the code simple we map all the calls to expired histograms to the same histogram instance.
  // We create that instance lazily when needed.
  const bool isExpired = IsExpiredVersion(passedInfo.expiration());
  HistogramInfo info = passedInfo;

  if (isExpired) {
    if (gExpiredHistogram) {
      return gExpiredHistogram;
    }

    info.min = 1;
    info.max = 2;
    info.bucketCount = 3;
    info.histogramType = nsITelemetry::HISTOGRAM_LINEAR;
  }

  Histogram::Flags flags = Histogram::kNoFlags;
  Histogram* h = nullptr;
  switch (info.histogramType) {
  case nsITelemetry::HISTOGRAM_EXPONENTIAL:
    h = Histogram::FactoryGet(info.min, info.max, info.bucketCount, flags);
    break;
  case nsITelemetry::HISTOGRAM_LINEAR:
  case nsITelemetry::HISTOGRAM_CATEGORICAL:
    h = LinearHistogram::FactoryGet(info.min, info.max, info.bucketCount, flags);
    break;
  case nsITelemetry::HISTOGRAM_BOOLEAN:
    h = BooleanHistogram::FactoryGet(flags);
    break;
  case nsITelemetry::HISTOGRAM_FLAG:
    h = FlagHistogram::FactoryGet(flags);
    break;
  case nsITelemetry::HISTOGRAM_COUNT:
    h = CountHistogram::FactoryGet(flags);
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
internal_HistogramAdd(Histogram& histogram,
                      const HistogramID id,
                      int32_t value)
{
  // Check if we are allowed to record the data.
  bool canRecordDataset = CanRecordDataset(gHistogramInfos[id].dataset,
                                           internal_CanRecordBase(),
                                           internal_CanRecordExtended());
  if (!canRecordDataset || !internal_IsRecordingEnabled(id)) {
    return NS_OK;
  }

  // It is safe to add to the histogram now: the subsession histogram was already
  // cloned from this so we won't add the sample twice.
  histogram.Add(value);

  return NS_OK;
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
internal_ShouldReflectHistogram(Histogram* h, HistogramID id)
{
  // Only count & flag histograms are serialized when they are empty.
  // This has historical reasons, changing this will require downstream changes.
  // The cheaper path here is to just deprecate flag histograms in favor
  // of scalars.
  uint32_t type = gHistogramInfos[id].histogramType;
  if (internal_IsEmpty(h) && (type != nsITelemetry::HISTOGRAM_FLAG)) {
    return false;
  }

  return true;
}

} // namespace

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE: class KeyedHistogram

namespace {

KeyedHistogram::KeyedHistogram(HistogramID id, const HistogramInfo& info)
  : mHistogramMap()
#if !defined(MOZ_WIDGET_ANDROID)
  , mSubsessionMap()
#endif
  , mId(id)
  , mHistogramInfo(info)
{
}

nsresult
KeyedHistogram::GetHistogram(const nsCString& key, Histogram** histogram,
                             bool subsession)
{
#if !defined(MOZ_WIDGET_ANDROID)
  KeyedHistogramMapType& map = subsession ? mSubsessionMap : mHistogramMap;
#else
  KeyedHistogramMapType& map = mHistogramMap;
#endif
  KeyedHistogramEntry* entry = map.GetEntry(key);
  if (entry) {
    *histogram = entry->mData;
    return NS_OK;
  }

  Histogram* h = internal_CreateHistogramInstance(mHistogramInfo);
  if (!h) {
    return NS_ERROR_FAILURE;
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
KeyedHistogram::Add(const nsCString& key, uint32_t sample)
{
  bool canRecordDataset = CanRecordDataset(mHistogramInfo.dataset,
                                           internal_CanRecordBase(),
                                           internal_CanRecordExtended());
  if (!canRecordDataset || !internal_IsRecordingEnabled(mId)) {
    return NS_OK;
  }

  Histogram* histogram = GetHistogram(key, false);
  MOZ_ASSERT(histogram);
  if (!histogram) {
    return NS_ERROR_FAILURE;
  }
#if !defined(MOZ_WIDGET_ANDROID)
  Histogram* subsession = GetHistogram(key, true);
  MOZ_ASSERT(subsession);
  if (!subsession) {
    return NS_ERROR_FAILURE;
  }
#endif

  histogram->Add(sample);
#if !defined(MOZ_WIDGET_ANDROID)
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
#if !defined(MOZ_WIDGET_ANDROID)
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
#if !defined(MOZ_WIDGET_ANDROID)
  KeyedHistogramMapType& map = subsession ? mSubsessionMap : mHistogramMap;
#else
  KeyedHistogramMapType& map = mHistogramMap;
#endif
  if (!map.ReflectIntoJS(&KeyedHistogram::ReflectKeyedHistogram, cx, obj)) {
    return NS_ERROR_FAILURE;
  }

#if !defined(MOZ_WIDGET_ANDROID)
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
// PRIVATE: thread-unsafe helpers for the external interface

// This is a StaticMutex rather than a plain Mutex (1) so that
// it gets initialised in a thread-safe manner the first time
// it is used, and (2) because it is never de-initialised, and
// a normal Mutex would show up as a leak in BloatView.  StaticMutex
// also has the "OffTheBooks" property, so it won't show as a leak
// in BloatView.
static StaticMutex gTelemetryHistogramMutex;

namespace {

bool
internal_RemoteAccumulate(HistogramID aId, uint32_t aSample)
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
internal_RemoteAccumulate(HistogramID aId,
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

void internal_Accumulate(HistogramID aId, uint32_t aSample)
{
  if (!internal_CanRecordBase() ||
      internal_RemoteAccumulate(aId, aSample)) {
    return;
  }

  Histogram *h = internal_GetHistogramById(aId, ProcessID::Parent, SessionType::Session);
  MOZ_ASSERT(h);
  internal_HistogramAdd(*h, aId, aSample);

#if !defined(MOZ_WIDGET_ANDROID)
  h = internal_GetHistogramById(aId, ProcessID::Parent, SessionType::Subsession);
  MOZ_ASSERT(h);
  internal_HistogramAdd(*h, aId, aSample);
#endif
}

void
internal_Accumulate(HistogramID aId,
                    const nsCString& aKey, uint32_t aSample)
{
  if (!gInitDone || !internal_CanRecordBase() ||
      internal_RemoteAccumulate(aId, aKey, aSample)) {
    return;
  }

  KeyedHistogram* keyed = internal_GetKeyedHistogramById(aId, ProcessID::Parent);
  MOZ_ASSERT(keyed);
  keyed->Add(aKey, aSample);
}

void
internal_AccumulateChild(ProcessID aProcessType, HistogramID aId, uint32_t aSample)
{
  if (!internal_CanRecordBase()) {
    return;
  }

  if (Histogram* h = internal_GetHistogramById(aId, aProcessType, SessionType::Session)) {
    internal_HistogramAdd(*h, aId, aSample);
  } else {
    NS_WARNING("Failed GetHistogramById for CHILD");
  }

#if !defined(MOZ_WIDGET_ANDROID)
  if (Histogram* h = internal_GetHistogramById(aId, aProcessType, SessionType::Subsession)) {
    internal_HistogramAdd(*h, aId, aSample);
  } else {
    NS_WARNING("Failed GetHistogramById for CHILD");
  }
#endif
}

void
internal_AccumulateChildKeyed(ProcessID aProcessType, HistogramID aId,
                              const nsCString& aKey, uint32_t aSample)
{
  if (!gInitDone || !internal_CanRecordBase()) {
    return;
  }

  KeyedHistogram* keyed = internal_GetKeyedHistogramById(aId, aProcessType);
  MOZ_ASSERT(keyed);
  keyed->Add(aKey, aSample);
}

void
internal_ClearHistogram(HistogramID id, bool onlySubsession)
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
        kh->Clear(onlySubsession);
      }
    }
  }

  // Handle plain histograms.
  // Define the session types we want to clear.
  nsTArray<SessionType> sessionTypes;
  if (!onlySubsession) {
    sessionTypes.AppendElement(SessionType::Session);
  }
#if !defined(MOZ_WIDGET_ANDROID)
  sessionTypes.AppendElement(SessionType::Subsession);
#endif

  if (sessionTypes.Length() == 0) {
    // Nothing to do here.
    return;
  }

  // Now reset the histograms instances for all processes.
  for (uint32_t sessionIdx = 0; sessionIdx < sessionTypes.Length(); ++sessionIdx) {
    for (uint32_t process = 0; process < static_cast<uint32_t>(ProcessID::Count); ++process) {
      internal_ClearHistogramById(id,
                                  static_cast<ProcessID>(process),
                                  sessionTypes[sessionIdx]);
    }
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
  nullptr, /* getProperty */
  nullptr, /* setProperty */
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
internal_JSHistogram_Add(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JSObject *obj = JS_THIS_OBJECT(cx, vp);
  MOZ_ASSERT(obj);
  if (!obj ||
      JS_GetClass(obj) != &sJSHistogramClass) {
    JS_ReportErrorASCII(cx, "Wrong JS class, expected JSHistogram class");
    return false;
  }

  JSHistogramData* data = static_cast<JSHistogramData*>(JS_GetPrivate(obj));
  MOZ_ASSERT(data);
  HistogramID id = data->histogramId;
  MOZ_ASSERT(internal_IsHistogramEnumId(id));
  uint32_t type = gHistogramInfos[id].histogramType;

  JS::CallArgs args = CallArgsFromVp(argc, vp);
  // This function should always return |undefined| and never fail but
  // rather report failures using the console.
  args.rval().setUndefined();

  if (!internal_CanRecordBase()) {
    return true;
  }

  uint32_t value = 0;
  if ((type == nsITelemetry::HISTOGRAM_COUNT) && (args.length() == 0)) {
    // If we don't have an argument for the count histogram, assume an increment of 1.
    // Otherwise, make sure to run some sanity checks on the argument.
    value = 1;
  } else if ((args.length() > 0) && args[0].isString() &&
             gHistogramInfos[id].histogramType == nsITelemetry::HISTOGRAM_CATEGORICAL) {
    // For categorical histograms we allow passing a string argument that specifies the label.
    nsAutoJSString label;
    if (!label.init(cx, args[0])) {
      LogToBrowserConsole(nsIScriptError::errorFlag, NS_LITERAL_STRING("Invalid string parameter"));
      return true;
    }

    // Get label id value.
    nsresult rv = gHistogramInfos[id].label_id(NS_ConvertUTF16toUTF8(label).get(), &value);
    if (NS_FAILED(rv)) {
      LogToBrowserConsole(nsIScriptError::errorFlag,
                          NS_LITERAL_STRING("Unknown label for categorical histogram"));
      return true;
    }
  } else {
    // All other accumulations expect one numerical argument.
    if (!args.length()) {
      LogToBrowserConsole(nsIScriptError::errorFlag, NS_LITERAL_STRING("Expected one argument"));
      return true;
    }

    if (!(args[0].isNumber() || args[0].isBoolean())) {
      LogToBrowserConsole(nsIScriptError::errorFlag, NS_LITERAL_STRING("Not a number"));
      return true;
    }

    if (!JS::ToUint32(cx, args[0], &value)) {
      LogToBrowserConsole(nsIScriptError::errorFlag, NS_LITERAL_STRING("Failed to convert argument"));
      return true;
    }
  }

  {
    StaticMutexAutoLock locker(gTelemetryHistogramMutex);
    internal_Accumulate(id, value);
  }

  return true;
}

bool
internal_JSHistogram_Snapshot(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  JSObject *obj = JS_THIS_OBJECT(cx, vp);
  if (!obj ||
      JS_GetClass(obj) != &sJSHistogramClass) {
    JS_ReportErrorASCII(cx, "Wrong JS class, expected JSHistogram class");
    return false;
  }

  JSHistogramData* data = static_cast<JSHistogramData*>(JS_GetPrivate(obj));
  MOZ_ASSERT(data);
  HistogramID id = data->histogramId;
  MOZ_ASSERT(internal_IsHistogramEnumId(id));

  // This is not good standard behavior given that we have histogram instances
  // covering multiple processes and two session types.
  // However, changing this requires some broader changes to callers.
  Histogram* h = internal_GetHistogramById(id, ProcessID::Parent, SessionType::Session);
  MOZ_ASSERT(h);

  JS::Rooted<JSObject*> snapshot(cx, JS_NewPlainObject(cx));
  if (!snapshot) {
    return false;
  }

  switch (internal_ReflectHistogramSnapshot(cx, snapshot, h)) {
  case REFLECT_FAILURE:
    return false;
  case REFLECT_OK:
    args.rval().setObject(*snapshot);
    return true;
  default:
    MOZ_ASSERT_UNREACHABLE("Unhandled reflection status.");
  }

  return true;
}

bool
internal_JSHistogram_Clear(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JSObject *obj = JS_THIS_OBJECT(cx, vp);
  if (!obj ||
      JS_GetClass(obj) != &sJSHistogramClass) {
    JS_ReportErrorASCII(cx, "Wrong JS class, expected JSHistogram class");
    return false;
  }

  JSHistogramData* data = static_cast<JSHistogramData*>(JS_GetPrivate(obj));
  MOZ_ASSERT(data);
  HistogramID id = data->histogramId;
  MOZ_ASSERT(internal_IsHistogramEnumId(id));

  bool onlySubsession = false;
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  // This function should always return |undefined| and never fail but
  // rather report failures using the console.
  args.rval().setUndefined();

#if !defined(MOZ_WIDGET_ANDROID)
  if (args.length() >= 1) {
    if (!args[0].isBoolean()) {
      JS_ReportErrorASCII(cx, "Not a boolean");
      return false;
    }

    onlySubsession = JS::ToBoolean(args[0]);
  }
#endif

  internal_ClearHistogram(id, onlySubsession);

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

  // TODO: delete in finalizer
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
//   internal_JSKeyedHistogram_SubsessionSnapshot
//   internal_JSKeyedHistogram_SnapshotSubsessionAndClear
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
  nullptr, /* getProperty */
  nullptr, /* setProperty */
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
                                     bool subsession, bool clearSubsession)
{
  JSObject *obj = JS_THIS_OBJECT(cx, vp);
  if (!obj ||
      JS_GetClass(obj) != &sJSKeyedHistogramClass) {
    JS_ReportErrorASCII(cx, "Wrong JS class, expected JSKeyedHistogram class");
    return false;
  }

  JSHistogramData* data = static_cast<JSHistogramData*>(JS_GetPrivate(obj));
  MOZ_ASSERT(data);
  HistogramID id = data->histogramId;
  MOZ_ASSERT(internal_IsHistogramEnumId(id));

  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  // This function should always return |undefined| and never fail but
  // rather report failures using the console.
  args.rval().setUndefined();

  // This is not good standard behavior given that we have histogram instances
  // covering multiple processes and two session types.
  // However, changing this requires some broader changes to callers.
  KeyedHistogram* keyed = internal_GetKeyedHistogramById(id, ProcessID::Parent, /* instantiate = */ true);
  if (!keyed) {
    JS_ReportErrorASCII(cx, "Failed to look up keyed histogram");
    return false;
  }

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
    JS_ReportErrorASCII(cx, "Failed to reflect histogram");
    return false;
  case REFLECT_OK:
    args.rval().setObject(*snapshot);
    return true;
  default:
    MOZ_CRASH("unhandled reflection status");
  }

  return true;
}

bool
internal_JSKeyedHistogram_Add(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JSObject *obj = JS_THIS_OBJECT(cx, vp);
  if (!obj ||
      JS_GetClass(obj) != &sJSKeyedHistogramClass) {
    JS_ReportErrorASCII(cx, "Wrong JS class, expected JSKeyedHistogram class");
    return false;
  }

  JSHistogramData* data = static_cast<JSHistogramData*>(JS_GetPrivate(obj));
  MOZ_ASSERT(data);
  HistogramID id = data->histogramId;
  MOZ_ASSERT(internal_IsHistogramEnumId(id));

  JS::CallArgs args = CallArgsFromVp(argc, vp);
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

  const uint32_t type = gHistogramInfos[id].histogramType;

  // If we don't have an argument for the count histogram, assume an increment of 1.
  // Otherwise, make sure to run some sanity checks on the argument.
  uint32_t value = 1;
  if ((type != nsITelemetry::HISTOGRAM_COUNT) || (args.length() == 2)) {
    if (args.length() < 2) {
      LogToBrowserConsole(nsIScriptError::errorFlag,
                          NS_LITERAL_STRING("Expected two arguments for this histogram type"));
      return true;
    }

    if (type == nsITelemetry::HISTOGRAM_CATEGORICAL && args[1].isString()) {
      // For categorical histograms we allow passing a string argument that specifies the label.

      // Get label string.
      nsAutoJSString label;
      if (!label.init(cx, args[1])) {
        LogToBrowserConsole(nsIScriptError::errorFlag, NS_LITERAL_STRING("Invalid string parameter"));
        return true;
      }

      // Get label id value.
      nsresult rv = gHistogramInfos[id].label_id(NS_ConvertUTF16toUTF8(label).get(), &value);
      if (NS_FAILED(rv)) {
        LogToBrowserConsole(nsIScriptError::errorFlag,
                            NS_LITERAL_STRING("Unknown label for categorical histogram"));
        return true;
      }
    } else {
      // All other accumulations expect one numerical argument.
      if (!(args[1].isNumber() || args[1].isBoolean())) {
        LogToBrowserConsole(nsIScriptError::errorFlag, NS_LITERAL_STRING("Not a number"));
        return true;
      }

      if (!JS::ToUint32(cx, args[1], &value)) {
        LogToBrowserConsole(nsIScriptError::errorFlag, NS_LITERAL_STRING("Failed to convert argument"));
        return true;
      }
    }
  }

  internal_Accumulate(id, NS_ConvertUTF16toUTF8(key), value);

  return true;
}

bool
internal_JSKeyedHistogram_Keys(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JSObject *obj = JS_THIS_OBJECT(cx, vp);
  if (!obj ||
      JS_GetClass(obj) != &sJSKeyedHistogramClass) {
    JS_ReportErrorASCII(cx, "Wrong JS class, expected JSKeyedHistogram class");
    return false;
  }

  JSHistogramData* data = static_cast<JSHistogramData*>(JS_GetPrivate(obj));
  MOZ_ASSERT(data);
  HistogramID id = data->histogramId;
  MOZ_ASSERT(internal_IsHistogramEnumId(id));

  // This is not good standard behavior given that we have histogram instances
  // covering multiple processes and two session types.
  // However, changing this requires some broader changes to callers.
  KeyedHistogram* keyed = internal_GetKeyedHistogramById(id, ProcessID::Parent);
  MOZ_ASSERT(keyed);
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

#if !defined(MOZ_WIDGET_ANDROID)
bool
internal_JSKeyedHistogram_SubsessionSnapshot(JSContext *cx,
                                             unsigned argc, JS::Value *vp)
{
  return internal_KeyedHistogram_SnapshotImpl(cx, argc, vp, true, false);
}
#endif

#if !defined(MOZ_WIDGET_ANDROID)
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
  if (!obj ||
      JS_GetClass(obj) != &sJSKeyedHistogramClass) {
    JS_ReportErrorASCII(cx, "Wrong JS class, expected JSKeyedHistogram class");
    return false;
  }

  JSHistogramData* data = static_cast<JSHistogramData*>(JS_GetPrivate(obj));
  MOZ_ASSERT(data);
  HistogramID id = data->histogramId;
  MOZ_ASSERT(internal_IsHistogramEnumId(id));

  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  // This function should always return |undefined| and never fail but
  // rather report failures using the console.
  args.rval().setUndefined();

  // This is not good standard behavior given that we have histogram instances
  // covering multiple processes and two session types.
  // However, changing this requires some broader changes to callers.
  KeyedHistogram* keyed = internal_GetKeyedHistogramById(id, ProcessID::Parent, /* instantiate = */ false);
  if (!keyed) {
    return true;
  }

#if !defined(MOZ_WIDGET_ANDROID)
  bool onlySubsession = false;

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
#if !defined(MOZ_WIDGET_ANDROID)
        && JS_DefineFunction(cx, obj, "subsessionSnapshot",
                             internal_JSKeyedHistogram_SubsessionSnapshot, 1, 0)
        && JS_DefineFunction(cx, obj, "snapshotSubsessionAndClear",
                             internal_JSKeyedHistogram_SnapshotSubsessionAndClear, 0, 0)
#endif
        && JS_DefineFunction(cx, obj, "keys",
                             internal_JSKeyedHistogram_Keys, 0, 0)
        && JS_DefineFunction(cx, obj, "clear",
                             internal_JSKeyedHistogram_Clear, 0, 0))) {
    return NS_ERROR_FAILURE;
  }

  // TODO: delete in finalizer
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

  // gNameToHistogramIDMap should have been pre-sized correctly at the
  // declaration point further up in this file.

  // Populate the static histogram name->id cache.
  // Note that the histogram names are statically allocated.
  for (uint32_t i = 0; i < HistogramCount; i++) {
    gNameToHistogramIDMap.Put(nsDependentCString(gHistogramInfos[i].name()), HistogramID(i));
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
    internal_SetHistogramRecordingEnabled(id,
                                          CanRecordInProcess(h.record_in_processes,
                                                             processType));
  }

  for (auto recordingInitiallyDisabledID : kRecordingInitiallyDisabledIDs) {
    internal_SetHistogramRecordingEnabled(recordingInitiallyDisabledID,
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

  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  internal_SetHistogramRecordingEnabled(aID, aEnabled);
}


nsresult
TelemetryHistogram::SetHistogramRecordingEnabled(const nsACString& name,
                                                 bool aEnabled)
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  HistogramID id;
  if (NS_FAILED(internal_GetHistogramIdByName(name, &id))) {
    return NS_ERROR_FAILURE;
  }

  const HistogramInfo& hi = gHistogramInfos[id];
  if (CanRecordInProcess(hi.record_in_processes, XRE_GetProcessType())) {
    internal_SetHistogramRecordingEnabled(id, aEnabled);
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
  internal_Accumulate(aID, aSample);
}

void
TelemetryHistogram::Accumulate(HistogramID aID,
                               const nsCString& aKey, uint32_t aSample)
{
  if (NS_WARN_IF(!internal_IsHistogramEnumId(aID))) {
    MOZ_ASSERT_UNREACHABLE("Histogram usage requires valid ids.");
    return;
  }

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
  HistogramID id;
  nsresult rv = internal_GetHistogramIdByName(nsDependentCString(name), &id);
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
  HistogramID id;
  nsresult rv = internal_GetHistogramIdByName(nsDependentCString(name), &id);
  if (NS_SUCCEEDED(rv)) {
    internal_Accumulate(id, key, sample);
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
  internal_Accumulate(aId, labelId);
}

void
TelemetryHistogram::AccumulateChild(ProcessID aProcessType,
                                    const nsTArray<Accumulation>& aAccumulations)
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
    internal_AccumulateChild(aProcessType, aAccumulations[i].mId, aAccumulations[i].mSample);
  }
}

void
TelemetryHistogram::AccumulateChildKeyed(ProcessID aProcessType,
                                         const nsTArray<KeyedAccumulation>& aAccumulations)
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
  HistogramID id;
  {
    StaticMutexAutoLock locker(gTelemetryHistogramMutex);
    nsresult rv = internal_GetHistogramIdByName(name, &id);
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
    nsresult rv = internal_GetHistogramIdByName(name, &id);
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
TelemetryHistogram::CreateHistogramSnapshots(JSContext *cx,
                                             JS::MutableHandle<JS::Value> ret,
                                             bool subsession,
                                             bool clearSubsession)
{
  // Runs without protection from |gTelemetryHistogramMutex|
  JS::Rooted<JSObject*> root_obj(cx, JS_NewPlainObject(cx));
  if (!root_obj) {
    return NS_ERROR_FAILURE;
  }
  ret.setObject(*root_obj);

  // Include the GPU process in histogram snapshots only if we actually tried
  // to launch a process for it.
  bool includeGPUProcess = false;
  if (auto gpm = mozilla::gfx::GPUProcessManager::Get()) {
    includeGPUProcess = gpm->AttemptedGPUProcess();
  }

#if !defined(MOZ_WIDGET_ANDROID)
  SessionType sessionType = SessionType(subsession);
#else
  SessionType sessionType = SessionType::Session;
#endif

  // Ensure that all the HISTOGRAM_FLAG histograms have
  // been created, so that their values are snapshotted.
  auto processType = XRE_GetProcessType();
  for (uint32_t histogramId = 0; histogramId < HistogramCount; ++histogramId) {
    const HistogramInfo& info = gHistogramInfos[histogramId];
    if (info.keyed ||
      info.histogramType != nsITelemetry::HISTOGRAM_FLAG ||
      !CanRecordInProcess(info.record_in_processes, processType)) {
      continue;
    }

    for (uint32_t process = 0; process < static_cast<uint32_t>(ProcessID::Count); ++process) {
      if ((ProcessID(process) == ProcessID::Gpu) && !includeGPUProcess) {
        continue;
      }

      mozilla::DebugOnly<Histogram*> h = nullptr;
      h = internal_GetHistogramById(HistogramID(histogramId), ProcessID(process), sessionType);
      MOZ_ASSERT(h);
    }
  }

  // TODO: This won't get us data from different processes.
  // We'll have to refactor this function to return {"parent": {...}, "content": {...}}.

  // OK, now we can actually reflect things.
  JS::Rooted<JSObject*> hobj(cx);
  for (size_t i = 0; i < HistogramCount; ++i) {
    const HistogramInfo& info = gHistogramInfos[i];
    if (info.keyed) {
      continue;
    }

    HistogramID id = HistogramID(i);

    // TODO: support multiple processes.
    //for (uint32_t process = 0; process < static_cast<uint32_t>(ProcessID::Count); ++process) {

    uint32_t process = uint32_t(ProcessID::Parent);
    if (!CanRecordInProcess(info.record_in_processes, ProcessID(process)) ||
      ((ProcessID(process) == ProcessID::Gpu) && !includeGPUProcess)) {
      continue;
    }

    Histogram* h = internal_GetHistogramById(id, ProcessID(process), sessionType, /* instantiate = */ false);
    if (!h || internal_IsExpired(h) || !internal_ShouldReflectHistogram(h, id)) {
      continue;
    }

    hobj = JS_NewPlainObject(cx);
    if (!hobj) {
      return NS_ERROR_FAILURE;
    }
    switch (internal_ReflectHistogramSnapshot(cx, hobj, h)) {
    case REFLECT_FAILURE:
      return NS_ERROR_FAILURE;
    case REFLECT_OK:
      if (!JS_DefineProperty(cx, root_obj, gHistogramInfos[id].name(),
                             hobj, JSPROP_ENUMERATE)) {
        return NS_ERROR_FAILURE;
      }
    }

#if !defined(MOZ_WIDGET_ANDROID)
    if ((sessionType == SessionType::Subsession) && clearSubsession) {
      h->Clear();
    }
#endif

    // TODO: support multiple processes.
    // }
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

  // for (auto iter = gKeyedHistograms.Iter(); !iter.Done(); iter.Next()) {
  for (uint32_t id = 0; id < HistogramCount; ++id) {
    if (!gHistogramInfos[id].keyed) {
      continue;
    }

    // TODO: This won't get us data from different processes.
    // We'll have to refactor this function to return {"parent": {...}, "content": {...}}.

    // We don't want to trigger instantiation of empty keyed histograms.
    KeyedHistogram* keyed = internal_GetKeyedHistogramById(HistogramID(id), ProcessID::Parent,
                                                           /* instantiate = */ false);
    if (!keyed) {
      continue;
    }

    JS::RootedObject snapshot(cx, JS_NewPlainObject(cx));
    if (!snapshot) {
      return NS_ERROR_FAILURE;
    }

    if (!NS_SUCCEEDED(keyed->GetJSSnapshot(cx, snapshot, false, false))) {
      return NS_ERROR_FAILURE;
    }

    if (!JS_DefineProperty(cx, obj, gHistogramInfos[id].name(),
                           snapshot, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
  }

  ret.setObject(*obj);
  return NS_OK;
}

size_t
TelemetryHistogram::GetMapShallowSizesOfExcludingThis(mozilla::MallocSizeOf
                                                      aMallocSizeOf)
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  // TODO
  return 0;
}

size_t
TelemetryHistogram::GetHistogramSizesofIncludingThis(mozilla::MallocSizeOf
                                                     aMallocSizeOf)
{
  StaticMutexAutoLock locker(gTelemetryHistogramMutex);
  // TODO
  return 0;
}
