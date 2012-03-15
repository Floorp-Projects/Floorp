/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Taras Glek <tglek@mozilla.com>
 *   Vladan Djeric <vdjeric@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "base/histogram.h"
#include "base/pickle.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "mozilla/ModuleUtils.h"
#include "nsIXPConnect.h"
#include "mozilla/Services.h"
#include "jsapi.h" 
#include "nsStringGlue.h"
#include "nsITelemetry.h"
#include "nsIFile.h"
#include "nsILocalFile.h"
#include "Telemetry.h" 
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsBaseHashtable.h"
#include "nsXULAppAPI.h"
#include "nsThreadUtils.h"
#include "mozilla/Mutex.h"
#include "mozilla/FileUtils.h"

namespace {

using namespace base;
using namespace mozilla;

template<class EntryType>
class AutoHashtable : public nsTHashtable<EntryType>
{
public:
  AutoHashtable(PRUint32 initSize = PL_DHASH_MIN_SIZE);
  ~AutoHashtable();
  typedef bool (*ReflectEntryFunc)(EntryType *entry, JSContext *cx, JSObject *obj);
  bool ReflectHashtable(ReflectEntryFunc entryFunc, JSContext *cx, JSObject *obj);
private:
  struct EnumeratorArgs {
    JSContext *cx;
    JSObject *obj;
    ReflectEntryFunc entryFunc;
  };
  static PLDHashOperator ReflectEntryStub(EntryType *entry, void *arg);
};

template<class EntryType>
AutoHashtable<EntryType>::AutoHashtable(PRUint32 initSize)
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
AutoHashtable<EntryType>::ReflectHashtable(ReflectEntryFunc entryFunc,
                                           JSContext *cx, JSObject *obj)
{
  EnumeratorArgs args = { cx, obj, entryFunc };
  PRUint32 num = this->EnumerateEntries(ReflectEntryStub, static_cast<void*>(&args));
  return num == this->Count();
}

class TelemetryImpl : public nsITelemetry
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSITELEMETRY

public:
  TelemetryImpl();
  ~TelemetryImpl();
  
  static bool CanRecord();
  static already_AddRefed<nsITelemetry> CreateTelemetryInstance();
  static void ShutdownTelemetry();
  static void RecordSlowStatement(const nsACString &statement,
                                  const nsACString &dbName,
                                  PRUint32 delay);
#if defined(MOZ_ENABLE_PROFILER_SPS)
  static void RecordChromeHang(PRUint32 duration,
                               const Telemetry::HangStack &callStack,
                               SharedLibraryInfo &moduleMap);
#endif
  static nsresult GetHistogramEnumId(const char *name, Telemetry::ID *id);
  struct StmtStats {
    PRUint32 hitCount;
    PRUint32 totalTime;
  };
  typedef nsBaseHashtableET<nsCStringHashKey, StmtStats> SlowSQLEntryType;
  struct HangReport {
    PRUint32 duration;
    Telemetry::HangStack callStack;
#if defined(MOZ_ENABLE_PROFILER_SPS)
    SharedLibraryInfo moduleMap;
#endif
  };

private:
  static bool StatementReflector(SlowSQLEntryType *entry, JSContext *cx,
                                 JSObject *obj);
  bool AddSQLInfo(JSContext *cx, JSObject *rootObj, bool mainThread);

  // Like GetHistogramById, but returns the underlying C++ object, not the JS one.
  nsresult GetHistogramByName(const nsACString &name, Histogram **ret);
  bool ShouldReflectHistogram(Histogram *h);
  void IdentifyCorruptHistograms(StatisticsRecorder::Histograms &hs);
  typedef StatisticsRecorder::Histograms::iterator HistogramIterator;

  struct AddonHistogramInfo {
    PRUint32 min;
    PRUint32 max;
    PRUint32 bucketCount;
    PRUint32 histogramType;
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
  PRUint32 min;
  PRUint32 max;
  PRUint32 bucketCount;
  PRUint32 histogramType;
  const char *comment;
};

// Perform the checks at the beginning of HistogramGet at compile time, so
// that if people add incorrect histogram definitions, they get compiler
// errors.
#define HISTOGRAM(id, min, max, bucket_count, histogram_type, b) \
  PR_STATIC_ASSERT(nsITelemetry::HISTOGRAM_ ## histogram_type == nsITelemetry::HISTOGRAM_BOOLEAN || \
                   nsITelemetry::HISTOGRAM_ ## histogram_type == nsITelemetry::HISTOGRAM_FLAG || \
                   (min < max && bucket_count > 2 && min >= 1));

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
TelemetryHistogramType(Histogram *h, PRUint32 *result)
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
  default:
    return false;
  }
  return true;
}

nsresult
HistogramGet(const char *name, PRUint32 min, PRUint32 max, PRUint32 bucketCount,
             PRUint32 histogramType, Histogram **result)
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

JSBool
JSHistogram_Add(JSContext *cx, unsigned argc, jsval *vp)
{
  if (!argc) {
    JS_ReportError(cx, "Expected one argument");
    return JS_FALSE;
  }

  jsval v = JS_ARGV(cx, vp)[0];
  int32 value;

  if (!(JSVAL_IS_NUMBER(v) || JSVAL_IS_BOOLEAN(v))) {
    JS_ReportError(cx, "Not a number");
    return JS_FALSE;
  }

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

nsresult 
WrapAndReturnHistogram(Histogram *h, JSContext *cx, jsval *ret)
{
  static JSClass JSHistogram_class = {
    "JSHistogram",  /* name */
    JSCLASS_HAS_PRIVATE, /* flags */
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
  };

  JSObject *obj = JS_NewObject(cx, &JSHistogram_class, NULL, NULL);
  if (!obj)
    return NS_ERROR_FAILURE;
  JS::AutoObjectRooter root(cx, obj);
  if (!(JS_DefineFunction (cx, obj, "add", JSHistogram_Add, 1, 0)
        && JS_DefineFunction (cx, obj, "snapshot", JSHistogram_Snapshot, 1, 0))) {
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
TelemetryImpl::NewHistogram(const nsACString &name, PRUint32 min, PRUint32 max, PRUint32 bucketCount, PRUint32 histogramType, JSContext *cx, jsval *ret)
{
  Histogram *h;
  nsresult rv = HistogramGet(PromiseFlatCString(name).get(), min, max, bucketCount, histogramType, &h);
  if (NS_FAILED(rv))
    return rv;
  h->ClearFlags(Histogram::kUmaTargetedHistogramFlag);
  return WrapAndReturnHistogram(h, cx, ret);
}

bool
TelemetryImpl::StatementReflector(SlowSQLEntryType *entry, JSContext *cx,
                                  JSObject *obj)
{
  const nsACString &sql = entry->GetKey();
  jsval hitCount = UINT_TO_JSVAL(entry->mData.hitCount);
  jsval totalTime = UINT_TO_JSVAL(entry->mData.totalTime);

  JSObject *arrayObj = JS_NewArrayObject(cx, 2, nsnull);
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
TelemetryImpl::AddSQLInfo(JSContext *cx, JSObject *rootObj, bool mainThread)
{
  JSObject *statsObj = JS_NewObject(cx, NULL, NULL, NULL);
  if (!statsObj)
    return false;
  JS::AutoObjectRooter root(cx, statsObj);

  AutoHashtable<SlowSQLEntryType> &sqlMap = (mainThread
                                             ? mSlowSQLOnMainThread
                                             : mSlowSQLOnOtherThread);
  if (!sqlMap.ReflectHashtable(StatementReflector, cx, statsObj)) {
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
    for (PRUint32 i = 0; i < Telemetry::HistogramCount; i++) {
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

  PRUint32 histogramType;
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
                                      PRUint32 min, PRUint32 max,
                                      PRUint32 bucketCount,
                                      PRUint32 histogramType)
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
    if (!ShouldReflectHistogram(h)) {
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
  if (!(map->ReflectHashtable(AddonHistogramReflector, cx, subobj)
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

  if (!mAddonMap.ReflectHashtable(AddonReflector, cx, obj)) {
    return NS_ERROR_FAILURE;
  }
  *ret = OBJECT_TO_JSVAL(obj);
  return NS_OK;
}

NS_IMETHODIMP
TelemetryImpl::GetSlowSQL(JSContext *cx, jsval *ret)
{
  JSObject *root_obj = JS_NewObject(cx, NULL, NULL, NULL);
  if (!root_obj)
    return NS_ERROR_FAILURE;
  *ret = OBJECT_TO_JSVAL(root_obj);

  MutexAutoLock hashMutex(mHashMutex);
  // Add info about slow SQL queries on the main thread
  if (!AddSQLInfo(cx, root_obj, true))
    return NS_ERROR_FAILURE;
  // Add info about slow SQL queries on other threads
  if (!AddSQLInfo(cx, root_obj, false))
    return NS_ERROR_FAILURE;

  return NS_OK;
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

    const PRUint32 pcCount = mHangReports[i].callStack.Length();
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
    const PRUint32 moduleCount = mHangReports[i].moduleMap.GetSize();
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

class TelemetrySessionData : public nsITelemetrySessionData
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSITELEMETRYSESSIONDATA

public:
  static nsresult LoadFromDisk(nsIFile *, TelemetrySessionData **ptr);
  static nsresult SaveToDisk(nsIFile *, const nsACString &uuid);

  TelemetrySessionData(const char *uuid);
  ~TelemetrySessionData();

private:
  typedef nsBaseHashtableET<nsUint32HashKey, Histogram::SampleSet> EntryType;
  typedef AutoHashtable<EntryType> SessionMapType;
  static bool SampleReflector(EntryType *entry, JSContext *cx, JSObject *obj);
  SessionMapType mSampleSetMap;
  nsCString mUUID;

  bool DeserializeHistogramData(Pickle &pickle, void **iter);
  static bool SerializeHistogramData(Pickle &pickle);

  // The file format version.  Should be incremented whenever we change
  // how individual SampleSets are stored in the file.
  static const unsigned int sVersion = 1;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(TelemetrySessionData, nsITelemetrySessionData)

TelemetrySessionData::TelemetrySessionData(const char *uuid)
  : mUUID(uuid)
{
}

TelemetrySessionData::~TelemetrySessionData()
{
}

NS_IMETHODIMP
TelemetrySessionData::GetUuid(nsACString &uuid)
{
  uuid = mUUID;
  return NS_OK;
}

bool
TelemetrySessionData::SampleReflector(EntryType *entry, JSContext *cx,
                                      JSObject *snapshots)
{
  // Don't reflect histograms with no data associated with them.
  if (entry->mData.sum() == 0) {
    return true;
  }

  // This has the undesirable effect of creating a histogram for the
  // current session with the given ID.  But there's no good way to
  // compute the ranges and buckets from scratch.
  Histogram *h = nsnull;
  nsresult rv = GetHistogramByEnumId(Telemetry::ID(entry->GetKey()), &h);
  if (NS_FAILED(rv)) {
    return true;
  }

  JSObject *snapshot = JS_NewObject(cx, NULL, NULL, NULL);
  if (!snapshot) {
    return false;
  }
  JS::AutoObjectRooter root(cx, snapshot);
  switch (ReflectHistogramAndSamples(cx, snapshot, h, entry->mData)) {
  case REFLECT_OK:
    return JS_DefineProperty(cx, snapshots,
                             h->histogram_name().c_str(),
                             OBJECT_TO_JSVAL(snapshot), NULL, NULL,
                             JSPROP_ENUMERATE);
  case REFLECT_CORRUPT:
    // Just ignore this one.
    return true;
  case REFLECT_FAILURE:
    return false;
  default:
    MOZ_NOT_REACHED("unhandled reflection status");
    return false;
  }
}

NS_IMETHODIMP
TelemetrySessionData::GetSnapshots(JSContext *cx, jsval *ret)
{
  JSObject *snapshots = JS_NewObject(cx, NULL, NULL, NULL);
  if (!snapshots) {
    return NS_ERROR_FAILURE;
  }
  JS::AutoObjectRooter root(cx, snapshots);

  if (!mSampleSetMap.ReflectHashtable(SampleReflector, cx, snapshots)) {
    return NS_ERROR_FAILURE;
  }

  *ret = OBJECT_TO_JSVAL(snapshots);
  return NS_OK;
}

bool
TelemetrySessionData::DeserializeHistogramData(Pickle &pickle, void **iter)
{
  PRUint32 count = 0;
  if (!pickle.ReadUInt32(iter, &count)) {
    return false;
  }

  for (size_t i = 0; i < count; ++i) {
    int stored_length;
    const char *name;
    if (!pickle.ReadData(iter, &name, &stored_length)) {
      return false;
    }

    Telemetry::ID id;
    nsresult rv = TelemetryImpl::GetHistogramEnumId(name, &id);
    if (NS_FAILED(rv)) {
      // We serialized a non-static histogram or we serialized a
      // histogram that is no longer defined in TelemetryHistograms.h.
      // Just drop its data on the floor.  If we can't deserialize the
      // data, though, we're in trouble.
      Histogram::SampleSet ss;
      if (!ss.Deserialize(iter, pickle)) {
        return false;
      }
    } else {
      EntryType *entry = mSampleSetMap.GetEntry(id);
      if (!entry) {
        entry = mSampleSetMap.PutEntry(id);
        if (NS_UNLIKELY(!entry)) {
          return false;
        }
        if (!entry->mData.Deserialize(iter, pickle)) {
          return false;
        }
      }
    }
  }

  return true;
}

nsresult
TelemetrySessionData::LoadFromDisk(nsIFile *file, TelemetrySessionData **ptr)
{
  *ptr = nsnull;
  nsresult rv;
  nsCOMPtr<nsILocalFile> f(do_QueryInterface(file, &rv));
  if (NS_FAILED(rv)) {
    return rv;
  }

  AutoFDClose fd;
  rv = f->OpenNSPRFileDesc(PR_RDONLY, 0, &fd);
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  // If there's not even enough data to read the header for the pickle,
  // don't bother.  Conveniently, this handles the error case as well.
  PRInt32 size = PR_Available(fd);
  if (size < static_cast<PRInt32>(sizeof(Pickle::Header))) {
    return NS_ERROR_FAILURE;
  }

  nsAutoArrayPtr<char> data(new char[size]);
  PRInt32 amount = PR_Read(fd, data, size);
  if (amount != size) {
    return NS_ERROR_FAILURE;
  }

  Pickle pickle(data, size);
  void *iter = NULL;

  // Make sure that how much data the pickle thinks it has corresponds
  // with how much data we actually read.
  const Pickle::Header *header = pickle.headerT<Pickle::Header>();
  if (header->payload_size != static_cast<PRUint32>(amount) - sizeof(*header)) {
    return NS_ERROR_FAILURE;
  }

  unsigned int storedVersion;
  if (!(pickle.ReadUInt32(&iter, &storedVersion)
        && storedVersion == sVersion)) {
    return NS_ERROR_FAILURE;
  }

  const char *uuid;
  int uuidLength;
  if (!pickle.ReadData(&iter, &uuid, &uuidLength)) {
    return NS_ERROR_FAILURE;
  }

  nsAutoPtr<TelemetrySessionData> sessionData(new TelemetrySessionData(uuid));
  if (!sessionData->DeserializeHistogramData(pickle, &iter)) {
    return NS_ERROR_FAILURE;
  }

  *ptr = sessionData.forget();
  return NS_OK;
}

bool
TelemetrySessionData::SerializeHistogramData(Pickle &pickle)
{
  StatisticsRecorder::Histograms hs;
  StatisticsRecorder::GetHistograms(&hs);

  if (!pickle.WriteUInt32(hs.size())) {
    return false;
  }

  for (StatisticsRecorder::Histograms::const_iterator it = hs.begin();
       it != hs.end();
       ++it) {
    const Histogram *h = *it;
    const char *name = h->histogram_name().c_str();

    Histogram::SampleSet ss;
    h->SnapshotSample(&ss);

    if (!(pickle.WriteData(name, strlen(name)+1)
          && ss.Serialize(&pickle))) {
      return false;
    }
  }

  return true;
}

nsresult
TelemetrySessionData::SaveToDisk(nsIFile *file, const nsACString &uuid)
{
  nsresult rv;
  nsCOMPtr<nsILocalFile> f(do_QueryInterface(file, &rv));
  if (NS_FAILED(rv)) {
    return rv;
  }

  AutoFDClose fd;
  rv = f->OpenNSPRFileDesc(PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 0600, &fd);
  if (NS_FAILED(rv)) {
    return rv;
  }

  Pickle pickle;
  if (!pickle.WriteUInt32(sVersion)) {
    return NS_ERROR_FAILURE;
  }

  // Include the trailing NULL for the UUID to make reading easier.
  const char *data;
  size_t length = uuid.GetData(&data);
  if (!(pickle.WriteData(data, length+1)
        && SerializeHistogramData(pickle))) {
    return NS_ERROR_FAILURE;
  }

  PRInt32 amount = PR_Write(fd, static_cast<const char*>(pickle.data()),
                            pickle.size());
  if (amount != pickle.size()) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

class SaveHistogramEvent : public nsRunnable
{
public:
  SaveHistogramEvent(nsIFile *file, const nsACString &uuid,
                     nsITelemetrySaveSessionDataCallback *callback)
    : mFile(file), mUUID(uuid), mCallback(callback)
  {}

  NS_IMETHOD Run()
  {
    nsresult rv = TelemetrySessionData::SaveToDisk(mFile, mUUID);
    mCallback->Handle(!!NS_SUCCEEDED(rv));
    return rv;
  }

private:
  nsCOMPtr<nsIFile> mFile;
  nsCString mUUID;
  nsCOMPtr<nsITelemetrySaveSessionDataCallback> mCallback;
};

NS_IMETHODIMP
TelemetryImpl::SaveHistograms(nsIFile *file, const nsACString &uuid,
                              nsITelemetrySaveSessionDataCallback *callback,
                              bool isSynchronous)
{
  nsCOMPtr<nsIRunnable> event = new SaveHistogramEvent(file, uuid, callback);
  if (isSynchronous) {
    return event ? event->Run() : NS_ERROR_FAILURE;
  } else {
    return NS_DispatchToCurrentThread(event);
  }
}

class LoadHistogramEvent : public nsRunnable
{
public:
  LoadHistogramEvent(nsIFile *file,
                     nsITelemetryLoadSessionDataCallback *callback)
    : mFile(file), mCallback(callback)
  {}

  NS_IMETHOD Run()
  {
    TelemetrySessionData *sessionData = nsnull;
    nsresult rv = TelemetrySessionData::LoadFromDisk(mFile, &sessionData);
    if (NS_FAILED(rv)) {
      mCallback->Handle(nsnull);
    } else {
      nsCOMPtr<nsITelemetrySessionData> data(sessionData);
      mCallback->Handle(data);
    }
    return rv;
  }

private:
  nsCOMPtr<nsIFile> mFile;
  nsCOMPtr<nsITelemetryLoadSessionDataCallback> mCallback;
};

NS_IMETHODIMP
TelemetryImpl::LoadHistograms(nsIFile *file,
                              nsITelemetryLoadSessionDataCallback *callback,
                              bool isSynchronous)
{
  nsCOMPtr<nsIRunnable> event = new LoadHistogramEvent(file, callback);
  if (isSynchronous) {
    return event ? event->Run() : NS_ERROR_FAILURE;
  } else {
    return NS_DispatchToCurrentThread(event);
  }
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
TelemetryImpl::RecordSlowStatement(const nsACString &statement,
                                   const nsACString &dbName,
                                   PRUint32 delay)
{
  MOZ_ASSERT(sTelemetry);
  if (!sTelemetry->mCanRecord || !sTelemetry->mTrackedDBs.GetEntry(dbName))
    return;

  AutoHashtable<SlowSQLEntryType> *slowSQLMap = NULL;
  if (NS_IsMainThread())
    slowSQLMap = &(sTelemetry->mSlowSQLOnMainThread);
  else
    slowSQLMap = &(sTelemetry->mSlowSQLOnOtherThread);

  MutexAutoLock hashMutex(sTelemetry->mHashMutex);
  SlowSQLEntryType *entry = slowSQLMap->GetEntry(statement);
  if (!entry) {
    entry = slowSQLMap->PutEntry(statement);
    if (NS_UNLIKELY(!entry))
      return;
    entry->mData.hitCount = 0;
    entry->mData.totalTime = 0;
  }
  entry->mData.hitCount++;
  entry->mData.totalTime += delay;
}

#if defined(MOZ_ENABLE_PROFILER_SPS)
void
TelemetryImpl::RecordChromeHang(PRUint32 duration,
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
Accumulate(ID aHistogram, PRUint32 aSample)
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
             static_cast<PRUint32>((end - start).ToMilliseconds()));
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
                       PRUint32 delay)
{
  TelemetryImpl::RecordSlowStatement(statement, dbName, delay);
}

void Init()
{
  // Make the service manager hold a long-lived reference to the service
  nsCOMPtr<nsITelemetry> telemetryService =
    do_GetService("@mozilla.org/base/telemetry;1");
  MOZ_ASSERT(telemetryService);
}

#if defined(MOZ_ENABLE_PROFILER_SPS)
void RecordChromeHang(PRUint32 duration,
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
XRE_TelemetryAccumulate(int aID, PRUint32 aSample)
{
  mozilla::Telemetry::Accumulate((mozilla::Telemetry::ID) aID, aSample);
}
