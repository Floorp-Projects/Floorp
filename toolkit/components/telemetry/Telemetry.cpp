/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/histogram.h"
#include "base/pickle.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "mozilla/ModuleUtils.h"
#include "nsIXPConnect.h"
#include "mozilla/Services.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "nsStringGlue.h"
#include "nsITelemetry.h"
#include "nsIFile.h"
#include "Telemetry.h" 
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsBaseHashtable.h"
#include "nsXULAppAPI.h"
#include "nsThreadUtils.h"
#include "mozilla/Mutex.h"
#include "mozilla/FileUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/Attributes.h"

namespace {

using namespace base;
using namespace mozilla;

template<class EntryType>
class AutoHashtable : public nsTHashtable<EntryType>
{
public:
  AutoHashtable(uint32_t initSize = PL_DHASH_MIN_SIZE);
  ~AutoHashtable();
  typedef bool (*ReflectEntryFunc)(EntryType *entry, JSContext *cx, JSObject *obj);
  bool ReflectIntoJS(ReflectEntryFunc entryFunc, JSContext *cx, JSObject *obj);
private:
  struct EnumeratorArgs {
    JSContext *cx;
    JSObject *obj;
    ReflectEntryFunc entryFunc;
  };
  static PLDHashOperator ReflectEntryStub(EntryType *entry, void *arg);
};

template<class EntryType>
AutoHashtable<EntryType>::AutoHashtable(uint32_t initSize)
{
  this->Init(initSize);
}

template<class EntryType>
AutoHashtable<EntryType>::~AutoHashtable()
{
  this->Clear();
}

template<typename EntryType>
PLDHashOperator
AutoHashtable<EntryType>::ReflectEntryStub(EntryType *entry, void *arg)
{
  EnumeratorArgs *args = static_cast<EnumeratorArgs *>(arg);
  if (!args->entryFunc(entry, args->cx, args->obj)) {
    return PL_DHASH_STOP;
  }
  return PL_DHASH_NEXT;
}

/**
 * Reflect the individual entries of table into JS, usually by defining
 * some property and value of obj.  entryFunc is called for each entry.
 */
template<typename EntryType>
bool
AutoHashtable<EntryType>::ReflectIntoJS(ReflectEntryFunc entryFunc,
                                        JSContext *cx, JSObject *obj)
{
  EnumeratorArgs args = { cx, obj, entryFunc };
  uint32_t num = this->EnumerateEntries(ReflectEntryStub, static_cast<void*>(&args));
  return num == this->Count();
}

class TelemetryImpl MOZ_FINAL : public nsITelemetry
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSITELEMETRY

public:
  TelemetryImpl();
  ~TelemetryImpl();
  
  static bool CanRecord();
  static already_AddRefed<nsITelemetry> CreateTelemetryInstance();
  static void ShutdownTelemetry();
  static void RecordSlowStatement(const nsACString &sql, const nsACString &dbName,
                                  uint32_t delay, bool isDynamicString);
#if defined(MOZ_ENABLE_PROFILER_SPS)
  static void RecordChromeHang(uint32_t duration,
                               const Telemetry::HangStack &callStack,
                               SharedLibraryInfo &moduleMap);
#endif
  static nsresult GetHistogramEnumId(const char *name, Telemetry::ID *id);
  struct StmtStats {
    uint32_t hitCount;
    uint32_t totalTime;
    bool isDynamicSql;
    bool isTrackedDb;
    bool isAggregate;
  };
  typedef nsBaseHashtableET<nsCStringHashKey, StmtStats> SlowSQLEntryType;
  struct HangReport {
    uint32_t duration;
    Telemetry::HangStack callStack;
#if defined(MOZ_ENABLE_PROFILER_SPS)
    SharedLibraryInfo moduleMap;
#endif
  };

private:
  static void StoreSlowSQL(const nsACString &offender, uint32_t delay,
                           bool isDynamicSql, bool isTrackedDB, bool isAggregate);

  static bool ReflectPublicSql(SlowSQLEntryType *entry, JSContext *cx,
                               JSObject *obj);
  static bool ReflectPrivateSql(SlowSQLEntryType *entry, JSContext *cx,
                                JSObject *obj);
  static bool ReflectSql(SlowSQLEntryType *entry, JSContext *cx, JSObject *obj);

  bool AddSQLInfo(JSContext *cx, JSObject *rootObj, bool mainThread,
                  bool includePrivateStrings);
  bool GetSQLStats(JSContext *cx, jsval *ret, bool includePrivateSql);

  // Like GetHistogramById, but returns the underlying C++ object, not the JS one.
  nsresult GetHistogramByName(const nsACString &name, Histogram **ret);
  bool ShouldReflectHistogram(Histogram *h);
  void IdentifyCorruptHistograms(StatisticsRecorder::Histograms &hs);
  typedef StatisticsRecorder::Histograms::iterator HistogramIterator;

  struct AddonHistogramInfo {
    uint32_t min;
    uint32_t max;
    uint32_t bucketCount;
    uint32_t histogramType;
    Histogram *h;
  };
  typedef nsBaseHashtableET<nsCStringHashKey, AddonHistogramInfo> AddonHistogramEntryType;
  typedef AutoHashtable<AddonHistogramEntryType> AddonHistogramMapType;
  typedef nsBaseHashtableET<nsCStringHashKey, AddonHistogramMapType *> AddonEntryType;
  typedef AutoHashtable<AddonEntryType> AddonMapType;
  static bool AddonHistogramReflector(AddonHistogramEntryType *entry,
                                      JSContext *cx, JSObject *obj);
  static bool AddonReflector(AddonEntryType *entry, JSContext *cx, JSObject *obj);
  static bool CreateHistogramForAddon(const nsACString &name,
                                      AddonHistogramInfo &info);
  AddonMapType mAddonMap;

  // This is used for speedy string->Telemetry::ID conversions
  typedef nsBaseHashtableET<nsCharPtrHashKey, Telemetry::ID> CharPtrEntryType;
  typedef AutoHashtable<CharPtrEntryType> HistogramMapType;
  HistogramMapType mHistogramMap;
  bool mCanRecord;
  static TelemetryImpl *sTelemetry;
  AutoHashtable<SlowSQLEntryType> mSlowSQLOnMainThread;
  AutoHashtable<SlowSQLEntryType> mSlowSQLOnOtherThread;
  // This gets marked immutable in debug builds, so we can't use
  // AutoHashtable here.
  nsTHashtable<nsCStringHashKey> mTrackedDBs;
  Mutex mHashMutex;
  nsTArray<HangReport> mHangReports;
  Mutex mHangReportsMutex;
};

TelemetryImpl*  TelemetryImpl::sTelemetry = NULL;

// A initializer to initialize histogram collection
StatisticsRecorder gStatisticsRecorder;

// Hardcoded probes
struct TelemetryHistogram {
  const char *id;
  uint32_t min;
  uint32_t max;
  uint32_t bucketCount;
  uint32_t histogramType;
  const char *comment;
};

// Perform the checks at the beginning of HistogramGet at compile time, so
// that if people add incorrect histogram definitions, they get compiler
// errors.
#define HISTOGRAM(id, min, max, bucket_count, histogram_type, b) \
  MOZ_STATIC_ASSERT(nsITelemetry::HISTOGRAM_ ## histogram_type == nsITelemetry::HISTOGRAM_BOOLEAN || \
                    nsITelemetry::HISTOGRAM_ ## histogram_type == nsITelemetry::HISTOGRAM_FLAG || \
                    (min < max && bucket_count > 2 && min >= 1), \
                    "Incorrect histogram definitions were found");

#include "TelemetryHistograms.h"

#undef HISTOGRAM

const TelemetryHistogram gHistograms[] = {
#define HISTOGRAM(id, min, max, bucket_count, histogram_type, comment) \
  { NS_STRINGIFY(id), min, max, bucket_count, \
    nsITelemetry::HISTOGRAM_ ## histogram_type, comment },

#include "TelemetryHistograms.h"

#undef HISTOGRAM
};
bool gCorruptHistograms[Telemetry::HistogramCount];

bool
TelemetryHistogramType(Histogram *h, uint32_t *result)
{
  switch (h->histogram_type()) {
  case Histogram::HISTOGRAM:
    *result = nsITelemetry::HISTOGRAM_EXPONENTIAL;
    break;
  case Histogram::LINEAR_HISTOGRAM:
    *result = nsITelemetry::HISTOGRAM_LINEAR;
    break;
  case Histogram::BOOLEAN_HISTOGRAM:
    *result = nsITelemetry::HISTOGRAM_BOOLEAN;
    break;
  case Histogram::FLAG_HISTOGRAM:
    *result = nsITelemetry::HISTOGRAM_FLAG;
    break;
  default:
    return false;
  }
  return true;
}

nsresult
HistogramGet(const char *name, uint32_t min, uint32_t max, uint32_t bucketCount,
             uint32_t histogramType, Histogram **result)
{
  if (histogramType != nsITelemetry::HISTOGRAM_BOOLEAN
      && histogramType != nsITelemetry::HISTOGRAM_FLAG) {
    // Sanity checks for histogram parameters.
    if (min >= max)
      return NS_ERROR_ILLEGAL_VALUE;

    if (bucketCount <= 2)
      return NS_ERROR_ILLEGAL_VALUE;

    if (min < 1)
      return NS_ERROR_ILLEGAL_VALUE;
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
  default:
    return NS_ERROR_INVALID_ARG;
  }
  return NS_OK;
}

// O(1) histogram lookup by numeric id
nsresult
GetHistogramByEnumId(Telemetry::ID id, Histogram **ret)
{
  static Histogram* knownHistograms[Telemetry::HistogramCount] = {0};
  Histogram *h = knownHistograms[id];
  if (h) {
    *ret = h;
    return NS_OK;
  }

  const TelemetryHistogram &p = gHistograms[id];
  nsresult rv = HistogramGet(p.id, p.min, p.max, p.bucketCount, p.histogramType, &h);
  if (NS_FAILED(rv))
    return rv;

  *ret = knownHistograms[id] = h;
  return NS_OK;
}

bool
FillRanges(JSContext *cx, JSObject *array, Histogram *h)
{
  for (size_t i = 0; i < h->bucket_count(); i++) {
    if (!JS_DefineElement(cx, array, i, INT_TO_JSVAL(h->ranges(i)), NULL, NULL, JSPROP_ENUMERATE))
      return false;
  }
  return true;
}

enum reflectStatus {
  REFLECT_OK,
  REFLECT_CORRUPT,
  REFLECT_FAILURE
};

enum reflectStatus
ReflectHistogramAndSamples(JSContext *cx, JSObject *obj, Histogram *h,
                           const Histogram::SampleSet &ss)
{
  // We don't want to reflect corrupt histograms.
  if (h->FindCorruption(ss) != Histogram::NO_INCONSISTENCIES) {
    return REFLECT_CORRUPT;
  }

  if (!(JS_DefineProperty(cx, obj, "min", INT_TO_JSVAL(h->declared_min()), NULL, NULL, JSPROP_ENUMERATE)
        && JS_DefineProperty(cx, obj, "max", INT_TO_JSVAL(h->declared_max()), NULL, NULL, JSPROP_ENUMERATE)
        && JS_DefineProperty(cx, obj, "histogram_type", INT_TO_JSVAL(h->histogram_type()), NULL, NULL, JSPROP_ENUMERATE)
        && JS_DefineProperty(cx, obj, "sum", DOUBLE_TO_JSVAL(ss.sum()), NULL, NULL, JSPROP_ENUMERATE))) {
    return REFLECT_FAILURE;
  }

  const size_t count = h->bucket_count();
  JSObject *rarray = JS_NewArrayObject(cx, count, nsnull);
  if (!rarray) {
    return REFLECT_FAILURE;
  }
  JS::AutoObjectRooter aroot(cx, rarray);
  if (!(FillRanges(cx, rarray, h)
        && JS_DefineProperty(cx, obj, "ranges", OBJECT_TO_JSVAL(rarray),
                             NULL, NULL, JSPROP_ENUMERATE))) {
    return REFLECT_FAILURE;
  }

  JSObject *counts_array = JS_NewArrayObject(cx, count, NULL);
  if (!counts_array) {
    return REFLECT_FAILURE;
  }
  JS::AutoObjectRooter croot(cx, counts_array);
  if (!JS_DefineProperty(cx, obj, "counts", OBJECT_TO_JSVAL(counts_array),
                         NULL, NULL, JSPROP_ENUMERATE)) {
    return REFLECT_FAILURE;
  }
  for (size_t i = 0; i < count; i++) {
    if (!JS_DefineElement(cx, counts_array, i, INT_TO_JSVAL(ss.counts(i)),
                          NULL, NULL, JSPROP_ENUMERATE)) {
      return REFLECT_FAILURE;
    }
  }
 
  return REFLECT_OK;
}

enum reflectStatus
ReflectHistogramSnapshot(JSContext *cx, JSObject *obj, Histogram *h)
{
  Histogram::SampleSet ss;
  h->SnapshotSample(&ss);
  return ReflectHistogramAndSamples(cx, obj, h, ss);
}

bool
IsEmpty(const Histogram *h)
{
  Histogram::SampleSet ss;
  h->SnapshotSample(&ss);

  return ss.counts(0) == 0 && ss.sum() == 0;
}

JSBool
JSHistogram_Add(JSContext *cx, unsigned argc, jsval *vp)
{
  if (!argc) {
    JS_ReportError(cx, "Expected one argument");
    return JS_FALSE;
  }

  jsval v = JS_ARGV(cx, vp)[0];

  if (!(JSVAL_IS_NUMBER(v) || JSVAL_IS_BOOLEAN(v))) {
    JS_ReportError(cx, "Not a number");
    return JS_FALSE;
  }

  int32_t value;
  if (!JS_ValueToECMAInt32(cx, v, &value)) {
    return JS_FALSE;
  }

  if (TelemetryImpl::CanRecord()) {
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj) {
      return JS_FALSE;
    }

    Histogram *h = static_cast<Histogram*>(JS_GetPrivate(obj));
    if (h->histogram_type() == Histogram::BOOLEAN_HISTOGRAM)
      h->Add(!!value);
    else
      h->Add(value);
  }
  return JS_TRUE;
}

JSBool
JSHistogram_Snapshot(JSContext *cx, unsigned argc, jsval *vp)
{
  JSObject *obj = JS_THIS_OBJECT(cx, vp);
  if (!obj) {
    return JS_FALSE;
  }

  Histogram *h = static_cast<Histogram*>(JS_GetPrivate(obj));
  JSObject *snapshot = JS_NewObject(cx, nsnull, nsnull, nsnull);
  if (!snapshot)
    return JS_FALSE;
  JS::AutoObjectRooter sroot(cx, snapshot);

  switch (ReflectHistogramSnapshot(cx, snapshot, h)) {
  case REFLECT_FAILURE:
    return JS_FALSE;
  case REFLECT_CORRUPT:
    JS_ReportError(cx, "Histogram is corrupt");
    return JS_FALSE;
  case REFLECT_OK:
    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(snapshot));
    return JS_TRUE;
  default:
    MOZ_NOT_REACHED("unhandled reflection status");
    return JS_FALSE;
  }
}

JSBool
JSHistogram_Clear(JSContext *cx, unsigned argc, jsval *vp)
{
  JSObject *obj = JS_THIS_OBJECT(cx, vp);
  if (!obj) {
    return JS_FALSE;
  }

  Histogram *h = static_cast<Histogram*>(JS_GetPrivate(obj));
  h->Clear();
  return JS_TRUE;
}

nsresult 
WrapAndReturnHistogram(Histogram *h, JSContext *cx, jsval *ret)
{
  static JSClass JSHistogram_class = {
    "JSHistogram",  /* name */
    JSCLASS_HAS_PRIVATE, /* flags */
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub
  };

  JSObject *obj = JS_NewObject(cx, &JSHistogram_class, NULL, NULL);
  if (!obj)
    return NS_ERROR_FAILURE;
  JS::AutoObjectRooter root(cx, obj);
  if (!(JS_DefineFunction(cx, obj, "add", JSHistogram_Add, 1, 0)
        && JS_DefineFunction(cx, obj, "snapshot", JSHistogram_Snapshot, 0, 0)
        && JS_DefineFunction(cx, obj, "clear", JSHistogram_Clear, 0, 0))) {
    return NS_ERROR_FAILURE;
  }
  *ret = OBJECT_TO_JSVAL(obj);
  JS_SetPrivate(obj, h);
  return NS_OK;
}

TelemetryImpl::TelemetryImpl():
mHistogramMap(Telemetry::HistogramCount),
mCanRecord(XRE_GetProcessType() == GeckoProcessType_Default),
mHashMutex("Telemetry::mHashMutex"),
mHangReportsMutex("Telemetry::mHangReportsMutex")
{
  // A whitelist to prevent Telemetry reporting on Addon & Thunderbird DBs
  const char *trackedDBs[] = {
    "addons.sqlite", "chromeappsstore.sqlite", "content-prefs.sqlite",
    "cookies.sqlite", "downloads.sqlite", "extensions.sqlite",
    "formhistory.sqlite", "index.sqlite", "permissions.sqlite", "places.sqlite",
    "search.sqlite", "signons.sqlite", "urlclassifier3.sqlite",
    "webappsstore.sqlite"
  };

  mTrackedDBs.Init();
  for (size_t i = 0; i < ArrayLength(trackedDBs); i++)
    mTrackedDBs.PutEntry(nsDependentCString(trackedDBs[i]));

#ifdef DEBUG
  // Mark immutable to prevent asserts on simultaneous access from multiple threads
  mTrackedDBs.MarkImmutable();
#endif
}

TelemetryImpl::~TelemetryImpl() {
}

NS_IMETHODIMP
TelemetryImpl::NewHistogram(const nsACString &name, uint32_t min, uint32_t max, uint32_t bucketCount, uint32_t histogramType, JSContext *cx, jsval *ret)
{
  Histogram *h;
  nsresult rv = HistogramGet(PromiseFlatCString(name).get(), min, max, bucketCount, histogramType, &h);
  if (NS_FAILED(rv))
    return rv;
  h->ClearFlags(Histogram::kUmaTargetedHistogramFlag);
  return WrapAndReturnHistogram(h, cx, ret);
}

bool
TelemetryImpl::ReflectSql(SlowSQLEntryType *entry, JSContext *cx, JSObject *obj)
{
  const nsACString &sql = entry->GetKey();
  jsval hitCount = UINT_TO_JSVAL(entry->mData.hitCount);
  jsval totalTime = UINT_TO_JSVAL(entry->mData.totalTime);

  JSObject *arrayObj = JS_NewArrayObject(cx, 0, nsnull);
  if (!arrayObj) {
    return false;
  }
  JS::AutoObjectRooter root(cx, arrayObj);
  return (JS_SetElement(cx, arrayObj, 0, &hitCount)
          && JS_SetElement(cx, arrayObj, 1, &totalTime)
          && JS_DefineProperty(cx, obj,
                               sql.BeginReading(),
                               OBJECT_TO_JSVAL(arrayObj),
                               NULL, NULL, JSPROP_ENUMERATE));
}

bool
TelemetryImpl::ReflectPublicSql(SlowSQLEntryType *entry, JSContext *cx,
                                JSObject *obj)
{
  bool isPrivateSql = entry->mData.isDynamicSql || (!entry->mData.isTrackedDb);
  if (!isPrivateSql || entry->mData.isAggregate)
    return ReflectSql(entry, cx, obj);
  return true;
}

bool
TelemetryImpl::ReflectPrivateSql(SlowSQLEntryType *entry, JSContext *cx,
                                 JSObject *obj)
{
  if (!entry->mData.isAggregate)
    return ReflectSql(entry, cx, obj);
  return true;
}

bool
TelemetryImpl::AddSQLInfo(JSContext *cx, JSObject *rootObj, bool mainThread,
                          bool includePrivateStrings)
{
  JSObject *statsObj = JS_NewObject(cx, NULL, NULL, NULL);
  if (!statsObj)
    return false;
  JS::AutoObjectRooter root(cx, statsObj);

  AutoHashtable<SlowSQLEntryType> &sqlMap =
    (mainThread ? mSlowSQLOnMainThread : mSlowSQLOnOtherThread);
  AutoHashtable<SlowSQLEntryType>::ReflectEntryFunc reflectFunction =
    (includePrivateStrings ? ReflectPrivateSql : ReflectPublicSql);
  if(!sqlMap.ReflectIntoJS(reflectFunction, cx, statsObj)) {
    return false;
  }

  return JS_DefineProperty(cx, rootObj,
                           mainThread ? "mainThread" : "otherThreads",
                           OBJECT_TO_JSVAL(statsObj),
                           NULL, NULL, JSPROP_ENUMERATE);
}

nsresult
TelemetryImpl::GetHistogramEnumId(const char *name, Telemetry::ID *id)
{
  if (!sTelemetry) {
    return NS_ERROR_FAILURE;
  }

  // Cache names
  // Note the histogram names are statically allocated
  TelemetryImpl::HistogramMapType *map = &sTelemetry->mHistogramMap;
  if (!map->Count()) {
    for (uint32_t i = 0; i < Telemetry::HistogramCount; i++) {
      CharPtrEntryType *entry = map->PutEntry(gHistograms[i].id);
      if (NS_UNLIKELY(!entry)) {
        map->Clear();
        return NS_ERROR_OUT_OF_MEMORY;
      }
      entry->mData = (Telemetry::ID) i;
    }
  }

  CharPtrEntryType *entry = map->GetEntry(name);
  if (!entry) {
    return NS_ERROR_INVALID_ARG;
  }
  *id = entry->mData;
  return NS_OK;
}

nsresult
TelemetryImpl::GetHistogramByName(const nsACString &name, Histogram **ret)
{
  Telemetry::ID id;
  nsresult rv = GetHistogramEnumId(PromiseFlatCString(name).get(), &id);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = GetHistogramByEnumId(id, ret);
  if (NS_FAILED(rv))
    return rv;

  return NS_OK;
}

NS_IMETHODIMP
TelemetryImpl::HistogramFrom(const nsACString &name, const nsACString &existing_name,
                             JSContext *cx, jsval *ret)
{
  Histogram *existing;
  nsresult rv = GetHistogramByName(existing_name, &existing);
  if (NS_FAILED(rv))
    return rv;

  uint32_t histogramType;
  bool success = TelemetryHistogramType(existing, &histogramType);
  if (!success)
    return NS_ERROR_INVALID_ARG;

  Histogram *clone;
  rv = HistogramGet(PromiseFlatCString(name).get(), existing->declared_min(),
                    existing->declared_max(), existing->bucket_count(),
                    histogramType, &clone);
  if (NS_FAILED(rv))
    return rv;

  Histogram::SampleSet ss;
  existing->SnapshotSample(&ss);
  clone->AddSampleSet(ss);
  return WrapAndReturnHistogram(clone, cx, ret);
}

void
TelemetryImpl::IdentifyCorruptHistograms(StatisticsRecorder::Histograms &hs)
{
  for (HistogramIterator it = hs.begin(); it != hs.end(); ++it) {
    Histogram *h = *it;

    Telemetry::ID id;
    nsresult rv = GetHistogramEnumId(h->histogram_name().c_str(), &id);
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
      Telemetry::ID corruptID = Telemetry::HistogramCount;
      if (check & Histogram::RANGE_CHECKSUM_ERROR) {
        corruptID = Telemetry::RANGE_CHECKSUM_ERRORS;
      } else if (check & Histogram::BUCKET_ORDER_ERROR) {
        corruptID = Telemetry::BUCKET_ORDER_ERRORS;
      } else if (check & Histogram::COUNT_HIGH_ERROR) {
        corruptID = Telemetry::TOTAL_COUNT_HIGH_ERRORS;
      } else if (check & Histogram::COUNT_LOW_ERROR) {
        corruptID = Telemetry::TOTAL_COUNT_LOW_ERRORS;
      }
      Telemetry::Accumulate(corruptID, 1);
    }

    gCorruptHistograms[id] = corrupt;
  }
}

bool
TelemetryImpl::ShouldReflectHistogram(Histogram *h)
{
  const char *name = h->histogram_name().c_str();
  Telemetry::ID id;
  nsresult rv = GetHistogramEnumId(name, &id);
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

// Compute the name to pass into Histogram for the addon histogram
// 'name' from the addon 'id'.  We can't use 'name' directly because it
// might conflict with other histograms in other addons or even with our
// own.
void
AddonHistogramName(const nsACString &id, const nsACString &name,
                   nsACString &ret)
{
  ret.Append(id);
  ret.Append(NS_LITERAL_CSTRING(":"));
  ret.Append(name);
}

NS_IMETHODIMP
TelemetryImpl::RegisterAddonHistogram(const nsACString &id,
                                      const nsACString &name,
                                      uint32_t min, uint32_t max,
                                      uint32_t bucketCount,
                                      uint32_t histogramType)
{
  AddonEntryType *addonEntry = mAddonMap.GetEntry(id);
  if (!addonEntry) {
    addonEntry = mAddonMap.PutEntry(id);
    if (NS_UNLIKELY(!addonEntry)) {
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
  if (NS_UNLIKELY(!histogramEntry)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  AddonHistogramInfo &info = histogramEntry->mData;
  info.min = min;
  info.max = max;
  info.bucketCount = bucketCount;
  info.histogramType = histogramType;

  return NS_OK;
}

NS_IMETHODIMP
TelemetryImpl::GetAddonHistogram(const nsACString &id, const nsACString &name,
                                 JSContext *cx, jsval *ret)
{
  AddonEntryType *addonEntry = mAddonMap.GetEntry(id);
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
    nsCAutoString actualName;
    AddonHistogramName(id, name, actualName);
    if (!CreateHistogramForAddon(actualName, info)) {
      return NS_ERROR_FAILURE;
    }
  }
  return WrapAndReturnHistogram(info.h, cx, ret);
}

NS_IMETHODIMP
TelemetryImpl::UnregisterAddonHistograms(const nsACString &id)
{
  AddonEntryType *addonEntry = mAddonMap.GetEntry(id);
  if (addonEntry) {
    // Histogram's destructor is private, so this is the best we can do.
    // The histograms the addon created *will* stick around, but they
    // will be deleted if and when the addon registers histograms with
    // the same names.
    delete addonEntry->mData;
    mAddonMap.RemoveEntry(id);
  }

  return NS_OK;
}

NS_IMETHODIMP
TelemetryImpl::GetHistogramSnapshots(JSContext *cx, jsval *ret)
{
  JSObject *root_obj = JS_NewObject(cx, NULL, NULL, NULL);
  if (!root_obj)
    return NS_ERROR_FAILURE;
  *ret = OBJECT_TO_JSVAL(root_obj);

  // Ensure that all the HISTOGRAM_FLAG histograms have been created, so
  // that their values are snapshotted.
  for (size_t i = 0; i < Telemetry::HistogramCount; ++i) {
    if (gHistograms[i].histogramType == nsITelemetry::HISTOGRAM_FLAG) {
      Histogram *h;
      DebugOnly<nsresult> rv = GetHistogramByEnumId(Telemetry::ID(i), &h);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  };

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
  for (HistogramIterator it = hs.begin(); it != hs.end(); ++it) {
    Histogram *h = *it;
    if (!ShouldReflectHistogram(h) || IsEmpty(h)) {
      continue;
    }

    JSObject *hobj = JS_NewObject(cx, NULL, NULL, NULL);
    if (!hobj) {
      return NS_ERROR_FAILURE;
    }
    JS::AutoObjectRooter root(cx, hobj);
    switch (ReflectHistogramSnapshot(cx, hobj, h)) {
    case REFLECT_CORRUPT:
      // We can still hit this case even if ShouldReflectHistograms
      // returns true.  The histogram lies outside of our control
      // somehow; just skip it.
      continue;
    case REFLECT_FAILURE:
      return NS_ERROR_FAILURE;
    case REFLECT_OK:
      if (!JS_DefineProperty(cx, root_obj, h->histogram_name().c_str(),
                             OBJECT_TO_JSVAL(hobj), NULL, NULL, JSPROP_ENUMERATE)) {
        return NS_ERROR_FAILURE;
      }
    }
  }
  return NS_OK;
}

bool
TelemetryImpl::CreateHistogramForAddon(const nsACString &name,
                                       AddonHistogramInfo &info)
{
  Histogram *h;
  nsresult rv = HistogramGet(PromiseFlatCString(name).get(),
                             info.min, info.max, info.bucketCount,
                             info.histogramType, &h);
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
TelemetryImpl::AddonHistogramReflector(AddonHistogramEntryType *entry,
                                       JSContext *cx, JSObject *obj)
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

  JSObject *snapshot = JS_NewObject(cx, NULL, NULL, NULL);
  if (!snapshot) {
    // Just consider this to be skippable.
    return true;
  }
  JS::AutoObjectRooter r(cx, snapshot);
  switch (ReflectHistogramSnapshot(cx, snapshot, info.h)) {
  case REFLECT_FAILURE:
  case REFLECT_CORRUPT:
    return false;
  case REFLECT_OK:
    const nsACString &histogramName = entry->GetKey();
    if (!JS_DefineProperty(cx, obj,
                           PromiseFlatCString(histogramName).get(),
                           OBJECT_TO_JSVAL(snapshot), NULL, NULL,
                           JSPROP_ENUMERATE)) {
      return false;
    }
    break;
  }
  return true;
}

bool
TelemetryImpl::AddonReflector(AddonEntryType *entry,
                              JSContext *cx, JSObject *obj)
{
  const nsACString &addonId = entry->GetKey();
  JSObject *subobj = JS_NewObject(cx, NULL, NULL, NULL);
  if (!subobj) {
    return false;
  }
  JS::AutoObjectRooter r(cx, subobj);

  AddonHistogramMapType *map = entry->mData;
  if (!(map->ReflectIntoJS(AddonHistogramReflector, cx, subobj)
        && JS_DefineProperty(cx, obj,
                             PromiseFlatCString(addonId).get(),
                             OBJECT_TO_JSVAL(subobj), NULL, NULL,
                             JSPROP_ENUMERATE))) {
    return false;
  }
  return true;
}

NS_IMETHODIMP
TelemetryImpl::GetAddonHistogramSnapshots(JSContext *cx, jsval *ret)
{
  *ret = JSVAL_VOID;
  JSObject *obj = JS_NewObject(cx, NULL, NULL, NULL);
  if (!obj) {
    return NS_ERROR_FAILURE;
  }
  JS::AutoObjectRooter r(cx, obj);

  if (!mAddonMap.ReflectIntoJS(AddonReflector, cx, obj)) {
    return NS_ERROR_FAILURE;
  }
  *ret = OBJECT_TO_JSVAL(obj);
  return NS_OK;
}

bool
TelemetryImpl::GetSQLStats(JSContext *cx, jsval *ret, bool includePrivateSql)
{
  JSObject *root_obj = JS_NewObject(cx, NULL, NULL, NULL);
  if (!root_obj)
    return false;
  *ret = OBJECT_TO_JSVAL(root_obj);

  MutexAutoLock hashMutex(mHashMutex);
  // Add info about slow SQL queries on the main thread
  if (!AddSQLInfo(cx, root_obj, true, includePrivateSql))
    return false;
  // Add info about slow SQL queries on other threads
  if (!AddSQLInfo(cx, root_obj, false, includePrivateSql))
    return false;
  
  return true;
}

NS_IMETHODIMP
TelemetryImpl::GetSlowSQL(JSContext *cx, jsval *ret)
{
  if (GetSQLStats(cx, ret, false))
    return NS_OK;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelemetryImpl::GetDebugSlowSQL(JSContext *cx, jsval *ret)
{
  bool revealPrivateSql =
    Preferences::GetBool("toolkit.telemetry.debugSlowSql", false);
  if (GetSQLStats(cx, ret, revealPrivateSql))
    return NS_OK;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelemetryImpl::GetChromeHangs(JSContext *cx, jsval *ret)
{
  MutexAutoLock hangReportMutex(mHangReportsMutex);
  JSObject *reportArray = JS_NewArrayObject(cx, 0, nsnull);
  if (!reportArray) {
    return NS_ERROR_FAILURE;
  }
  *ret = OBJECT_TO_JSVAL(reportArray);

  // Each hang report is an object in the 'chromeHangs' array
  for (size_t i = 0; i < mHangReports.Length(); ++i) {
    JSObject *reportObj = JS_NewObject(cx, NULL, NULL, NULL);
    if (!reportObj) {
      return NS_ERROR_FAILURE;
    }
    jsval reportObjVal = OBJECT_TO_JSVAL(reportObj);
    if (!JS_SetElement(cx, reportArray, i, &reportObjVal)) {
      return NS_ERROR_FAILURE;
    }

    // Record the hang duration (expressed in seconds)
    JSBool ok = JS_DefineProperty(cx, reportObj, "duration",
                                  INT_TO_JSVAL(mHangReports[i].duration),
                                  NULL, NULL, JSPROP_ENUMERATE);
    if (!ok) {
      return NS_ERROR_FAILURE;
    }

    // Represent call stack PCs as strings
    // (JS can't represent all 64-bit integer values)
    JSObject *pcArray = JS_NewArrayObject(cx, 0, nsnull);
    if (!pcArray) {
      return NS_ERROR_FAILURE;
    }
    ok = JS_DefineProperty(cx, reportObj, "stack", OBJECT_TO_JSVAL(pcArray),
                           NULL, NULL, JSPROP_ENUMERATE);
    if (!ok) {
      return NS_ERROR_FAILURE;
    }

    const uint32_t pcCount = mHangReports[i].callStack.Length();
    for (size_t pcIndex = 0; pcIndex < pcCount; ++pcIndex) {
      nsCAutoString pcString;
      pcString.AppendPrintf("0x%p", mHangReports[i].callStack[pcIndex]);
      JSString *str = JS_NewStringCopyZ(cx, pcString.get());
      if (!str) {
        return NS_ERROR_FAILURE;
      }
      jsval v = STRING_TO_JSVAL(str);
      if (!JS_SetElement(cx, pcArray, pcIndex, &v)) {
        return NS_ERROR_FAILURE;
      }
    }

    // Record memory map info
    JSObject *moduleArray = JS_NewArrayObject(cx, 0, nsnull);
    if (!moduleArray) {
      return NS_ERROR_FAILURE;
    }
    ok = JS_DefineProperty(cx, reportObj, "memoryMap",
                           OBJECT_TO_JSVAL(moduleArray),
                           NULL, NULL, JSPROP_ENUMERATE);
    if (!ok) {
      return NS_ERROR_FAILURE;
    }

#if defined(MOZ_ENABLE_PROFILER_SPS)
    const uint32_t moduleCount = mHangReports[i].moduleMap.GetSize();
    for (size_t moduleIndex = 0; moduleIndex < moduleCount; ++moduleIndex) {
      // Current module
      const SharedLibrary &module =
        mHangReports[i].moduleMap.GetEntry(moduleIndex);

      JSObject *moduleInfoArray = JS_NewArrayObject(cx, 0, nsnull);
      if (!moduleInfoArray) {
        return NS_ERROR_FAILURE;
      }
      jsval val = OBJECT_TO_JSVAL(moduleInfoArray);
      if (!JS_SetElement(cx, moduleArray, moduleIndex, &val)) {
        return NS_ERROR_FAILURE;
      }

      // Start address
      nsCAutoString addressString;
      addressString.AppendPrintf("0x%p", module.GetStart());
      JSString *str = JS_NewStringCopyZ(cx, addressString.get());
      if (!str) {
        return NS_ERROR_FAILURE;
      }
      val = STRING_TO_JSVAL(str);
      if (!JS_SetElement(cx, moduleInfoArray, 0, &val)) {
        return NS_ERROR_FAILURE;
      }

      // Module name
      str = JS_NewStringCopyZ(cx, module.GetName());
      if (!str) {
        return NS_ERROR_FAILURE;
      }
      val = STRING_TO_JSVAL(str);
      if (!JS_SetElement(cx, moduleInfoArray, 1, &val)) {
        return NS_ERROR_FAILURE;
      }

      // Module size in memory
      val = INT_TO_JSVAL(int32_t(module.GetEnd() - module.GetStart()));
      if (!JS_SetElement(cx, moduleInfoArray, 2, &val)) {
        return NS_ERROR_FAILURE;
      }

      // "PDB Age" identifier
      val = INT_TO_JSVAL(0);
#if defined(MOZ_PROFILING) && defined(XP_WIN)
      val = INT_TO_JSVAL(module.GetPdbAge());
#endif
      if (!JS_SetElement(cx, moduleInfoArray, 3, &val)) {
        return NS_ERROR_FAILURE;
      }

      // "PDB Signature" GUID
      char guidString[NSID_LENGTH] = { 0 };
#if defined(MOZ_PROFILING) && defined(XP_WIN)
      module.GetPdbSignature().ToProvidedString(guidString);
#endif
      str = JS_NewStringCopyZ(cx, guidString);
      if (!str) {
        return NS_ERROR_FAILURE;
      }
      val = STRING_TO_JSVAL(str);
      if (!JS_SetElement(cx, moduleInfoArray, 4, &val)) {
        return NS_ERROR_FAILURE;
      }

      // Name of associated PDB file
      const char *pdbName = "";
#if defined(MOZ_PROFILING) && defined(XP_WIN)
      pdbName = module.GetPdbName();
#endif
      str = JS_NewStringCopyZ(cx, pdbName);
      if (!str) {
        return NS_ERROR_FAILURE;
      }
      val = STRING_TO_JSVAL(str);
      if (!JS_SetElement(cx, moduleInfoArray, 5, &val)) {
        return NS_ERROR_FAILURE;
      }
    }
#endif
  }

  return NS_OK;
}

NS_IMETHODIMP
TelemetryImpl::GetRegisteredHistograms(JSContext *cx, jsval *ret)
{
  size_t count = ArrayLength(gHistograms);
  JSObject *info = JS_NewObject(cx, NULL, NULL, NULL);
  if (!info)
    return NS_ERROR_FAILURE;
  JS::AutoObjectRooter root(cx, info);

  for (size_t i = 0; i < count; ++i) {
    JSString *comment = JS_InternString(cx, gHistograms[i].comment);
    
    if (!(comment
          && JS_DefineProperty(cx, info, gHistograms[i].id,
                               STRING_TO_JSVAL(comment), NULL, NULL,
                               JSPROP_ENUMERATE))) {
      return NS_ERROR_FAILURE;
    }
  }

  *ret = OBJECT_TO_JSVAL(info);
  return NS_OK;
}

NS_IMETHODIMP
TelemetryImpl::GetHistogramById(const nsACString &name, JSContext *cx, jsval *ret)
{
  Histogram *h;
  nsresult rv = GetHistogramByName(name, &h);
  if (NS_FAILED(rv))
    return rv;

  return WrapAndReturnHistogram(h, cx, ret);
}

NS_IMETHODIMP
TelemetryImpl::GetCanRecord(bool *ret) {
  *ret = mCanRecord;
  return NS_OK;
}

NS_IMETHODIMP
TelemetryImpl::SetCanRecord(bool canRecord) {
  mCanRecord = !!canRecord;
  return NS_OK;
}

bool
TelemetryImpl::CanRecord() {
  return !sTelemetry || sTelemetry->mCanRecord;
}

NS_IMETHODIMP
TelemetryImpl::GetCanSend(bool *ret) {
#if defined(MOZILLA_OFFICIAL) && defined(MOZ_TELEMETRY_REPORTING)
  *ret = true;
#else
  *ret = false;
#endif
  return NS_OK;
}

already_AddRefed<nsITelemetry>
TelemetryImpl::CreateTelemetryInstance()
{
  NS_ABORT_IF_FALSE(sTelemetry == NULL, "CreateTelemetryInstance may only be called once, via GetService()");
  sTelemetry = new TelemetryImpl(); 
  // AddRef for the local reference
  NS_ADDREF(sTelemetry);
  // AddRef for the caller
  NS_ADDREF(sTelemetry);
  return sTelemetry;
}

void
TelemetryImpl::ShutdownTelemetry()
{
  NS_IF_RELEASE(sTelemetry);
}

void
TelemetryImpl::StoreSlowSQL(const nsACString &sql, uint32_t delay,
                            bool isDynamicSql, bool isTrackedDB, bool isAggregate)
{
  AutoHashtable<SlowSQLEntryType> *slowSQLMap = NULL;
  if (NS_IsMainThread())
    slowSQLMap = &(sTelemetry->mSlowSQLOnMainThread);
  else
    slowSQLMap = &(sTelemetry->mSlowSQLOnOtherThread);

  MutexAutoLock hashMutex(sTelemetry->mHashMutex);

  SlowSQLEntryType *entry = slowSQLMap->GetEntry(sql);
  if (!entry) {
    entry = slowSQLMap->PutEntry(sql);
    if (NS_UNLIKELY(!entry))
      return;
    entry->mData.isDynamicSql = isDynamicSql;
    entry->mData.isTrackedDb = isTrackedDB;
    entry->mData.isAggregate = isAggregate;

    entry->mData.hitCount = 0;
    entry->mData.totalTime = 0;
  }

  entry->mData.hitCount++;
  entry->mData.totalTime += delay;
}

void
TelemetryImpl::RecordSlowStatement(const nsACString &sql, const nsACString &dbName,
                                   uint32_t delay, bool isDynamicString)
{
  MOZ_ASSERT(sTelemetry);
  if (!sTelemetry->mCanRecord)
    return;

  bool isTrackedDb = sTelemetry->mTrackedDBs.Contains(dbName);
  bool isPrivate = (!isTrackedDb) || isDynamicString;
  if (isPrivate) {
    // Report aggregate DB-level statistics to Telemetry for potentially
    // sensitive SQL strings
    nsCAutoString aggregate;
    aggregate.AppendPrintf("Untracked SQL for %s", dbName.BeginReading());
    StoreSlowSQL(aggregate, delay, isDynamicString, isTrackedDb, true);
  }

  // Record original SQL string
  nsCAutoString fullSql(sql);
  if (!isTrackedDb)
    fullSql.AppendPrintf(" -- Untracked DB %s", dbName.BeginReading());
  StoreSlowSQL(fullSql, delay, isDynamicString, isTrackedDb, false);
}

#if defined(MOZ_ENABLE_PROFILER_SPS)
void
TelemetryImpl::RecordChromeHang(uint32_t duration,
                                const Telemetry::HangStack &callStack,
                                SharedLibraryInfo &moduleMap)
{
  MOZ_ASSERT(sTelemetry);
  if (!sTelemetry->mCanRecord) {
    return;
  }

  MutexAutoLock hangReportMutex(sTelemetry->mHangReportsMutex);

  // Only report the modules which changed since the first hang report
  if (sTelemetry->mHangReports.Length()) {
    SharedLibraryInfo &firstModuleMap =
      sTelemetry->mHangReports[0].moduleMap;
    for (size_t i = 0; i < moduleMap.GetSize(); ++i) {
      if (firstModuleMap.Contains(moduleMap.GetEntry(i))) {
        moduleMap.RemoveEntries(i, i + 1);
        --i;
      }
    }
  }

  HangReport newReport = { duration, callStack, moduleMap };
  sTelemetry->mHangReports.AppendElement(newReport);
}
#endif

NS_IMPL_THREADSAFE_ISUPPORTS1(TelemetryImpl, nsITelemetry)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsITelemetry, TelemetryImpl::CreateTelemetryInstance)

#define NS_TELEMETRY_CID \
  {0xaea477f2, 0xb3a2, 0x469c, {0xaa, 0x29, 0x0a, 0x82, 0xd1, 0x32, 0xb8, 0x29}}
NS_DEFINE_NAMED_CID(NS_TELEMETRY_CID);

const Module::CIDEntry kTelemetryCIDs[] = {
  { &kNS_TELEMETRY_CID, false, NULL, nsITelemetryConstructor },
  { NULL }
};

const Module::ContractIDEntry kTelemetryContracts[] = {
  { "@mozilla.org/base/telemetry;1", &kNS_TELEMETRY_CID },
  { NULL }
};

const Module kTelemetryModule = {
  Module::kVersion,
  kTelemetryCIDs,
  kTelemetryContracts,
  NULL,
  NULL,
  NULL,
  TelemetryImpl::ShutdownTelemetry
};

} // anonymous namespace

namespace mozilla {
namespace Telemetry {

void
Accumulate(ID aHistogram, uint32_t aSample)
{
  if (!TelemetryImpl::CanRecord()) {
    return;
  }
  Histogram *h;
  nsresult rv = GetHistogramByEnumId(aHistogram, &h);
  if (NS_SUCCEEDED(rv))
    h->Add(aSample);
}

void
AccumulateTimeDelta(ID aHistogram, TimeStamp start, TimeStamp end)
{
  Accumulate(aHistogram,
             static_cast<uint32_t>((end - start).ToMilliseconds()));
}

bool
CanRecord()
{
  return TelemetryImpl::CanRecord();
}

base::Histogram*
GetHistogramById(ID id)
{
  Histogram *h = NULL;
  GetHistogramByEnumId(id, &h);
  return h;
}

void
RecordSlowSQLStatement(const nsACString &statement,
                       const nsACString &dbName,
                       uint32_t delay,
                       bool isDynamicString)
{
  TelemetryImpl::RecordSlowStatement(statement, dbName, delay, isDynamicString);
}

void Init()
{
  // Make the service manager hold a long-lived reference to the service
  nsCOMPtr<nsITelemetry> telemetryService =
    do_GetService("@mozilla.org/base/telemetry;1");
  MOZ_ASSERT(telemetryService);
}

#if defined(MOZ_ENABLE_PROFILER_SPS)
void RecordChromeHang(uint32_t duration,
                      const Telemetry::HangStack &callStack,
                      SharedLibraryInfo &moduleMap)
{
  TelemetryImpl::RecordChromeHang(duration, callStack, moduleMap);
}
#endif

} // namespace Telemetry
} // namespace mozilla

NSMODULE_DEFN(nsTelemetryModule) = &kTelemetryModule;

/**
 * The XRE_TelemetryAdd function is to be used by embedding applications
 * that can't use mozilla::Telemetry::Accumulate() directly.
 */
void
XRE_TelemetryAccumulate(int aID, uint32_t aSample)
{
  mozilla::Telemetry::Accumulate((mozilla::Telemetry::ID) aID, aSample);
}
