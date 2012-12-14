/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Likely.h"

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
#include "nsIMemoryReporter.h"
#include "Telemetry.h" 
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsBaseHashtable.h"
#include "nsXULAppAPI.h"
#include "nsThreadUtils.h"
#include "nsNetCID.h"
#include "plstr.h"
#include "nsAppDirectoryServiceDefs.h"
#include "mozilla/ProcessedStack.h"
#include "mozilla/Mutex.h"
#include "mozilla/FileUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/mozPoisonWrite.h"

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

// This class is conceptually a list of ProcessedStack objects, but it represents them
// more efficiently by keeping a single global list of modules.
class CombinedStacks {
public:
  typedef std::vector<Telemetry::ProcessedStack::Frame> Stack;
  const Telemetry::ProcessedStack::Module& GetModule(unsigned aIndex) const;
  size_t GetModuleCount() const;
  const Stack& GetStack(unsigned aIndex) const;
  void AddStack(const Telemetry::ProcessedStack& aStack);
  size_t GetStackCount() const;
  size_t SizeOfExcludingThis() const;
private:
  std::vector<Telemetry::ProcessedStack::Module> mModules;
  std::vector<Stack> mStacks;
};

static JSObject *
CreateJSStackObject(JSContext *cx, const CombinedStacks &stacks);

size_t
CombinedStacks::GetModuleCount() const {
  return mModules.size();
}

const Telemetry::ProcessedStack::Module&
CombinedStacks::GetModule(unsigned aIndex) const {
  return mModules[aIndex];
}

void
CombinedStacks::AddStack(const Telemetry::ProcessedStack& aStack) {
  mStacks.resize(mStacks.size() + 1);
  CombinedStacks::Stack& adjustedStack = mStacks.back();

  size_t stackSize = aStack.GetStackSize();
  for (int i = 0; i < stackSize; ++i) {
    const Telemetry::ProcessedStack::Frame& frame = aStack.GetFrame(i);
    uint16_t modIndex;
    if (frame.mModIndex == std::numeric_limits<uint16_t>::max()) {
      modIndex = frame.mModIndex;
    } else {
      const Telemetry::ProcessedStack::Module& module =
        aStack.GetModule(frame.mModIndex);
      std::vector<Telemetry::ProcessedStack::Module>::iterator modIterator =
        std::find(mModules.begin(), mModules.end(), module);
      if (modIterator == mModules.end()) {
        mModules.push_back(module);
        modIndex = mModules.size() - 1;
      } else {
        modIndex = modIterator - mModules.begin();
      }
    }
    Telemetry::ProcessedStack::Frame adjustedFrame = { frame.mOffset, modIndex };
    adjustedStack.push_back(adjustedFrame);
  }
}

const CombinedStacks::Stack&
CombinedStacks::GetStack(unsigned aIndex) const {
  return mStacks[aIndex];
}

size_t
CombinedStacks::GetStackCount() const {
  return mStacks.size();
}

size_t
CombinedStacks::SizeOfExcludingThis() const {
  // This is a crude approximation. We would like to do something like
  // aMallocSizeOf(&mModules[0]), but on linux aMallocSizeOf will call
  // malloc_usable_size which is only safe on the pointers returned by malloc.
  // While it works on current libstdc++, it is better to be safe and not assume
  // that &vec[0] points to one. We could use a custom allocator, but
  // it doesn't seem worth it.
  size_t n = 0;
  n += mModules.capacity() * sizeof(Telemetry::ProcessedStack::Module);
  n += mStacks.capacity() * sizeof(Stack);
  for (std::vector<Stack>::const_iterator i = mStacks.begin(),
         e = mStacks.end(); i != e; ++i) {
    const Stack& s = *i;
    n += s.capacity() * sizeof(Telemetry::ProcessedStack::Frame);
  }
  return n;
}

class HangReports {
public:
  size_t SizeOfExcludingThis() const;
  void AddHang(const Telemetry::ProcessedStack& aStack, uint32_t aDuration);
  uint32_t GetDuration(unsigned aIndex) const;
  const CombinedStacks& GetStacks() const;
private:
  CombinedStacks mStacks;
  std::vector<uint32_t> mDurations;
};

void
HangReports::AddHang(const Telemetry::ProcessedStack& aStack, uint32_t aDuration) {
  mStacks.AddStack(aStack);
  mDurations.push_back(aDuration);
}

size_t
HangReports::SizeOfExcludingThis() const {
  size_t n = 0;
  n += mStacks.SizeOfExcludingThis();
  // This is a crude approximation. See comment on
  // CombinedStacks::SizeOfExcludingThis.
  n += mDurations.capacity() * sizeof(uint32_t);
  return n;
}

const CombinedStacks&
HangReports::GetStacks() const {
  return mStacks;
}

uint32_t
HangReports::GetDuration(unsigned aIndex) const {
  return mDurations[aIndex];
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
                                  uint32_t delay);
#if defined(MOZ_ENABLE_PROFILER_SPS)
  static void RecordChromeHang(uint32_t duration,
                               Telemetry::ProcessedStack &aStack);
#endif
  static nsresult GetHistogramEnumId(const char *name, Telemetry::ID *id);
  static int64_t GetTelemetryMemoryUsed();
  size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf);
  struct Stat {
    uint32_t hitCount;
    uint32_t totalTime;
  };
  struct StmtStats {
    struct Stat mainThread;
    struct Stat otherThreads;
  };
  typedef nsBaseHashtableET<nsCStringHashKey, StmtStats> SlowSQLEntryType;

private:
  // We don't need to poke inside any of our hashtables for more
  // information, so we just have One Function To Size Them All.
  template<typename EntryType>
  struct impl {
    static size_t SizeOfEntryExcludingThis(EntryType *,
                                           nsMallocSizeOfFun,
                                           void *) {
      return 0;
    };
  };

  static nsCString SanitizeSQL(const nsACString& sql);

  enum SanitizedState { Sanitized, Unsanitized };

  static void StoreSlowSQL(const nsACString &offender, uint32_t delay,
                           SanitizedState state);

  static bool ReflectMainThreadSQL(SlowSQLEntryType *entry, JSContext *cx,
                                   JSObject *obj);
  static bool ReflectOtherThreadsSQL(SlowSQLEntryType *entry, JSContext *cx,
                                     JSObject *obj);
  static bool ReflectSQL(const SlowSQLEntryType *entry, const Stat *stat,
                         JSContext *cx, JSObject *obj);

  bool AddSQLInfo(JSContext *cx, JSObject *rootObj, bool mainThread,
                  bool privateSQL);
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
  AutoHashtable<SlowSQLEntryType> mPrivateSQL;
  AutoHashtable<SlowSQLEntryType> mSanitizedSQL;
  // This gets marked immutable in debug builds, so we can't use
  // AutoHashtable here.
  nsTHashtable<nsCStringHashKey> mTrackedDBs;
  Mutex mHashMutex;
  HangReports mHangReports;
  Mutex mHangReportsMutex;
  nsIMemoryReporter *mMemoryReporter;

  bool mCachedShutdownTime;
  uint32_t mLastShutdownTime;
  std::vector<nsCOMPtr<nsIReadShutdownTimeCallback> > mCallbacks;
  friend class nsReadShutdownTime;
};

TelemetryImpl*  TelemetryImpl::sTelemetry = NULL;

NS_MEMORY_REPORTER_MALLOC_SIZEOF_FUN(TelemetryMallocSizeOf, "telemetry")

size_t
TelemetryImpl::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf)
{
  size_t n = 0;
  n += aMallocSizeOf(this);
  // Ignore the hashtables in mAddonMap; they are not significant.
  n += mAddonMap.SizeOfExcludingThis(impl<AddonEntryType>::SizeOfEntryExcludingThis,
                                     aMallocSizeOf);
  n += mHistogramMap.SizeOfExcludingThis(impl<CharPtrEntryType>::SizeOfEntryExcludingThis,
                                         aMallocSizeOf);
  n += mPrivateSQL.SizeOfExcludingThis(impl<SlowSQLEntryType>::SizeOfEntryExcludingThis,
                                       aMallocSizeOf);
  n += mSanitizedSQL.SizeOfExcludingThis(impl<SlowSQLEntryType>::SizeOfEntryExcludingThis,
                                         aMallocSizeOf);
  n += mTrackedDBs.SizeOfExcludingThis(impl<nsCStringHashKey>::SizeOfEntryExcludingThis,
                                       aMallocSizeOf);
  n += mHangReports.SizeOfExcludingThis();
  return n;
}

int64_t
TelemetryImpl::GetTelemetryMemoryUsed()
{
  int64_t n = 0;
  if (sTelemetry) {
    n += sTelemetry->SizeOfIncludingThis(TelemetryMallocSizeOf);
  }

  StatisticsRecorder::Histograms hs;
  StatisticsRecorder::GetHistograms(&hs);

  for (HistogramIterator it = hs.begin(); it != hs.end(); ++it) {
    Histogram *h = *it;
    n += h->SizeOfIncludingThis(TelemetryMallocSizeOf);
  }
  return n;
}

NS_MEMORY_REPORTER_IMPLEMENT(Telemetry,
  "explicit/telemetry",
  KIND_HEAP,
  UNITS_BYTES,
  TelemetryImpl::GetTelemetryMemoryUsed,
  "Memory used by the telemetry system.")

// A initializer to initialize histogram collection
StatisticsRecorder gStatisticsRecorder;

// Hardcoded probes
struct TelemetryHistogram {
  uint32_t min;
  uint32_t max;
  uint32_t bucketCount;
  uint32_t histogramType;
  uint16_t id_offset;
  uint16_t comment_offset;

  const char *id() const;
  const char *comment() const;
};

#include "TelemetryHistogramData.inc"
bool gCorruptHistograms[Telemetry::HistogramCount];

const char *
TelemetryHistogram::id() const
{
  return &gHistogramStringTable[this->id_offset];
}

const char *
TelemetryHistogram::comment() const
{
  return &gHistogramStringTable[this->comment_offset];
}

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
  nsresult rv = HistogramGet(p.id(), p.min, p.max, p.bucketCount, p.histogramType, &h);
  if (NS_FAILED(rv))
    return rv;

#ifdef DEBUG
  // Check that the C++ Histogram code computes the same ranges as the
  // Python histogram code.
  const struct bounds &b = gBucketLowerBoundIndex[id];
  if (b.length != 0) {
    MOZ_ASSERT(size_t(b.length) == h->bucket_count(),
               "C++/Python bucket # mismatch");
    for (int i = 0; i < b.length; ++i) {
      MOZ_ASSERT(gBucketLowerBounds[b.offset + i] == h->ranges(i),
                 "C++/Python bucket mismatch");
    }
  }
#endif

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
        && JS_DefineProperty(cx, obj, "sum", DOUBLE_TO_JSVAL(ss.sum()), NULL, NULL, JSPROP_ENUMERATE)
        && JS_DefineProperty(cx, obj, "log_sum", DOUBLE_TO_JSVAL(ss.log_sum()), NULL, NULL, JSPROP_ENUMERATE)
        && JS_DefineProperty(cx, obj, "log_sum_squares", DOUBLE_TO_JSVAL(ss.log_sum_squares()), NULL, NULL, JSPROP_ENUMERATE))) {
    return REFLECT_FAILURE;
  }

  // Export |sum_squares| as two separate 32-bit properties so that we
  // can accurately reconstruct it on the analysis side.
  uint64_t sum_squares = ss.sum_squares();
  uint32_t lo = sum_squares;
  uint32_t hi = sum_squares >> 32;
  if (!(JS_DefineProperty(cx, obj, "sum_squares_lo", INT_TO_JSVAL(lo), NULL, NULL, JSPROP_ENUMERATE)
        && JS_DefineProperty(cx, obj, "sum_squares_hi", INT_TO_JSVAL(hi), NULL, NULL, JSPROP_ENUMERATE))) {
    return REFLECT_FAILURE;
  }

  const size_t count = h->bucket_count();
  JSObject *rarray = JS_NewArrayObject(cx, count, nullptr);
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
  JSObject *snapshot = JS_NewObject(cx, nullptr, nullptr, nullptr);
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

static uint32_t
ReadLastShutdownDuration(const char *filename) {
  FILE *f = fopen(filename, "r");
  if (!f) {
    return 0;
  }

  int shutdownTime;
  int r = fscanf(f, "%d\n", &shutdownTime);
  fclose(f);
  if (r != 1) {
    return 0;
  }

  return shutdownTime;
}

class nsReadShutdownTime : public nsRunnable
{
public:
  nsReadShutdownTime(const char *aFilename) :
    mFilename(aFilename), mTelemetry(TelemetryImpl::sTelemetry) {
  }

private:
  const char *mFilename;
  nsCOMPtr<TelemetryImpl> mTelemetry;

public:
  void MainThread() {
    mTelemetry->mCachedShutdownTime = true;
    for (unsigned int i = 0, n = mTelemetry->mCallbacks.size(); i < n; ++i) {
      mTelemetry->mCallbacks[i]->Complete();
    }
    mTelemetry->mCallbacks.clear();
  }

  NS_IMETHOD Run() {
    mTelemetry->mLastShutdownTime = ReadLastShutdownDuration(mFilename);
    nsCOMPtr<nsIRunnable> e =
      NS_NewRunnableMethod(this, &nsReadShutdownTime::MainThread);
    NS_ENSURE_STATE(e);
    NS_DispatchToMainThread(e, NS_DISPATCH_NORMAL);
    return NS_OK;
  }
};

static TimeStamp gRecordedShutdownStartTime;
static bool gAlreadyFreedShutdownTimeFileName = false;
static char *gRecordedShutdownTimeFileName = nullptr;

static char *
GetShutdownTimeFileName()
{
  if (gAlreadyFreedShutdownTimeFileName) {
    return nullptr;
  }

  if (!gRecordedShutdownTimeFileName) {
    nsCOMPtr<nsIFile> mozFile;
    NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(mozFile));
    if (!mozFile)
      return nullptr;

    mozFile->AppendNative(NS_LITERAL_CSTRING("Telemetry.ShutdownTime.txt"));
    nsAutoCString nativePath;
    nsresult rv = mozFile->GetNativePath(nativePath);
    if (!NS_SUCCEEDED(rv))
      return nullptr;

    gRecordedShutdownTimeFileName = PL_strdup(nativePath.get());
  }

  return gRecordedShutdownTimeFileName;
}

NS_IMETHODIMP
TelemetryImpl::GetLastShutdownDuration(uint32_t *aResult)
{
  // The user must call ReadShutdownTime first. We return zero instead of
  // reporting a failure so that the rest of telemetry can uniformly handle
  // the read not being available yet.
  if (!mCachedShutdownTime) {
    *aResult = 0;
    return NS_OK;
  }

  *aResult = mLastShutdownTime;
  return NS_OK;
}

NS_IMETHODIMP
TelemetryImpl::AsyncReadShutdownTime(nsIReadShutdownTimeCallback *aCallback)
{
  // We have finished reading the data already, just call the callback.
  if (mCachedShutdownTime) {
    aCallback->Complete();
    return NS_OK;
  }

  // We already have a read request running, just remember the callback.
  if (!mCallbacks.empty()) {
    mCallbacks.push_back(aCallback);
    return NS_OK;
  }

  // We make this check so that GetShutdownTimeFileName() doesn't get
  // called; calling that function without telemetry enabled violates
  // assumptions that the write-the-shutdown-timestamp machinery makes.
  if (!Telemetry::CanRecord()) {
    mCachedShutdownTime = true;
    aCallback->Complete();
    return NS_OK;
  }

  // Send the read to a background thread provided by the stream transport
  // service to avoid a read in the main thread.
  nsCOMPtr<nsIEventTarget> targetThread =
    do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  if (!targetThread) {
    mCachedShutdownTime = true;
    aCallback->Complete();
    return NS_OK;
  }

  // We have to get the filename from the main thread.
  const char *filename = GetShutdownTimeFileName();
  if (!filename) {
    mCachedShutdownTime = true;
    aCallback->Complete();
    return NS_OK;
  }

  mCallbacks.push_back(aCallback);
  nsCOMPtr<nsIRunnable> event = new nsReadShutdownTime(filename);

  targetThread->Dispatch(event, NS_DISPATCH_NORMAL);
  return NS_OK;
}

TelemetryImpl::TelemetryImpl():
mHistogramMap(Telemetry::HistogramCount),
mCanRecord(XRE_GetProcessType() == GeckoProcessType_Default),
mHashMutex("Telemetry::mHashMutex"),
mHangReportsMutex("Telemetry::mHangReportsMutex"),
mCachedShutdownTime(false),
mLastShutdownTime(0)
{
  // A whitelist to prevent Telemetry reporting on Addon & Thunderbird DBs
  const char *trackedDBs[] = {
    "addons.sqlite", "content-prefs.sqlite",
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
  mMemoryReporter = new NS_MEMORY_REPORTER_NAME(Telemetry);
  NS_RegisterMemoryReporter(mMemoryReporter);
}

TelemetryImpl::~TelemetryImpl() {
  NS_UnregisterMemoryReporter(mMemoryReporter);
  mMemoryReporter = nullptr;
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
TelemetryImpl::ReflectSQL(const SlowSQLEntryType *entry,
                          const Stat *stat,
                          JSContext *cx,
                          JSObject *obj)
{
  if (stat->hitCount == 0)
    return true;

  const nsACString &sql = entry->GetKey();
  jsval hitCount = UINT_TO_JSVAL(stat->hitCount);
  jsval totalTime = UINT_TO_JSVAL(stat->totalTime);

  JSObject *arrayObj = JS_NewArrayObject(cx, 0, nullptr);
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
TelemetryImpl::ReflectMainThreadSQL(SlowSQLEntryType *entry, JSContext *cx,
                                    JSObject *obj)
{
  return ReflectSQL(entry, &entry->mData.mainThread, cx, obj);
}

bool
TelemetryImpl::ReflectOtherThreadsSQL(SlowSQLEntryType *entry, JSContext *cx,
                                      JSObject *obj)
{
  return ReflectSQL(entry, &entry->mData.otherThreads, cx, obj);
}

bool
TelemetryImpl::AddSQLInfo(JSContext *cx, JSObject *rootObj, bool mainThread,
                          bool privateSQL)
{
  JSObject *statsObj = JS_NewObject(cx, NULL, NULL, NULL);
  if (!statsObj)
    return false;
  JS::AutoObjectRooter root(cx, statsObj);

  AutoHashtable<SlowSQLEntryType> &sqlMap =
    (privateSQL ? mPrivateSQL : mSanitizedSQL);
  AutoHashtable<SlowSQLEntryType>::ReflectEntryFunc reflectFunction =
    (mainThread ? ReflectMainThreadSQL : ReflectOtherThreadsSQL);
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
      CharPtrEntryType *entry = map->PutEntry(gHistograms[i].id());
      if (MOZ_UNLIKELY(!entry)) {
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
    nsAutoCString actualName;
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

  const CombinedStacks& stacks = mHangReports.GetStacks();
  JSObject *fullReportObj = CreateJSStackObject(cx, stacks);
  if (!fullReportObj) {
    return NS_ERROR_FAILURE;
  }

  *ret = OBJECT_TO_JSVAL(fullReportObj);

  JSObject *durationArray = JS_NewArrayObject(cx, 0, nullptr);
  if (!durationArray) {
    return NS_ERROR_FAILURE;
  }
  JSBool ok = JS_DefineProperty(cx, fullReportObj, "durations",
                                OBJECT_TO_JSVAL(durationArray),
                                NULL, NULL, JSPROP_ENUMERATE);
  if (!ok) {
    return NS_ERROR_FAILURE;
  }

  const size_t length = stacks.GetStackCount();
  for (size_t i = 0; i < length; ++i) {
    jsval duration = INT_TO_JSVAL(mHangReports.GetDuration(i));
    if (!JS_SetElement(cx, durationArray, i, &duration)) {
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

static JSObject *
CreateJSStackObject(JSContext *cx, const CombinedStacks &stacks) {
  JSObject *ret = JS_NewObject(cx, nullptr, nullptr, nullptr);
  if (!ret) {
    return nullptr;
  }

  JSObject *moduleArray = JS_NewArrayObject(cx, 0, nullptr);
  if (!moduleArray) {
    return nullptr;
  }
  JSBool ok = JS_DefineProperty(cx, ret, "memoryMap",
                                OBJECT_TO_JSVAL(moduleArray),
                                NULL, NULL, JSPROP_ENUMERATE);
  if (!ok) {
    return nullptr;
  }

  const size_t moduleCount = stacks.GetModuleCount();
  for (size_t moduleIndex = 0; moduleIndex < moduleCount; ++moduleIndex) {
    // Current module
    const Telemetry::ProcessedStack::Module& module =
      stacks.GetModule(moduleIndex);

    JSObject *moduleInfoArray = JS_NewArrayObject(cx, 0, nullptr);
    if (!moduleInfoArray) {
      return nullptr;
    }
    jsval val = OBJECT_TO_JSVAL(moduleInfoArray);
    if (!JS_SetElement(cx, moduleArray, moduleIndex, &val)) {
      return nullptr;
    }

    unsigned index = 0;

    // Module name
    JSString *str = JS_NewStringCopyZ(cx, module.mName.c_str());
    if (!str) {
      return nullptr;
    }
    val = STRING_TO_JSVAL(str);
    if (!JS_SetElement(cx, moduleInfoArray, index++, &val)) {
      return nullptr;
    }

    // "PDB Age" identifier
    val = INT_TO_JSVAL(module.mPdbAge);
    if (!JS_SetElement(cx, moduleInfoArray, index++, &val)) {
      return nullptr;
    }

    // "PDB Signature" GUID
    str = JS_NewStringCopyZ(cx, module.mPdbSignature.c_str());
    if (!str) {
      return nullptr;
    }
    val = STRING_TO_JSVAL(str);
    if (!JS_SetElement(cx, moduleInfoArray, index++, &val)) {
      return nullptr;
    }

    // Name of associated PDB file
    str = JS_NewStringCopyZ(cx, module.mPdbName.c_str());
    if (!str) {
      return nullptr;
    }
    val = STRING_TO_JSVAL(str);
    if (!JS_SetElement(cx, moduleInfoArray, index++, &val)) {
      return nullptr;
    }
  }

  JSObject *reportArray = JS_NewArrayObject(cx, 0, nullptr);
  if (!reportArray) {
    return nullptr;
  }
  ok = JS_DefineProperty(cx, ret, "stacks",
                         OBJECT_TO_JSVAL(reportArray),
                         NULL, NULL, JSPROP_ENUMERATE);
  if (!ok) {
    return nullptr;
  }

  const size_t length = stacks.GetStackCount();
  for (size_t i = 0; i < length; ++i) {
    // Represent call stack PCs as (module index, offset) pairs.
    JSObject *pcArray = JS_NewArrayObject(cx, 0, nullptr);
    if (!pcArray) {
      return nullptr;
    }

    jsval pcArrayVal = OBJECT_TO_JSVAL(pcArray);
    if (!JS_SetElement(cx, reportArray, i, &pcArrayVal)) {
      return nullptr;
    }

    const CombinedStacks::Stack& stack = stacks.GetStack(i);
    const uint32_t pcCount = stack.size();
    for (size_t pcIndex = 0; pcIndex < pcCount; ++pcIndex) {
      const Telemetry::ProcessedStack::Frame& frame = stack[pcIndex];
      JSObject *framePair = JS_NewArrayObject(cx, 0, nullptr);
      if (!framePair) {
        return nullptr;
      }
      int modIndex = (std::numeric_limits<uint16_t>::max() == frame.mModIndex) ?
        -1 : frame.mModIndex;
      jsval modIndexVal = INT_TO_JSVAL(modIndex);
      if (!JS_SetElement(cx, framePair, 0, &modIndexVal)) {
        return nullptr;
      }
      jsval mOffsetVal = INT_TO_JSVAL(frame.mOffset);
      if (!JS_SetElement(cx, framePair, 1, &mOffsetVal)) {
        return nullptr;
      }
      jsval framePairVal = OBJECT_TO_JSVAL(framePair);
      if (!JS_SetElement(cx, pcArray, pcIndex, &framePairVal)) {
        return nullptr;
      }
    }
  }

  return ret;
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
    JSString *comment = JS_InternString(cx, gHistograms[i].comment());
    
    if (!(comment
          && JS_DefineProperty(cx, info, gHistograms[i].id(),
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
                            SanitizedState state)
{
  AutoHashtable<SlowSQLEntryType> *slowSQLMap = NULL;
  if (state == Sanitized)
    slowSQLMap = &(sTelemetry->mSanitizedSQL);
  else
    slowSQLMap = &(sTelemetry->mPrivateSQL);

  MutexAutoLock hashMutex(sTelemetry->mHashMutex);

  SlowSQLEntryType *entry = slowSQLMap->GetEntry(sql);
  if (!entry) {
    entry = slowSQLMap->PutEntry(sql);
    if (MOZ_UNLIKELY(!entry))
      return;
    entry->mData.mainThread.hitCount = 0;
    entry->mData.mainThread.totalTime = 0;
    entry->mData.otherThreads.hitCount = 0;
    entry->mData.otherThreads.totalTime = 0;
  }

  if (NS_IsMainThread()) {
    entry->mData.mainThread.hitCount++;
    entry->mData.mainThread.totalTime += delay;
  } else {
    entry->mData.otherThreads.hitCount++;
    entry->mData.otherThreads.totalTime += delay;
  }
}

/**
 * This method replaces string literals in SQL strings with the word :private
 *
 * States used in this state machine:
 *
 * NORMAL:
 *  - This is the active state when not iterating over a string literal or
 *  comment
 *
 * SINGLE_QUOTE:
 *  - Defined here: http://www.sqlite.org/lang_expr.html
 *  - This state represents iterating over a string literal opened with
 *  a single quote.
 *  - A single quote within the string can be encoded by putting 2 single quotes
 *  in a row, e.g. 'This literal contains an escaped quote '''
 *  - Any double quotes found within a single-quoted literal are ignored
 *  - This state covers BLOB literals, e.g. X'ABC123'
 *  - The string literal and the enclosing quotes will be replaced with
 *  the text :private
 *
 * DOUBLE_QUOTE:
 *  - Same rules as the SINGLE_QUOTE state.
 *  - According to http://www.sqlite.org/lang_keywords.html,
 *  SQLite interprets text in double quotes as an identifier unless it's used in
 *  a context where it cannot be resolved to an identifier and a string literal
 *  is allowed. This method removes text in double-quotes for safety.
 *
 *  DASH_COMMENT:
 *  - http://www.sqlite.org/lang_comment.html
 *  - A dash comment starts with two dashes in a row,
 *  e.g. DROP TABLE foo -- a comment
 *  - Any text following two dashes in a row is interpreted as a comment until
 *  end of input or a newline character
 *  - Any quotes found within the comment are ignored and no replacements made
 *
 *  C_STYLE_COMMENT:
 *  - http://www.sqlite.org/lang_comment.html
 *  - A C-style comment starts with a forward slash and an asterisk, and ends
 *  with an asterisk and a forward slash
 *  - Any text following comment start is interpreted as a comment up to end of
 *  input or comment end
 *  - Any quotes found within the comment are ignored and no replacements made
 */
nsCString
TelemetryImpl::SanitizeSQL(const nsACString &sql) {
  nsCString output;
  int length = sql.Length();

  typedef enum {
    NORMAL,
    SINGLE_QUOTE,
    DOUBLE_QUOTE,
    DASH_COMMENT,
    C_STYLE_COMMENT,
  } State;

  State state = NORMAL;
  int fragmentStart = 0;
  for (int i = 0; i < length; i++) {
    char character = sql[i];
    char nextCharacter = (i + 1 < length) ? sql[i + 1] : '\0';

    switch (character) {
      case '\'':
      case '"':
        if (state == NORMAL) {
          state = (character == '\'') ? SINGLE_QUOTE : DOUBLE_QUOTE;
          output += nsDependentCSubstring(sql, fragmentStart, i - fragmentStart);
          output += ":private";
          fragmentStart = -1;
        } else if ((state == SINGLE_QUOTE && character == '\'') ||
                   (state == DOUBLE_QUOTE && character == '"')) {
          if (nextCharacter == character) {
            // Two consecutive quotes within a string literal are a single escaped quote
            i++;
          } else {
            state = NORMAL;
            fragmentStart = i + 1;
          }
        }
        break;
      case '-':
        if (state == NORMAL) {
          if (nextCharacter == '-') {
            state = DASH_COMMENT;
            i++;
          }
        }
        break;
      case '\n':
        if (state == DASH_COMMENT) {
          state = NORMAL;
        }
        break;
      case '/':
        if (state == NORMAL) {
          if (nextCharacter == '*') {
            state = C_STYLE_COMMENT;
            i++;
          }
        }
        break;
      case '*':
        if (state == C_STYLE_COMMENT) {
          if (nextCharacter == '/') {
            state = NORMAL;
          }
        }
        break;
      default:
        continue;
    }
  }

  if ((fragmentStart >= 0) && fragmentStart < length)
    output += nsDependentCSubstring(sql, fragmentStart, length - fragmentStart);

  return output;
}

void
TelemetryImpl::RecordSlowStatement(const nsACString &sql,
                                   const nsACString &dbName,
                                   uint32_t delay)
{
  if (!sTelemetry || !sTelemetry->mCanRecord)
    return;

  nsAutoCString fullSQL(sql);
  fullSQL.AppendPrintf(" /* %s */", dbName.BeginReading());

  bool isFirefoxDB = sTelemetry->mTrackedDBs.Contains(dbName);
  if (isFirefoxDB) {
    nsAutoCString sanitizedSQL(SanitizeSQL(fullSQL));
    StoreSlowSQL(sanitizedSQL, delay, Sanitized);
  } else {
    // Report aggregate DB-level statistics for addon DBs
    nsAutoCString aggregate;
    aggregate.AppendPrintf("Untracked SQL for %s", dbName.BeginReading());
    StoreSlowSQL(aggregate, delay, Sanitized);
  }

  StoreSlowSQL(fullSQL, delay, Unsanitized);
}

#if defined(MOZ_ENABLE_PROFILER_SPS)
void
TelemetryImpl::RecordChromeHang(uint32_t duration,
                                Telemetry::ProcessedStack &aStack)
{
  if (!sTelemetry || !sTelemetry->mCanRecord)
    return;

  MutexAutoLock hangReportMutex(sTelemetry->mHangReportsMutex);

  sTelemetry->mHangReports.AddHang(aStack, duration);
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
void
RecordShutdownStartTimeStamp() {
  if (!Telemetry::CanRecord())
    return;

  gRecordedShutdownStartTime = TimeStamp::Now();

  GetShutdownTimeFileName();
}

void
RecordShutdownEndTimeStamp() {
  if (!gRecordedShutdownTimeFileName || gAlreadyFreedShutdownTimeFileName)
    return;

  nsCString name(gRecordedShutdownTimeFileName);
  PL_strfree(gRecordedShutdownTimeFileName);
  gRecordedShutdownTimeFileName = nullptr;
  gAlreadyFreedShutdownTimeFileName = true;

  nsCString tmpName = name;
  tmpName += ".tmp";
  FILE *f = fopen(tmpName.get(), "w");
  if (!f)
    return;
  // On a normal release build this should be called just before
  // calling _exit, but on a debug build or when the user forces a full
  // shutdown this is called as late as possible, so we have to
  // white list this write as write poisoning will be enabled.
  int fd = fileno(f);
  MozillaRegisterDebugFD(fd);

  TimeStamp now = TimeStamp::Now();
  MOZ_ASSERT(now >= gRecordedShutdownStartTime);
  TimeDuration diff = now - gRecordedShutdownStartTime;
  uint32_t diff2 = diff.ToMilliseconds();
  int written = fprintf(f, "%d\n", diff2);
  MozillaUnRegisterDebugFILE(f);
  int rv = fclose(f);
  if (written < 0 || rv != 0) {
    PR_Delete(tmpName.get());
    return;
  }
  PR_Delete(name.get());
  PR_Rename(tmpName.get(), name.get());
}

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
                       uint32_t delay)
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
void RecordChromeHang(uint32_t duration,
                      ProcessedStack &aStack)
{
  TelemetryImpl::RecordChromeHang(duration, aStack);
}
#endif

ProcessedStack::ProcessedStack()
{
}

size_t ProcessedStack::GetStackSize() const
{
  return mStack.size();
}

const ProcessedStack::Frame &ProcessedStack::GetFrame(unsigned aIndex) const
{
  MOZ_ASSERT(aIndex < mStack.size());
  return mStack[aIndex];
}

void ProcessedStack::AddFrame(const Frame &aFrame)
{
  mStack.push_back(aFrame);
}

size_t ProcessedStack::GetNumModules() const
{
  return mModules.size();
}

const ProcessedStack::Module &ProcessedStack::GetModule(unsigned aIndex) const
{
  MOZ_ASSERT(aIndex < mModules.size());
  return mModules[aIndex];
}

void ProcessedStack::AddModule(const Module &aModule)
{
  mModules.push_back(aModule);
}

void ProcessedStack::Clear() {
  mModules.clear();
  mStack.clear();
}

bool ProcessedStack::Module::operator==(const Module& aOther) const {
  return  mName == aOther.mName &&
    mPdbAge == aOther.mPdbAge &&
    mPdbSignature == aOther.mPdbSignature &&
    mPdbName == aOther.mPdbName;
}

struct StackFrame
{
  uintptr_t mPC;      // The program counter at this position in the call stack.
  uint16_t mIndex;    // The number of this frame in the call stack.
  uint16_t mModIndex; // The index of module that has this program counter.
};


#ifdef MOZ_ENABLE_PROFILER_SPS
static bool CompareByPC(const StackFrame &a, const StackFrame &b)
{
  return a.mPC < b.mPC;
}

static bool CompareByIndex(const StackFrame &a, const StackFrame &b)
{
  return a.mIndex < b.mIndex;
}
#endif

ProcessedStack
GetStackAndModules(const std::vector<uintptr_t>& aPCs)
{
  std::vector<StackFrame> rawStack;
  for (std::vector<uintptr_t>::const_iterator i = aPCs.begin(),
         e = aPCs.end(); i != e; ++i) {
    uintptr_t aPC = *i;
    StackFrame Frame = {aPC, static_cast<uint16_t>(rawStack.size()),
                        std::numeric_limits<uint16_t>::max()};
    rawStack.push_back(Frame);
  }

#ifdef MOZ_ENABLE_PROFILER_SPS
  // Remove all modules not referenced by a PC on the stack
  std::sort(rawStack.begin(), rawStack.end(), CompareByPC);

  size_t moduleIndex = 0;
  size_t stackIndex = 0;
  size_t stackSize = rawStack.size();

  SharedLibraryInfo rawModules = SharedLibraryInfo::GetInfoForSelf();
  rawModules.SortByAddress();

  while (moduleIndex < rawModules.GetSize()) {
    const SharedLibrary& module = rawModules.GetEntry(moduleIndex);
    uintptr_t moduleStart = module.GetStart();
    uintptr_t moduleEnd = module.GetEnd() - 1;
    // the interval is [moduleStart, moduleEnd)

    bool moduleReferenced = false;
    for (;stackIndex < stackSize; ++stackIndex) {
      uintptr_t pc = rawStack[stackIndex].mPC;
      if (pc >= moduleEnd)
        break;

      if (pc >= moduleStart) {
        // If the current PC is within the current module, mark
        // module as used
        moduleReferenced = true;
        rawStack[stackIndex].mPC -= moduleStart;
        rawStack[stackIndex].mModIndex = moduleIndex;
      } else {
        // PC does not belong to any module. It is probably from
        // the JIT. Use a fixed mPC so that we don't get different
        // stacks on different runs.
        rawStack[stackIndex].mPC =
          std::numeric_limits<uintptr_t>::max();
      }
    }

    if (moduleReferenced) {
      ++moduleIndex;
    } else {
      // Remove module if no PCs within its address range
      rawModules.RemoveEntries(moduleIndex, moduleIndex + 1);
    }
  }

  for (;stackIndex < stackSize; ++stackIndex) {
    // These PCs are past the last module.
    rawStack[stackIndex].mPC = std::numeric_limits<uintptr_t>::max();
  }

  std::sort(rawStack.begin(), rawStack.end(), CompareByIndex);
#endif

  // Copy the information to the return value.
  ProcessedStack Ret;
  for (std::vector<StackFrame>::iterator i = rawStack.begin(),
         e = rawStack.end(); i != e; ++i) {
    const StackFrame &rawFrame = *i;
    ProcessedStack::Frame frame = { rawFrame.mPC, rawFrame.mModIndex };
    Ret.AddFrame(frame);
  }

#ifdef MOZ_ENABLE_PROFILER_SPS
  for (unsigned i = 0, n = rawModules.GetSize(); i != n; ++i) {
    const SharedLibrary &info = rawModules.GetEntry(i);
    ProcessedStack::Module module = {
      info.GetName(),
#ifdef XP_WIN
      info.GetPdbAge(),
      "", // mPdbSignature
      info.GetPdbName(),
#else
      0, // mPdbAge
      "", // mPdbSignature
      "" // mPdbName
#endif
    };
#ifdef XP_WIN
    char guidString[NSID_LENGTH] = { 0 };
    info.GetPdbSignature().ToProvidedString(guidString);
    module.mPdbSignature = guidString;
#endif
    Ret.AddModule(module);
  }
#endif

  return Ret;
}

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
