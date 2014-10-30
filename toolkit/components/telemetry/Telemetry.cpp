/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include <fstream>

#include <prio.h>

#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Likely.h"
#include "mozilla/MathAlgorithms.h"

#include "base/histogram.h"
#include "base/pickle.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsThreadManager.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsXPCOMPrivate.h"
#include "nsIXULAppInfo.h"
#include "nsVersionComparator.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ModuleUtils.h"
#include "nsIXPConnect.h"
#include "mozilla/Services.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/GCAPI.h"
#include "nsString.h"
#include "nsITelemetry.h"
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "nsIMemoryReporter.h"
#include "nsISeekableStream.h"
#include "Telemetry.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsBaseHashtable.h"
#include "nsXULAppAPI.h"
#include "nsReadableUtils.h"
#include "nsThreadUtils.h"
#if defined(XP_WIN)
#include "nsUnicharUtils.h"
#endif
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsJSUtils.h"
#include "nsReadableUtils.h"
#include "plstr.h"
#include "nsAppDirectoryServiceDefs.h"
#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/ThreadHangStats.h"
#include "mozilla/ProcessedStack.h"
#include "mozilla/Mutex.h"
#include "mozilla/FileUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/IOInterposer.h"
#include "mozilla/PoisonIOInterposer.h"
#include "mozilla/StartupTimeline.h"
#if defined(MOZ_ENABLE_PROFILER_SPS)
#include "shared-libraries.h"
#endif

#define EXPIRED_ID "__expired__"

namespace {

using namespace base;
using namespace mozilla;

const char KEYED_HISTOGRAM_NAME_SEPARATOR[] = "#";

enum reflectStatus {
  REFLECT_OK,
  REFLECT_CORRUPT,
  REFLECT_FAILURE
};

nsresult
HistogramGet(const char *name, const char *expiration, uint32_t histogramType,
             uint32_t min, uint32_t max, uint32_t bucketCount, bool haveOptArgs,
             Histogram **result);

enum reflectStatus
ReflectHistogramSnapshot(JSContext *cx, JS::Handle<JSObject*> obj, Histogram *h);

template<class EntryType>
class AutoHashtable : public nsTHashtable<EntryType>
{
public:
  explicit AutoHashtable(uint32_t initLength = PL_DHASH_DEFAULT_INITIAL_LENGTH);
  typedef bool (*ReflectEntryFunc)(EntryType *entry, JSContext *cx, JS::Handle<JSObject*> obj);
  bool ReflectIntoJS(ReflectEntryFunc entryFunc, JSContext *cx, JS::Handle<JSObject*> obj);
private:
  struct EnumeratorArgs {
    JSContext *cx;
    JS::Handle<JSObject*> obj;
    ReflectEntryFunc entryFunc;
  };
  static PLDHashOperator ReflectEntryStub(EntryType *entry, void *arg);
};

template<class EntryType>
AutoHashtable<EntryType>::AutoHashtable(uint32_t initLength)
  : nsTHashtable<EntryType>(initLength)
{
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
                                        JSContext *cx, JS::Handle<JSObject*> obj)
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
  for (size_t i = 0; i < stackSize; ++i) {
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
  void AddHang(const Telemetry::ProcessedStack& aStack, uint32_t aDuration,
               int32_t aSystemUptime, int32_t aFirefoxUptime);
  uint32_t GetDuration(unsigned aIndex) const;
  int32_t GetSystemUptime(unsigned aIndex) const;
  int32_t GetFirefoxUptime(unsigned aIndex) const;
  const CombinedStacks& GetStacks() const;
private:
  struct HangInfo {
    // Hang duration (in seconds)
    uint32_t mDuration;
    // System uptime (in minutes) at the time of the hang
    int32_t mSystemUptime;
    // Firefox uptime (in minutes) at the time of the hang
    int32_t mFirefoxUptime;
  };
  std::vector<HangInfo> mHangInfo;
  CombinedStacks mStacks;
};

void
HangReports::AddHang(const Telemetry::ProcessedStack& aStack,
                     uint32_t aDuration,
                     int32_t aSystemUptime,
                     int32_t aFirefoxUptime) {
  HangInfo info = { aDuration, aSystemUptime, aFirefoxUptime };
  mHangInfo.push_back(info);
  mStacks.AddStack(aStack);
}

size_t
HangReports::SizeOfExcludingThis() const {
  size_t n = 0;
  n += mStacks.SizeOfExcludingThis();
  // This is a crude approximation. See comment on
  // CombinedStacks::SizeOfExcludingThis.
  n += mHangInfo.capacity() * sizeof(HangInfo);
  return n;
}

const CombinedStacks&
HangReports::GetStacks() const {
  return mStacks;
}

uint32_t
HangReports::GetDuration(unsigned aIndex) const {
  return mHangInfo[aIndex].mDuration;
}

int32_t
HangReports::GetSystemUptime(unsigned aIndex) const {
  return mHangInfo[aIndex].mSystemUptime;
}

int32_t
HangReports::GetFirefoxUptime(unsigned aIndex) const {
  return mHangInfo[aIndex].mFirefoxUptime;
}

/**
 * IOInterposeObserver recording statistics of main-thread I/O during execution,
 * aimed at consumption by TelemetryImpl
 */
class TelemetryIOInterposeObserver : public IOInterposeObserver
{
  /** File-level statistics structure */
  struct FileStats {
    FileStats()
      : creates(0)
      , reads(0)
      , writes(0)
      , fsyncs(0)
      , stats(0)
      , totalTime(0)
    {}
    uint32_t  creates;      /** Number of create/open operations */
    uint32_t  reads;        /** Number of read operations */
    uint32_t  writes;       /** Number of write operations */
    uint32_t  fsyncs;       /** Number of fsync operations */
    uint32_t  stats;        /** Number of stat operations */
    double    totalTime;    /** Accumulated duration of all operations */
  };

  struct SafeDir {
    SafeDir(const nsAString& aPath, const nsAString& aSubstName)
      : mPath(aPath)
      , mSubstName(aSubstName)
    {}
    size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
      return mPath.SizeOfExcludingThisIfUnshared(aMallocSizeOf) +
             mSubstName.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    }
    nsString  mPath;        /** Path to the directory */
    nsString  mSubstName;   /** Name to substitute with */
  };

public:
  explicit TelemetryIOInterposeObserver(nsIFile* aXreDir);

  /**
   * An implementation of Observe that records statistics of all
   * file IO operations.
   */
  void Observe(Observation& aOb);

  /**
   * Reflect recorded file IO statistics into Javascript
   */
  bool ReflectIntoJS(JSContext *cx, JS::Handle<JSObject*> rootObj);

  /**
   * Adds a path for inclusion in main thread I/O report.
   * @param aPath Directory path
   * @param aSubstName Name to substitute for aPath for privacy reasons
   */
  void AddPath(const nsAString& aPath, const nsAString& aSubstName);

  /**
   * Get size of hash table with file stats
   */
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    size_t size;
    size = mFileStats.SizeOfExcludingThis(SizeOfFileIOEntryTypeExcludingThis,
                                          aMallocSizeOf) +
           mSafeDirs.SizeOfExcludingThis(aMallocSizeOf);
    uint32_t safeDirsLen = mSafeDirs.Length();
    for (uint32_t i = 0; i < safeDirsLen; ++i) {
      size += mSafeDirs[i].SizeOfExcludingThis(aMallocSizeOf);
    }
    return size;
  }

private:
  enum Stage
  {
    STAGE_STARTUP = 0,
    STAGE_NORMAL,
    STAGE_SHUTDOWN,
    NUM_STAGES
  };
  static inline Stage NextStage(Stage aStage)
  {
    switch (aStage) {
      case STAGE_STARTUP:
        return STAGE_NORMAL;
      case STAGE_NORMAL:
        return STAGE_SHUTDOWN;
      case STAGE_SHUTDOWN:
        return STAGE_SHUTDOWN;
      default:
        return NUM_STAGES;
    }
  }

  struct FileStatsByStage
  {
    FileStats mStats[NUM_STAGES];
  };
  typedef nsBaseHashtableET<nsStringHashKey, FileStatsByStage> FileIOEntryType;

  // Statistics for each filename
  AutoHashtable<FileIOEntryType> mFileStats;
  // Container for whitelisted directories
  nsTArray<SafeDir> mSafeDirs;
  Stage             mCurStage;

  /**
   * Reflect a FileIOEntryType object to a Javascript property on obj with
   * filename as key containing array:
   * [totalTime, creates, reads, writes, fsyncs, stats]
   */
  static bool ReflectFileStats(FileIOEntryType* entry, JSContext *cx,
                               JS::Handle<JSObject*> obj);

  static size_t SizeOfFileIOEntryTypeExcludingThis(FileIOEntryType* aEntry,
                                                   mozilla::MallocSizeOf mallocSizeOf,
                                                   void*)
  {
    return aEntry->GetKey().SizeOfExcludingThisIfUnshared(mallocSizeOf);
  }
};

TelemetryIOInterposeObserver::TelemetryIOInterposeObserver(nsIFile* aXreDir)
  : mCurStage(STAGE_STARTUP)
{
  nsAutoString xreDirPath;
  nsresult rv = aXreDir->GetPath(xreDirPath);
  if (NS_SUCCEEDED(rv)) {
    AddPath(xreDirPath, NS_LITERAL_STRING("{xre}"));
  }
}

void TelemetryIOInterposeObserver::AddPath(const nsAString& aPath,
                                           const nsAString& aSubstName)
{
  mSafeDirs.AppendElement(SafeDir(aPath, aSubstName));
}

// Threshold for reporting slow main-thread I/O (50 milliseconds).
const TimeDuration kTelemetryReportThreshold = TimeDuration::FromMilliseconds(50);

void TelemetryIOInterposeObserver::Observe(Observation& aOb)
{
  // We only report main-thread I/O
  if (!IsMainThread()) {
    return;
  }

  if (aOb.ObservedOperation() == OpNextStage) {
    mCurStage = NextStage(mCurStage);
    MOZ_ASSERT(mCurStage < NUM_STAGES);
    return;
  }

  if (aOb.Duration() < kTelemetryReportThreshold) {
    return;
  }

  // Get the filename
  const char16_t* filename = aOb.Filename();
 
  // Discard observations without filename
  if (!filename) {
    return;
  }

#if defined(XP_WIN)
  nsCaseInsensitiveStringComparator comparator;
#else
  nsDefaultStringComparator comparator;
#endif
  nsAutoString      processedName;
  nsDependentString filenameStr(filename);
  uint32_t safeDirsLen = mSafeDirs.Length();
  for (uint32_t i = 0; i < safeDirsLen; ++i) {
    if (StringBeginsWith(filenameStr, mSafeDirs[i].mPath, comparator)) {
      processedName = mSafeDirs[i].mSubstName;
      processedName += Substring(filenameStr, mSafeDirs[i].mPath.Length());
      break;
    }
  }

  if (processedName.IsEmpty()) {
    return;
  }

  // Create a new entry or retrieve the existing one
  FileIOEntryType* entry = mFileStats.PutEntry(processedName);
  if (entry) {
    FileStats& stats = entry->mData.mStats[mCurStage];
    // Update the statistics
    stats.totalTime += (double) aOb.Duration().ToMilliseconds();
    switch (aOb.ObservedOperation()) {
      case OpCreateOrOpen:
        stats.creates++;
        break;
      case OpRead:
        stats.reads++;
        break;
      case OpWrite:
        stats.writes++;
        break;
      case OpFSync:
        stats.fsyncs++;
        break;
      case OpStat:
        stats.stats++;
        break;
      default:
        break;
    }
  }
}

bool TelemetryIOInterposeObserver::ReflectFileStats(FileIOEntryType* entry,
                                                    JSContext *cx,
                                                    JS::Handle<JSObject*> obj)
{
  JS::AutoValueArray<NUM_STAGES> stages(cx);

  FileStatsByStage& statsByStage = entry->mData;
  for (int s = STAGE_STARTUP; s < NUM_STAGES; ++s) {
    FileStats& fileStats = statsByStage.mStats[s];

    if (fileStats.totalTime == 0 && fileStats.creates == 0 &&
        fileStats.reads == 0 && fileStats.writes == 0 &&
        fileStats.fsyncs == 0 && fileStats.stats == 0) {
      // Don't add an array that contains no information
      stages[s].setNull();
      continue;
    }

    // Array we want to report
    JS::AutoValueArray<6> stats(cx);
    stats[0].setNumber(fileStats.totalTime);
    stats[1].setNumber(fileStats.creates);
    stats[2].setNumber(fileStats.reads);
    stats[3].setNumber(fileStats.writes);
    stats[4].setNumber(fileStats.fsyncs);
    stats[5].setNumber(fileStats.stats);

    // Create jsStats as array of elements above
    JS::RootedObject jsStats(cx, JS_NewArrayObject(cx, stats));
    if (!jsStats) {
      continue;
    }

    stages[s].setObject(*jsStats);
  }

  JS::Rooted<JSObject*> jsEntry(cx, JS_NewArrayObject(cx, stages));
  if (!jsEntry) {
    return false;
  }

  // Add jsEntry to top-level dictionary
  const nsAString& key = entry->GetKey();
  return JS_DefineUCProperty(cx, obj, key.Data(), key.Length(),
                             jsEntry, JSPROP_ENUMERATE | JSPROP_READONLY);
}

bool TelemetryIOInterposeObserver::ReflectIntoJS(JSContext *cx,
                                                 JS::Handle<JSObject*> rootObj)
{
  return mFileStats.ReflectIntoJS(ReflectFileStats, cx, rootObj);
}

// This is not a member of TelemetryImpl because we want to record I/O during
// startup.
StaticAutoPtr<TelemetryIOInterposeObserver> sTelemetryIOObserver;

void
ClearIOReporting()
{
  if (!sTelemetryIOObserver) {
    return;
  }
  IOInterposer::Unregister(IOInterposeObserver::OpAllWithStaging,
                           sTelemetryIOObserver);
  sTelemetryIOObserver = nullptr;
}

class KeyedHistogram {
public:
  KeyedHistogram(const nsACString &name, const nsACString &expiration,
                 uint32_t histogramType, uint32_t min, uint32_t max,
                 uint32_t bucketCount);
  nsresult GetHistogram(const nsCString& name, Histogram** histogram);
  uint32_t GetHistogramType() const { return mHistogramType; }
  nsresult GetJSKeys(JSContext* cx, JS::CallArgs& args);
  nsresult GetJSSnapshot(JSContext* cx, JS::Handle<JSObject*> obj);
  void Clear();

private:
  typedef nsBaseHashtableET<nsCStringHashKey, Histogram*> KeyedHistogramEntry;
  typedef AutoHashtable<KeyedHistogramEntry> KeyedHistogramMapType;
  KeyedHistogramMapType mHistogramMap;

  struct ReflectKeysArgs {
    JSContext* jsContext;
    JS::AutoValueVector* vector;
  };
  static PLDHashOperator ReflectKeys(KeyedHistogramEntry* entry, void* arg);

  static bool ReflectKeyedHistogram(KeyedHistogramEntry* entry,
                                    JSContext* cx,
                                    JS::Handle<JSObject*> obj);

  static PLDHashOperator ClearHistogramEnumerator(KeyedHistogramEntry*, void*);

  const nsCString mName;
  const nsCString mExpiration;
  const uint32_t mHistogramType;
  const uint32_t mMin;
  const uint32_t mMax;
  const uint32_t mBucketCount;
};

KeyedHistogram::KeyedHistogram(const nsACString &name, const nsACString &expiration,
                               uint32_t histogramType, uint32_t min, uint32_t max,
                               uint32_t bucketCount)
  : mHistogramMap()
  , mName(name)
  , mExpiration(expiration)
  , mHistogramType(histogramType)
  , mMin(min)
  , mMax(max)
  , mBucketCount(bucketCount)
{
}

nsresult
KeyedHistogram::GetHistogram(const nsCString& key, Histogram** histogram)
{
  KeyedHistogramEntry* entry = mHistogramMap.GetEntry(key);
  if (entry) {
    *histogram = entry->mData;
    return NS_OK;
  }

  nsCString histogramName = mName;
  histogramName.Append(KEYED_HISTOGRAM_NAME_SEPARATOR);
  histogramName.Append(key);

  Histogram* h;
  nsresult rv = HistogramGet(histogramName.get(), mExpiration.get(),
                             mHistogramType, mMin, mMax, mBucketCount,
                             true, &h);
  if (NS_FAILED(rv)) {
    return rv;
  }

  h->ClearFlags(Histogram::kUmaTargetedHistogramFlag);
  h->SetFlags(Histogram::kExtendedStatisticsFlag);
  *histogram = h;

  entry = mHistogramMap.PutEntry(key);
  if (MOZ_UNLIKELY(!entry)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  entry->mData = h;
  return NS_OK;
}

/* static */
PLDHashOperator
KeyedHistogram::ClearHistogramEnumerator(KeyedHistogramEntry* entry, void*)
{
  entry->mData->Clear();
  return PL_DHASH_NEXT;
}

void
KeyedHistogram::Clear()
{
  mHistogramMap.EnumerateEntries(&KeyedHistogram::ClearHistogramEnumerator, nullptr);
  mHistogramMap.Clear();
}

/* static */
PLDHashOperator
KeyedHistogram::ReflectKeys(KeyedHistogramEntry* entry, void* arg)
{
  ReflectKeysArgs* args = static_cast<ReflectKeysArgs*>(arg);
  const nsACString& key = entry->GetKey();

  JS::RootedValue jsKey(args->jsContext);
  const nsCString& flat = nsPromiseFlatCString(key);
  jsKey.setString(JS_NewStringCopyN(args->jsContext, flat.get(), flat.Length()));
  args->vector->append(jsKey);

  return PL_DHASH_NEXT;
}

nsresult
KeyedHistogram::GetJSKeys(JSContext* cx, JS::CallArgs& args)
{
  JS::AutoValueVector keys(cx);
  if (!keys.reserve(mHistogramMap.Count())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  ReflectKeysArgs reflectArgs = { cx, &keys };
  const uint32_t num = mHistogramMap.EnumerateEntries(&KeyedHistogram::ReflectKeys,
                                                      static_cast<void*>(&reflectArgs));
  if (num != mHistogramMap.Count()) {
    return NS_ERROR_FAILURE;
  }

  JS::RootedObject jsKeys(cx, JS_NewArrayObject(cx, keys));
  if (!jsKeys) {
    return NS_ERROR_FAILURE;
  }

  args.rval().setObject(*jsKeys);
  return NS_OK;
}

/* static */
bool
KeyedHistogram::ReflectKeyedHistogram(KeyedHistogramEntry* entry, JSContext* cx, JS::Handle<JSObject*> obj)
{
  JS::RootedObject histogramSnapshot(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
  if (!histogramSnapshot) {
    return false;
  }

  if (ReflectHistogramSnapshot(cx, histogramSnapshot, entry->mData) != REFLECT_OK) {
    return false;
  }

  const nsACString& key = entry->GetKey();
  const nsCString& flat = nsPromiseFlatCString(key);

  if (!JS_DefineProperty(cx, obj, flat.get(), histogramSnapshot, JSPROP_ENUMERATE)) {
    return false;
  }

  return true;
}

nsresult
KeyedHistogram::GetJSSnapshot(JSContext* cx, JS::Handle<JSObject*> obj)
{
  if (!mHistogramMap.ReflectIntoJS(&KeyedHistogram::ReflectKeyedHistogram, cx, obj)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

class TelemetryImpl MOZ_FINAL
  : public nsITelemetry
  , public nsIMemoryReporter
{
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITELEMETRY
  NS_DECL_NSIMEMORYREPORTER

public:
  void InitMemoryReporter();

  static bool CanRecord();
  static already_AddRefed<nsITelemetry> CreateTelemetryInstance();
  static void ShutdownTelemetry();
  static void RecordSlowStatement(const nsACString &sql, const nsACString &dbName,
                                  uint32_t delay);
#if defined(MOZ_ENABLE_PROFILER_SPS)
  static void RecordChromeHang(uint32_t aDuration,
                               Telemetry::ProcessedStack &aStack,
                               int32_t aSystemUptime,
                               int32_t aFirefoxUptime);
#endif
  static void RecordThreadHangStats(Telemetry::ThreadHangStats& aStats);
  static nsresult GetHistogramEnumId(const char *name, Telemetry::ID *id);
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);
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
  TelemetryImpl();
  ~TelemetryImpl();

  static nsCString SanitizeSQL(const nsACString& sql);

  enum SanitizedState { Sanitized, Unsanitized };

  static void StoreSlowSQL(const nsACString &offender, uint32_t delay,
                           SanitizedState state);

  static bool ReflectMainThreadSQL(SlowSQLEntryType *entry, JSContext *cx,
                                   JS::Handle<JSObject*> obj);
  static bool ReflectOtherThreadsSQL(SlowSQLEntryType *entry, JSContext *cx,
                                     JS::Handle<JSObject*> obj);
  static bool ReflectSQL(const SlowSQLEntryType *entry, const Stat *stat,
                         JSContext *cx, JS::Handle<JSObject*> obj);

  bool AddSQLInfo(JSContext *cx, JS::Handle<JSObject*> rootObj, bool mainThread,
                  bool privateSQL);
  bool GetSQLStats(JSContext *cx, JS::MutableHandle<JS::Value> ret,
                   bool includePrivateSql);

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
                                      JSContext *cx, JS::Handle<JSObject*> obj);
  static bool AddonReflector(AddonEntryType *entry, JSContext *cx, JS::Handle<JSObject*> obj);
  static bool CreateHistogramForAddon(const nsACString &name,
                                      AddonHistogramInfo &info);
  void ReadLateWritesStacks(nsIFile* aProfileDir);
  AddonMapType mAddonMap;

  // This is used for speedy string->Telemetry::ID conversions
  typedef nsBaseHashtableET<nsDepCharHashKey, Telemetry::ID> CharPtrEntryType;
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
  // mThreadHangStats stores recorded, inactive thread hang stats
  Vector<Telemetry::ThreadHangStats> mThreadHangStats;
  Mutex mThreadHangStatsMutex;

  CombinedStacks mLateWritesStacks; // This is collected out of the main thread.
  bool mCachedTelemetryData;
  uint32_t mLastShutdownTime;
  uint32_t mFailedLockCount;
  nsCOMArray<nsIFetchTelemetryDataCallback> mCallbacks;
  friend class nsFetchTelemetryData;

  typedef nsClassHashtable<nsCStringHashKey, KeyedHistogram> KeyedHistogramMapType;
  KeyedHistogramMapType mKeyedHistograms;

  struct KeyedHistogramReflectArgs {
    JSContext* jsContext;
    JS::Handle<JSObject*> object;
  };
  static PLDHashOperator KeyedHistogramsReflector(const nsACString&, nsAutoPtr<KeyedHistogram>&, void* args);
};

TelemetryImpl*  TelemetryImpl::sTelemetry = nullptr;

MOZ_DEFINE_MALLOC_SIZE_OF(TelemetryMallocSizeOf)

NS_IMETHODIMP
TelemetryImpl::CollectReports(nsIHandleReportCallback* aHandleReport,
                              nsISupports* aData, bool aAnonymize)
{
  return MOZ_COLLECT_REPORT(
    "explicit/telemetry", KIND_HEAP, UNITS_BYTES,
    SizeOfIncludingThis(TelemetryMallocSizeOf),
    "Memory used by the telemetry system.");
}

// A initializer to initialize histogram collection
StatisticsRecorder gStatisticsRecorder;

// Hardcoded probes
struct TelemetryHistogram {
  uint32_t min;
  uint32_t max;
  uint32_t bucketCount;
  uint32_t histogramType;
  uint32_t id_offset;
  uint32_t expiration_offset;
  bool extendedStatisticsOK;
  bool keyed;

  const char *id() const;
  const char *expiration() const;
};

#include "TelemetryHistogramData.inc"
bool gCorruptHistograms[Telemetry::HistogramCount];

const char *
TelemetryHistogram::id() const
{
  return &gHistogramStringTable[this->id_offset];
}

const char *
TelemetryHistogram::expiration() const
{
  return &gHistogramStringTable[this->expiration_offset];
}

bool
IsExpired(const char *expiration){
  static Version current_version = Version(MOZ_APP_VERSION);
  MOZ_ASSERT(expiration);
  return strcmp(expiration, "never") && strcmp(expiration, "default") &&
    (mozilla::Version(expiration) <= current_version);
}

bool
IsExpired(const Histogram *histogram){
  return histogram->histogram_name() == EXPIRED_ID;
}

bool
IsValidHistogramName(const nsACString& name)
{
  return !FindInReadable(nsCString(KEYED_HISTOGRAM_NAME_SEPARATOR), name);
}

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

  if (p.extendedStatisticsOK) {
    h->SetFlags(Histogram::kExtendedStatisticsFlag);
  }
  *ret = knownHistograms[id] = h;
  return NS_OK;
}

bool
FillRanges(JSContext *cx, JS::Handle<JSObject*> array, Histogram *h)
{
  JS::Rooted<JS::Value> range(cx);
  for (size_t i = 0; i < h->bucket_count(); i++) {
    range = INT_TO_JSVAL(h->ranges(i));
    if (!JS_DefineElement(cx, array, i, range, JSPROP_ENUMERATE))
      return false;
  }
  return true;
}

enum reflectStatus
ReflectHistogramAndSamples(JSContext *cx, JS::Handle<JSObject*> obj, Histogram *h,
                           const Histogram::SampleSet &ss)
{
  // We don't want to reflect corrupt histograms.
  if (h->FindCorruption(ss) != Histogram::NO_INCONSISTENCIES) {
    return REFLECT_CORRUPT;
  }

  if (!(JS_DefineProperty(cx, obj, "min", h->declared_min(), JSPROP_ENUMERATE)
        && JS_DefineProperty(cx, obj, "max", h->declared_max(), JSPROP_ENUMERATE)
        && JS_DefineProperty(cx, obj, "histogram_type", h->histogram_type(), JSPROP_ENUMERATE)
        && JS_DefineProperty(cx, obj, "sum", double(ss.sum()), JSPROP_ENUMERATE))) {
    return REFLECT_FAILURE;
  }

  if (h->histogram_type() == Histogram::HISTOGRAM) {
    if (!(JS_DefineProperty(cx, obj, "log_sum", ss.log_sum(), JSPROP_ENUMERATE)
          && JS_DefineProperty(cx, obj, "log_sum_squares", ss.log_sum_squares(), JSPROP_ENUMERATE))) {
      return REFLECT_FAILURE;
    }
  } else {
    // Export |sum_squares| as two separate 32-bit properties so that we
    // can accurately reconstruct it on the analysis side.
    uint64_t sum_squares = ss.sum_squares();
    // Cast to avoid implicit truncation warnings.
    uint32_t lo = static_cast<uint32_t>(sum_squares);
    uint32_t hi = static_cast<uint32_t>(sum_squares >> 32);
    if (!(JS_DefineProperty(cx, obj, "sum_squares_lo", lo, JSPROP_ENUMERATE)
          && JS_DefineProperty(cx, obj, "sum_squares_hi", hi, JSPROP_ENUMERATE))) {
      return REFLECT_FAILURE;
    }
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
    if (!JS_DefineElement(cx, counts_array, i, ss.counts(i), JSPROP_ENUMERATE)) {
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
IsEmpty(const Histogram *h)
{
  Histogram::SampleSet ss;
  h->SnapshotSample(&ss);

  return ss.counts(0) == 0 && ss.sum() == 0;
}

bool
JSHistogram_Add(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JSObject *obj = JS_THIS_OBJECT(cx, vp);
  if (!obj) {
    return false;
  }

  Histogram *h = static_cast<Histogram*>(JS_GetPrivate(obj));
  Histogram::ClassType type = h->histogram_type();

  int32_t value = 1;
  if (type != base::CountHistogram::COUNT_HISTOGRAM) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);
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

  if (TelemetryImpl::CanRecord()) {
    h->Add(value);
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
  JS::Rooted<JSObject*> snapshot(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
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

  Histogram *h = static_cast<Histogram*>(JS_GetPrivate(obj));
  h->Clear();
  return true;
}

nsresult
WrapAndReturnHistogram(Histogram *h, JSContext *cx, JS::MutableHandle<JS::Value> ret)
{
  static const JSClass JSHistogram_class = {
    "JSHistogram",  /* name */
    JSCLASS_HAS_PRIVATE, /* flags */
    JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub
  };

  JS::Rooted<JSObject*> obj(cx, JS_NewObject(cx, &JSHistogram_class, JS::NullPtr(), JS::NullPtr()));
  if (!obj)
    return NS_ERROR_FAILURE;
  if (!(JS_DefineFunction(cx, obj, "add", JSHistogram_Add, 1, 0)
        && JS_DefineFunction(cx, obj, "snapshot", JSHistogram_Snapshot, 0, 0)
        && JS_DefineFunction(cx, obj, "clear", JSHistogram_Clear, 0, 0))) {
    return NS_ERROR_FAILURE;
  }
  JS_SetPrivate(obj, h);
  ret.setObject(*obj);
  return NS_OK;
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
  int32_t value = 1;

  if (type != base::CountHistogram::COUNT_HISTOGRAM) {
    if (args.length() < 2) {
      JS_ReportError(cx, "Expected two arguments for this histogram type");
      return false;
    }

    if (!(args[1].isNumber() || args[1].isBoolean())) {
      JS_ReportError(cx, "Not a number");
      return false;
    }

    if (!JS::ToInt32(cx, args[0], &value)) {
      return false;
    }
  }

  Histogram* h = nullptr;
  nsresult rv = keyed->GetHistogram(NS_ConvertUTF16toUTF8(key), &h);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, "Failed to get histogram");
    return false;
  }

  if (TelemetryImpl::CanRecord()) {
    h->Add(value);
  }

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
    JS::RootedObject snapshot(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
    if (!snapshot) {
      JS_ReportError(cx, "Failed to create object");
      return false;
    }

    if (!NS_SUCCEEDED(keyed->GetJSSnapshot(cx, snapshot))) {
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
  nsresult rv = keyed->GetHistogram(NS_ConvertUTF16toUTF8(key), &h);
  if (NS_FAILED(rv)) {
    JS_ReportError(cx, "Failed to get histogram");
    return false;
  }

  JS::RootedObject snapshot(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
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

  keyed->Clear();
  return true;
}

nsresult
WrapAndReturnKeyedHistogram(KeyedHistogram *h, JSContext *cx, JS::MutableHandle<JS::Value> ret)
{
  static const JSClass JSHistogram_class = {
    "JSKeyedHistogram",  /* name */
    JSCLASS_HAS_PRIVATE, /* flags */
    JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub
  };

  JS::Rooted<JSObject*> obj(cx, JS_NewObject(cx, &JSHistogram_class, JS::NullPtr(), JS::NullPtr()));
  if (!obj)
    return NS_ERROR_FAILURE;
  if (!(JS_DefineFunction(cx, obj, "add", JSKeyedHistogram_Add, 2, 0)
        && JS_DefineFunction(cx, obj, "snapshot", JSKeyedHistogram_Snapshot, 1, 0)
        && JS_DefineFunction(cx, obj, "keys", JSKeyedHistogram_Keys, 0, 0)
        && JS_DefineFunction(cx, obj, "clear", JSKeyedHistogram_Clear, 0, 0))) {
    return NS_ERROR_FAILURE;
  }

  JS_SetPrivate(obj, h);
  ret.setObject(*obj);
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

const int32_t kMaxFailedProfileLockFileSize = 10;

bool
GetFailedLockCount(nsIInputStream* inStream, uint32_t aCount,
                   unsigned int& result)
{
  nsAutoCString bufStr;
  nsresult rv;
  rv = NS_ReadInputStreamToString(inStream, bufStr, aCount);
  NS_ENSURE_SUCCESS(rv, false);
  result = bufStr.ToInteger(&rv);
  return NS_SUCCEEDED(rv) && result > 0;
}

nsresult
GetFailedProfileLockFile(nsIFile* *aFile, nsIFile* aProfileDir)
{
  NS_ENSURE_ARG_POINTER(aProfileDir);

  nsresult rv = aProfileDir->Clone(aFile);
  NS_ENSURE_SUCCESS(rv, rv);

  (*aFile)->AppendNative(NS_LITERAL_CSTRING("Telemetry.FailedProfileLocks.txt"));
  return NS_OK;
}

class nsFetchTelemetryData : public nsRunnable
{
public:
  nsFetchTelemetryData(const char* aShutdownTimeFilename,
                       nsIFile* aFailedProfileLockFile,
                       nsIFile* aProfileDir)
    : mShutdownTimeFilename(aShutdownTimeFilename),
      mFailedProfileLockFile(aFailedProfileLockFile),
      mTelemetry(TelemetryImpl::sTelemetry),
      mProfileDir(aProfileDir)
  {
  }

private:
  const char* mShutdownTimeFilename;
  nsCOMPtr<nsIFile> mFailedProfileLockFile;
  nsRefPtr<TelemetryImpl> mTelemetry;
  nsCOMPtr<nsIFile> mProfileDir;

public:
  void MainThread() {
    mTelemetry->mCachedTelemetryData = true;
    for (unsigned int i = 0, n = mTelemetry->mCallbacks.Count(); i < n; ++i) {
      mTelemetry->mCallbacks[i]->Complete();
    }
    mTelemetry->mCallbacks.Clear();
  }

  NS_IMETHOD Run() {
    LoadFailedLockCount(mTelemetry->mFailedLockCount);
    mTelemetry->mLastShutdownTime = 
      ReadLastShutdownDuration(mShutdownTimeFilename);
    mTelemetry->ReadLateWritesStacks(mProfileDir);
    nsCOMPtr<nsIRunnable> e =
      NS_NewRunnableMethod(this, &nsFetchTelemetryData::MainThread);
    NS_ENSURE_STATE(e);
    NS_DispatchToMainThread(e);
    return NS_OK;
  }

private:
  nsresult
  LoadFailedLockCount(uint32_t& failedLockCount)
  {
    failedLockCount = 0;
    int64_t fileSize = 0;
    nsresult rv = mFailedProfileLockFile->GetFileSize(&fileSize);
    if (NS_FAILED(rv)) {
      return rv;
    }
    NS_ENSURE_TRUE(fileSize <= kMaxFailedProfileLockFileSize,
                   NS_ERROR_UNEXPECTED);
    nsCOMPtr<nsIInputStream> inStream;
    rv = NS_NewLocalFileInputStream(getter_AddRefs(inStream),
                                    mFailedProfileLockFile,
                                    PR_RDONLY);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(GetFailedLockCount(inStream, fileSize, failedLockCount),
                   NS_ERROR_UNEXPECTED);
    inStream->Close();

    mFailedProfileLockFile->Remove(false);
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
  // The user must call AsyncFetchTelemetryData first. We return zero instead of
  // reporting a failure so that the rest of telemetry can uniformly handle
  // the read not being available yet.
  if (!mCachedTelemetryData) {
    *aResult = 0;
    return NS_OK;
  }

  *aResult = mLastShutdownTime;
  return NS_OK;
}

NS_IMETHODIMP
TelemetryImpl::GetFailedProfileLockCount(uint32_t* aResult)
{
  // The user must call AsyncFetchTelemetryData first. We return zero instead of
  // reporting a failure so that the rest of telemetry can uniformly handle
  // the read not being available yet.
  if (!mCachedTelemetryData) {
    *aResult = 0;
    return NS_OK;
  }

  *aResult = mFailedLockCount;
  return NS_OK;
}

NS_IMETHODIMP
TelemetryImpl::AsyncFetchTelemetryData(nsIFetchTelemetryDataCallback *aCallback)
{
  // We have finished reading the data already, just call the callback.
  if (mCachedTelemetryData) {
    aCallback->Complete();
    return NS_OK;
  }

  // We already have a read request running, just remember the callback.
  if (mCallbacks.Count() != 0) {
    mCallbacks.AppendObject(aCallback);
    return NS_OK;
  }

  // We make this check so that GetShutdownTimeFileName() doesn't get
  // called; calling that function without telemetry enabled violates
  // assumptions that the write-the-shutdown-timestamp machinery makes.
  if (!Telemetry::CanRecord()) {
    mCachedTelemetryData = true;
    aCallback->Complete();
    return NS_OK;
  }

  // Send the read to a background thread provided by the stream transport
  // service to avoid a read in the main thread.
  nsCOMPtr<nsIEventTarget> targetThread =
    do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  if (!targetThread) {
    mCachedTelemetryData = true;
    aCallback->Complete();
    return NS_OK;
  }

  // We have to get the filename from the main thread.
  const char *shutdownTimeFilename = GetShutdownTimeFileName();
  if (!shutdownTimeFilename) {
    mCachedTelemetryData = true;
    aCallback->Complete();
    return NS_OK;
  }

  nsCOMPtr<nsIFile> profileDir;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(profileDir));
  if (NS_FAILED(rv)) {
    mCachedTelemetryData = true;
    aCallback->Complete();
    return NS_OK;
  }

  nsCOMPtr<nsIFile> failedProfileLockFile;
  rv = GetFailedProfileLockFile(getter_AddRefs(failedProfileLockFile),
                                profileDir);
  if (NS_FAILED(rv)) {
    mCachedTelemetryData = true;
    aCallback->Complete();
    return NS_OK;
  }

  mCallbacks.AppendObject(aCallback);

  nsCOMPtr<nsIRunnable> event = new nsFetchTelemetryData(shutdownTimeFilename,
                                                         failedProfileLockFile,
                                                         profileDir);

  targetThread->Dispatch(event, NS_DISPATCH_NORMAL);
  return NS_OK;
}

TelemetryImpl::TelemetryImpl():
mHistogramMap(Telemetry::HistogramCount),
mCanRecord(XRE_GetProcessType() == GeckoProcessType_Default),
mHashMutex("Telemetry::mHashMutex"),
mHangReportsMutex("Telemetry::mHangReportsMutex"),
mThreadHangStatsMutex("Telemetry::mThreadHangStatsMutex"),
mCachedTelemetryData(false),
mLastShutdownTime(0),
mFailedLockCount(0)
{
  // A whitelist to prevent Telemetry reporting on Addon & Thunderbird DBs
  const char *trackedDBs[] = {
    "addons.sqlite", "content-prefs.sqlite", "cookies.sqlite",
    "downloads.sqlite", "extensions.sqlite", "formhistory.sqlite",
    "index.sqlite", "healthreport.sqlite", "netpredictions.sqlite",
    "permissions.sqlite", "places.sqlite", "search.sqlite", "signons.sqlite",
    "urlclassifier3.sqlite", "webappsstore.sqlite"
  };

  for (size_t i = 0; i < ArrayLength(trackedDBs); i++)
    mTrackedDBs.PutEntry(nsDependentCString(trackedDBs[i]));

#ifdef DEBUG
  // Mark immutable to prevent asserts on simultaneous access from multiple threads
  mTrackedDBs.MarkImmutable();
#endif

  for (size_t i = 0; i < ArrayLength(gHistograms); ++i) {
    const TelemetryHistogram& h = gHistograms[i];
    if (!h.keyed) {
      continue;
    }

    const nsDependentCString id(h.id());
    const nsDependentCString expiration(h.expiration());
    mKeyedHistograms.Put(id, new KeyedHistogram(id, expiration, h.histogramType,
                                                    h.min, h.max, h.bucketCount));
  }
}

TelemetryImpl::~TelemetryImpl() {
  UnregisterWeakMemoryReporter(this);
}

void
TelemetryImpl::InitMemoryReporter() {
  RegisterWeakMemoryReporter(this);
}

NS_IMETHODIMP
TelemetryImpl::NewHistogram(const nsACString &name, const nsACString &expiration, uint32_t histogramType,
                            uint32_t min, uint32_t max, uint32_t bucketCount, JSContext *cx,
                            uint8_t optArgCount, JS::MutableHandle<JS::Value> ret)
{
  if (!IsValidHistogramName(name)) {
    return NS_ERROR_INVALID_ARG;
  }

  Histogram *h;
  nsresult rv = HistogramGet(PromiseFlatCString(name).get(), PromiseFlatCString(expiration).get(),
                             histogramType, min, max, bucketCount, optArgCount == 3, &h);
  if (NS_FAILED(rv))
    return rv;
  h->ClearFlags(Histogram::kUmaTargetedHistogramFlag);
  h->SetFlags(Histogram::kExtendedStatisticsFlag);
  return WrapAndReturnHistogram(h, cx, ret);
}

NS_IMETHODIMP
TelemetryImpl::NewKeyedHistogram(const nsACString &name, const nsACString &expiration, uint32_t histogramType,
                            uint32_t min, uint32_t max, uint32_t bucketCount, JSContext *cx,
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
                                             min, max, bucketCount);
  if (MOZ_UNLIKELY(!mKeyedHistograms.Put(name, keyed, fallible_t()))) {
    delete keyed;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return WrapAndReturnKeyedHistogram(keyed, cx, ret);
}


bool
TelemetryImpl::ReflectSQL(const SlowSQLEntryType *entry,
                          const Stat *stat,
                          JSContext *cx,
                          JS::Handle<JSObject*> obj)
{
  if (stat->hitCount == 0)
    return true;

  const nsACString &sql = entry->GetKey();

  JS::Rooted<JSObject*> arrayObj(cx, JS_NewArrayObject(cx, 0));
  if (!arrayObj) {
    return false;
  }
  return (JS_SetElement(cx, arrayObj, 0, stat->hitCount)
          && JS_SetElement(cx, arrayObj, 1, stat->totalTime)
          && JS_DefineProperty(cx, obj, sql.BeginReading(), arrayObj,
                               JSPROP_ENUMERATE));
}

bool
TelemetryImpl::ReflectMainThreadSQL(SlowSQLEntryType *entry, JSContext *cx,
                                    JS::Handle<JSObject*> obj)
{
  return ReflectSQL(entry, &entry->mData.mainThread, cx, obj);
}

bool
TelemetryImpl::ReflectOtherThreadsSQL(SlowSQLEntryType *entry, JSContext *cx,
                                      JS::Handle<JSObject*> obj)
{
  return ReflectSQL(entry, &entry->mData.otherThreads, cx, obj);
}

bool
TelemetryImpl::AddSQLInfo(JSContext *cx, JS::Handle<JSObject*> rootObj, bool mainThread,
                          bool privateSQL)
{
  JS::Rooted<JSObject*> statsObj(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
  if (!statsObj)
    return false;

  AutoHashtable<SlowSQLEntryType> &sqlMap =
    (privateSQL ? mPrivateSQL : mSanitizedSQL);
  AutoHashtable<SlowSQLEntryType>::ReflectEntryFunc reflectFunction =
    (mainThread ? ReflectMainThreadSQL : ReflectOtherThreadsSQL);
  if (!sqlMap.ReflectIntoJS(reflectFunction, cx, statsObj)) {
    return false;
  }

  return JS_DefineProperty(cx, rootObj,
                           mainThread ? "mainThread" : "otherThreads",
                           statsObj, JSPROP_ENUMERATE);
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
                             JSContext *cx, JS::MutableHandle<JS::Value> ret)
{
  Telemetry::ID id;
  nsresult rv = GetHistogramEnumId(PromiseFlatCString(existing_name).get(), &id);
  if (NS_FAILED(rv)) {
    return rv;
  }
  const TelemetryHistogram &p = gHistograms[id];

  Histogram *existing;
  rv = GetHistogramByEnumId(id, &existing);
  if (NS_FAILED(rv)) {
    return rv;
  }

  Histogram *clone;
  rv = HistogramGet(PromiseFlatCString(name).get(), p.expiration(),
                    p.histogramType, existing->declared_min(),
                    existing->declared_max(), existing->bucket_count(),
                    true, &clone);
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
  ret.Append(':');
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
                                 JSContext *cx, JS::MutableHandle<JS::Value> ret)
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
TelemetryImpl::GetHistogramSnapshots(JSContext *cx, JS::MutableHandle<JS::Value> ret)
{
  JS::Rooted<JSObject*> root_obj(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
  if (!root_obj)
    return NS_ERROR_FAILURE;
  ret.setObject(*root_obj);

  // Ensure that all the HISTOGRAM_FLAG & HISTOGRAM_COUNT histograms have
  // been created, so that their values are snapshotted.
  for (size_t i = 0; i < Telemetry::HistogramCount; ++i) {
    if (gHistograms[i].keyed) {
      continue;
    }
    const uint32_t type = gHistograms[i].histogramType;
    if (type == nsITelemetry::HISTOGRAM_FLAG ||
        type == nsITelemetry::HISTOGRAM_COUNT) {
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
  JS::Rooted<JSObject*> hobj(cx);
  for (HistogramIterator it = hs.begin(); it != hs.end(); ++it) {
    Histogram *h = *it;
    if (!ShouldReflectHistogram(h) || IsEmpty(h) || IsExpired(h)) {
      continue;
    }

    hobj = JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr());
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
      if (!JS_DefineProperty(cx, root_obj, h->histogram_name().c_str(), hobj,
                             JSPROP_ENUMERATE)) {
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
TelemetryImpl::AddonHistogramReflector(AddonHistogramEntryType *entry,
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

  JS::Rooted<JSObject*> snapshot(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
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
TelemetryImpl::AddonReflector(AddonEntryType *entry,
                              JSContext *cx, JS::Handle<JSObject*> obj)
{
  const nsACString &addonId = entry->GetKey();
  JS::Rooted<JSObject*> subobj(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
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

NS_IMETHODIMP
TelemetryImpl::GetAddonHistogramSnapshots(JSContext *cx, JS::MutableHandle<JS::Value> ret)
{
  JS::Rooted<JSObject*> obj(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
  if (!obj) {
    return NS_ERROR_FAILURE;
  }

  if (!mAddonMap.ReflectIntoJS(AddonReflector, cx, obj)) {
    return NS_ERROR_FAILURE;
  }
  ret.setObject(*obj);
  return NS_OK;
}

/* static */
PLDHashOperator
TelemetryImpl::KeyedHistogramsReflector(const nsACString& key, nsAutoPtr<KeyedHistogram>& entry, void* arg)
{
  KeyedHistogramReflectArgs* args = static_cast<KeyedHistogramReflectArgs*>(arg);
  JSContext *cx = args->jsContext;
  JS::RootedObject snapshot(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
  if (!snapshot) {
    return PL_DHASH_STOP;
  }

  if (!NS_SUCCEEDED(entry->GetJSSnapshot(cx, snapshot))) {
    return PL_DHASH_STOP;
  }

  if (!JS_DefineProperty(cx, args->object, PromiseFlatCString(key).get(),
                         snapshot, JSPROP_ENUMERATE)) {
    return PL_DHASH_STOP;
  }

  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
TelemetryImpl::GetKeyedHistogramSnapshots(JSContext *cx, JS::MutableHandle<JS::Value> ret)
{
  JS::Rooted<JSObject*> obj(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
  if (!obj) {
    return NS_ERROR_FAILURE;
  }

  KeyedHistogramReflectArgs reflectArgs = {cx, obj};
  const uint32_t num = mKeyedHistograms.Enumerate(&TelemetryImpl::KeyedHistogramsReflector,
                                                      static_cast<void*>(&reflectArgs));
  if (num != mKeyedHistograms.Count()) {
    return NS_ERROR_FAILURE;
  }

  ret.setObject(*obj);
  return NS_OK;
}

bool
TelemetryImpl::GetSQLStats(JSContext *cx, JS::MutableHandle<JS::Value> ret, bool includePrivateSql)
{
  JS::Rooted<JSObject*> root_obj(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
  if (!root_obj)
    return false;
  ret.setObject(*root_obj);

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
TelemetryImpl::GetSlowSQL(JSContext *cx, JS::MutableHandle<JS::Value> ret)
{
  if (GetSQLStats(cx, ret, false))
    return NS_OK;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelemetryImpl::GetDebugSlowSQL(JSContext *cx, JS::MutableHandle<JS::Value> ret)
{
  bool revealPrivateSql =
    Preferences::GetBool("toolkit.telemetry.debugSlowSql", false);
  if (GetSQLStats(cx, ret, revealPrivateSql))
    return NS_OK;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelemetryImpl::GetMaximalNumberOfConcurrentThreads(uint32_t *ret)
{
  *ret = nsThreadManager::get()->GetHighestNumberOfThreads();
  return NS_OK;
}

NS_IMETHODIMP
TelemetryImpl::GetChromeHangs(JSContext *cx, JS::MutableHandle<JS::Value> ret)
{
  MutexAutoLock hangReportMutex(mHangReportsMutex);

  const CombinedStacks& stacks = mHangReports.GetStacks();
  JS::Rooted<JSObject*> fullReportObj(cx, CreateJSStackObject(cx, stacks));
  if (!fullReportObj) {
    return NS_ERROR_FAILURE;
  }

  ret.setObject(*fullReportObj);

  JS::Rooted<JSObject*> durationArray(cx, JS_NewArrayObject(cx, 0));
  JS::Rooted<JSObject*> systemUptimeArray(cx, JS_NewArrayObject(cx, 0));
  JS::Rooted<JSObject*> firefoxUptimeArray(cx, JS_NewArrayObject(cx, 0));
  if (!durationArray || !systemUptimeArray || !firefoxUptimeArray) {
    return NS_ERROR_FAILURE;
  }

  bool ok = JS_DefineProperty(cx, fullReportObj, "durations",
                              durationArray, JSPROP_ENUMERATE);
  if (!ok) {
    return NS_ERROR_FAILURE;
  }

  ok = JS_DefineProperty(cx, fullReportObj, "systemUptime",
                         systemUptimeArray, JSPROP_ENUMERATE);
  if (!ok) {
    return NS_ERROR_FAILURE;
  }

  ok = JS_DefineProperty(cx, fullReportObj, "firefoxUptime",
                         firefoxUptimeArray, JSPROP_ENUMERATE);
  if (!ok) {
    return NS_ERROR_FAILURE;
  }

  const size_t length = stacks.GetStackCount();
  for (size_t i = 0; i < length; ++i) {
    if (!JS_SetElement(cx, durationArray, i, mHangReports.GetDuration(i))) {
      return NS_ERROR_FAILURE;
    }
    if (!JS_SetElement(cx, systemUptimeArray, i, mHangReports.GetSystemUptime(i))) {
      return NS_ERROR_FAILURE;
    }
    if (!JS_SetElement(cx, firefoxUptimeArray, i, mHangReports.GetFirefoxUptime(i))) {
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

static JSObject *
CreateJSStackObject(JSContext *cx, const CombinedStacks &stacks) {
  JS::Rooted<JSObject*> ret(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
  if (!ret) {
    return nullptr;
  }

  JS::Rooted<JSObject*> moduleArray(cx, JS_NewArrayObject(cx, 0));
  if (!moduleArray) {
    return nullptr;
  }
  bool ok = JS_DefineProperty(cx, ret, "memoryMap", moduleArray,
                              JSPROP_ENUMERATE);
  if (!ok) {
    return nullptr;
  }

  const size_t moduleCount = stacks.GetModuleCount();
  for (size_t moduleIndex = 0; moduleIndex < moduleCount; ++moduleIndex) {
    // Current module
    const Telemetry::ProcessedStack::Module& module =
      stacks.GetModule(moduleIndex);

    JS::Rooted<JSObject*> moduleInfoArray(cx, JS_NewArrayObject(cx, 0));
    if (!moduleInfoArray) {
      return nullptr;
    }
    if (!JS_SetElement(cx, moduleArray, moduleIndex, moduleInfoArray)) {
      return nullptr;
    }

    unsigned index = 0;

    // Module name
    JS::Rooted<JSString*> str(cx, JS_NewStringCopyZ(cx, module.mName.c_str()));
    if (!str) {
      return nullptr;
    }
    if (!JS_SetElement(cx, moduleInfoArray, index++, str)) {
      return nullptr;
    }

    // Module breakpad identifier
    JS::Rooted<JSString*> id(cx, JS_NewStringCopyZ(cx, module.mBreakpadId.c_str()));
    if (!id) {
      return nullptr;
    }
    if (!JS_SetElement(cx, moduleInfoArray, index++, id)) {
      return nullptr;
    }
  }

  JS::Rooted<JSObject*> reportArray(cx, JS_NewArrayObject(cx, 0));
  if (!reportArray) {
    return nullptr;
  }
  ok = JS_DefineProperty(cx, ret, "stacks", reportArray, JSPROP_ENUMERATE);
  if (!ok) {
    return nullptr;
  }

  const size_t length = stacks.GetStackCount();
  for (size_t i = 0; i < length; ++i) {
    // Represent call stack PCs as (module index, offset) pairs.
    JS::Rooted<JSObject*> pcArray(cx, JS_NewArrayObject(cx, 0));
    if (!pcArray) {
      return nullptr;
    }

    if (!JS_SetElement(cx, reportArray, i, pcArray)) {
      return nullptr;
    }

    const CombinedStacks::Stack& stack = stacks.GetStack(i);
    const uint32_t pcCount = stack.size();
    for (size_t pcIndex = 0; pcIndex < pcCount; ++pcIndex) {
      const Telemetry::ProcessedStack::Frame& frame = stack[pcIndex];
      JS::Rooted<JSObject*> framePair(cx, JS_NewArrayObject(cx, 0));
      if (!framePair) {
        return nullptr;
      }
      int modIndex = (std::numeric_limits<uint16_t>::max() == frame.mModIndex) ?
        -1 : frame.mModIndex;
      if (!JS_SetElement(cx, framePair, 0, modIndex)) {
        return nullptr;
      }
      if (!JS_SetElement(cx, framePair, 1, static_cast<double>(frame.mOffset))) {
        return nullptr;
      }
      if (!JS_SetElement(cx, pcArray, pcIndex, framePair)) {
        return nullptr;
      }
    }
  }

  return ret;
}

static bool
IsValidBreakpadId(const std::string &breakpadId) {
  if (breakpadId.size() < 33) {
    return false;
  }
  for (unsigned i = 0, n = breakpadId.size(); i < n; ++i) {
    char c = breakpadId[i];
    if ((c < '0' || c > '9') && (c < 'A' || c > 'F')) {
      return false;
    }
  }
  return true;
}

// Read a stack from the given file name. In case of any error, aStack is
// unchanged.
static void
ReadStack(const char *aFileName, Telemetry::ProcessedStack &aStack)
{
  std::ifstream file(aFileName);

  size_t numModules;
  file >> numModules;
  if (file.fail()) {
    return;
  }

  char newline = file.get();
  if (file.fail() || newline != '\n') {
    return;
  }

  Telemetry::ProcessedStack stack;
  for (size_t i = 0; i < numModules; ++i) {
    std::string breakpadId;
    file >> breakpadId;
    if (file.fail() || !IsValidBreakpadId(breakpadId)) {
      return;
    }

    char space = file.get();
    if (file.fail() || space != ' ') {
      return;
    }

    std::string moduleName;
    getline(file, moduleName);
    if (file.fail() || moduleName[0] == ' ') {
      return;
    }

    Telemetry::ProcessedStack::Module module = {
      moduleName,
      breakpadId
    };
    stack.AddModule(module);
  }

  size_t numFrames;
  file >> numFrames;
  if (file.fail()) {
    return;
  }

  newline = file.get();
  if (file.fail() || newline != '\n') {
    return;
  }

  for (size_t i = 0; i < numFrames; ++i) {
    uint16_t index;
    file >> index;
    uintptr_t offset;
    file >> std::hex >> offset >> std::dec;
    if (file.fail()) {
      return;
    }

    Telemetry::ProcessedStack::Frame frame = {
      offset,
      index
    };
    stack.AddFrame(frame);
  }

  aStack = stack;
}

static JSObject*
CreateJSTimeHistogram(JSContext* cx, const Telemetry::TimeHistogram& time)
{
  /* Create JS representation of TimeHistogram,
     in the format of Chromium-style histograms. */
  JS::RootedObject ret(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
  if (!ret) {
    return nullptr;
  }

  if (!JS_DefineProperty(cx, ret, "min", time.GetBucketMin(0),
                         JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, ret, "max",
                         time.GetBucketMax(ArrayLength(time) - 1),
                         JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, ret, "histogram_type",
                         nsITelemetry::HISTOGRAM_EXPONENTIAL,
                         JSPROP_ENUMERATE)) {
    return nullptr;
  }
  // TODO: calculate "sum", "log_sum", and "log_sum_squares"
  if (!JS_DefineProperty(cx, ret, "sum", 0, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, ret, "log_sum", 0.0, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, ret, "log_sum_squares", 0.0, JSPROP_ENUMERATE)) {
    return nullptr;
  }

  JS::RootedObject ranges(
    cx, JS_NewArrayObject(cx, ArrayLength(time) + 1));
  JS::RootedObject counts(
    cx, JS_NewArrayObject(cx, ArrayLength(time) + 1));
  if (!ranges || !counts) {
    return nullptr;
  }
  /* In a Chromium-style histogram, the first bucket is an "under" bucket
     that represents all values below the histogram's range. */
  if (!JS_SetElement(cx, ranges, 0, time.GetBucketMin(0)) ||
      !JS_SetElement(cx, counts, 0, 0)) {
    return nullptr;
  }
  for (size_t i = 0; i < ArrayLength(time); i++) {
    if (!JS_SetElement(cx, ranges, i + 1, time.GetBucketMax(i)) ||
        !JS_SetElement(cx, counts, i + 1, time[i])) {
      return nullptr;
    }
  }
  if (!JS_DefineProperty(cx, ret, "ranges", ranges, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, ret, "counts", counts, JSPROP_ENUMERATE)) {
    return nullptr;
  }
  return ret;
}

static JSObject*
CreateJSHangStack(JSContext* cx, const Telemetry::HangStack& stack)
{
  JS::RootedObject ret(cx, JS_NewArrayObject(cx, stack.length()));
  if (!ret) {
    return nullptr;
  }
  for (size_t i = 0; i < stack.length(); i++) {
    JS::RootedString string(cx, JS_NewStringCopyZ(cx, stack[i]));
    if (!JS_SetElement(cx, ret, i, string)) {
      return nullptr;
    }
  }
  return ret;
}

static JSObject*
CreateJSHangHistogram(JSContext* cx, const Telemetry::HangHistogram& hang)
{
  JS::RootedObject ret(cx,
    JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
  if (!ret) {
    return nullptr;
  }

  JS::RootedObject stack(cx, CreateJSHangStack(cx, hang.GetStack()));
  JS::RootedObject time(cx, CreateJSTimeHistogram(cx, hang));

  if (!stack ||
      !time ||
      !JS_DefineProperty(cx, ret, "stack", stack, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, ret, "histogram", time, JSPROP_ENUMERATE)) {
    return nullptr;
  }

  if (!hang.GetNativeStack().empty()) {
    JS::RootedObject native(cx, CreateJSHangStack(cx, hang.GetNativeStack()));
    if (!native ||
        !JS_DefineProperty(cx, ret, "nativeStack", native, JSPROP_ENUMERATE)) {
      return nullptr;
    }
  }
  return ret;
}

static JSObject*
CreateJSThreadHangStats(JSContext* cx, const Telemetry::ThreadHangStats& thread)
{
  JS::RootedObject ret(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
  if (!ret) {
    return nullptr;
  }
  JS::RootedString name(cx, JS_NewStringCopyZ(cx, thread.GetName()));
  if (!name ||
      !JS_DefineProperty(cx, ret, "name", name, JSPROP_ENUMERATE)) {
    return nullptr;
  }

  JS::RootedObject activity(cx, CreateJSTimeHistogram(cx, thread.mActivity));
  if (!activity ||
      !JS_DefineProperty(cx, ret, "activity", activity, JSPROP_ENUMERATE)) {
    return nullptr;
  }

  JS::RootedObject hangs(cx, JS_NewArrayObject(cx, 0));
  if (!hangs) {
    return nullptr;
  }
  for (size_t i = 0; i < thread.mHangs.length(); i++) {
    JS::RootedObject obj(cx, CreateJSHangHistogram(cx, thread.mHangs[i]));
    if (!JS_SetElement(cx, hangs, i, obj)) {
      return nullptr;
    }
  }
  if (!JS_DefineProperty(cx, ret, "hangs", hangs, JSPROP_ENUMERATE)) {
    return nullptr;
  }
  return ret;
}

NS_IMETHODIMP
TelemetryImpl::GetThreadHangStats(JSContext* cx, JS::MutableHandle<JS::Value> ret)
{
  JS::RootedObject retObj(cx, JS_NewArrayObject(cx, 0));
  if (!retObj) {
    return NS_ERROR_FAILURE;
  }
  size_t threadIndex = 0;

#ifdef MOZ_ENABLE_BACKGROUND_HANG_MONITOR
  /* First add active threads; we need to hold |iter| (and its lock)
     throughout this method to avoid a race condition where a thread can
     be recorded twice if the thread is destroyed while this method is
     running */
  BackgroundHangMonitor::ThreadHangStatsIterator iter;
  for (Telemetry::ThreadHangStats* histogram = iter.GetNext();
       histogram; histogram = iter.GetNext()) {
    JS::RootedObject obj(cx,
      CreateJSThreadHangStats(cx, *histogram));
    if (!JS_SetElement(cx, retObj, threadIndex++, obj)) {
      return NS_ERROR_FAILURE;
    }
  }
#endif

  // Add saved threads next
  MutexAutoLock autoLock(mThreadHangStatsMutex);
  for (size_t i = 0; i < mThreadHangStats.length(); i++) {
    JS::RootedObject obj(cx,
      CreateJSThreadHangStats(cx, mThreadHangStats[i]));
    if (!JS_SetElement(cx, retObj, threadIndex++, obj)) {
      return NS_ERROR_FAILURE;
    }
  }
  ret.setObject(*retObj);
  return NS_OK;
}

void
TelemetryImpl::ReadLateWritesStacks(nsIFile* aProfileDir)
{
  nsAutoCString nativePath;
  nsresult rv = aProfileDir->GetNativePath(nativePath);
  if (NS_FAILED(rv)) {
    return;
  }

  const char *name = nativePath.get();
  PRDir *dir = PR_OpenDir(name);
  if (!dir) {
    return;
  }

  PRDirEntry *ent;
  const char *prefix = "Telemetry.LateWriteFinal-";
  unsigned int prefixLen = strlen(prefix);
  while ((ent = PR_ReadDir(dir, PR_SKIP_NONE))) {
    if (strncmp(prefix, ent->name, prefixLen) != 0) {
      continue;
    }

    nsAutoCString stackNativePath = nativePath;
    stackNativePath += XPCOM_FILE_PATH_SEPARATOR;
    stackNativePath += nsDependentCString(ent->name);

    Telemetry::ProcessedStack stack;
    ReadStack(stackNativePath.get(), stack);
    if (stack.GetStackSize() != 0) {
      mLateWritesStacks.AddStack(stack);
    }
    // Delete the file so that we don't report it again on the next run.
    PR_Delete(stackNativePath.get());
  }
  PR_CloseDir(dir);
}

NS_IMETHODIMP
TelemetryImpl::GetLateWrites(JSContext *cx, JS::MutableHandle<JS::Value> ret)
{
  // The user must call AsyncReadTelemetryData first. We return an empty list
  // instead of reporting a failure so that the rest of telemetry can uniformly
  // handle the read not being available yet.

  // FIXME: we allocate the js object again and again in the getter. We should
  // figure out a way to cache it. In order to do that we have to call
  // JS_AddNamedObjectRoot. A natural place to do so is in the TelemetryImpl
  // constructor, but it is not clear how to get a JSContext in there.
  // Another option would be to call it in here when we first call
  // CreateJSStackObject, but we would still need to figure out where to call
  // JS_RemoveObjectRoot. Would it be ok to never call JS_RemoveObjectRoot
  // and just set the pointer to nullptr is the telemetry destructor?

  JSObject *report;
  if (!mCachedTelemetryData) {
    CombinedStacks empty;
    report = CreateJSStackObject(cx, empty);
  } else {
    report = CreateJSStackObject(cx, mLateWritesStacks);
  }

  if (report == nullptr) {
    return NS_ERROR_FAILURE;
  }

  ret.setObject(*report);
  return NS_OK;
}

nsresult
GetRegisteredHistogramIds(bool keyed, uint32_t *aCount, char*** aHistograms)
{
  nsTArray<char*> collection;

  for (size_t i = 0; i < ArrayLength(gHistograms); ++i) {
    const TelemetryHistogram& h = gHistograms[i];
    if (IsExpired(h.expiration()) || h.keyed != keyed) {
      continue;
    }

    const char* id = h.id();
    const size_t len = strlen(id);
    collection.AppendElement(static_cast<char*>(nsMemory::Clone(id, len+1)));
  }

  const size_t bytes = collection.Length() * sizeof(char*);
  char** histograms = static_cast<char**>(nsMemory::Alloc(bytes));
  memcpy(histograms, collection.Elements(), bytes);
  *aHistograms = histograms;
  *aCount = collection.Length();

  return NS_OK;
}

NS_IMETHODIMP
TelemetryImpl::RegisteredHistograms(uint32_t *aCount, char*** aHistograms)
{
  return GetRegisteredHistogramIds(false, aCount, aHistograms);
}

NS_IMETHODIMP
TelemetryImpl::RegisteredKeyedHistograms(uint32_t *aCount, char*** aHistograms)
{
  return GetRegisteredHistogramIds(true, aCount, aHistograms);
}

NS_IMETHODIMP
TelemetryImpl::GetHistogramById(const nsACString &name, JSContext *cx,
                                JS::MutableHandle<JS::Value> ret)
{
  Histogram *h;
  nsresult rv = GetHistogramByName(name, &h);
  if (NS_FAILED(rv))
    return rv;

  return WrapAndReturnHistogram(h, cx, ret);
}

NS_IMETHODIMP
TelemetryImpl::GetKeyedHistogramById(const nsACString &name, JSContext *cx,
                                     JS::MutableHandle<JS::Value> ret)
{
  KeyedHistogram* keyed = nullptr;
  if (!mKeyedHistograms.Get(name, &keyed)) {
    return NS_ERROR_FAILURE;
  }

  return WrapAndReturnKeyedHistogram(keyed, cx, ret);
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
  NS_ABORT_IF_FALSE(sTelemetry == nullptr, "CreateTelemetryInstance may only be called once, via GetService()");
  sTelemetry = new TelemetryImpl();
  // AddRef for the local reference
  NS_ADDREF(sTelemetry);
  // AddRef for the caller
  nsCOMPtr<nsITelemetry> ret = sTelemetry;

  sTelemetry->InitMemoryReporter();

  return ret.forget();
}

void
TelemetryImpl::ShutdownTelemetry()
{
  // No point in collecting IO beyond this point
  ClearIOReporting();
  NS_IF_RELEASE(sTelemetry);
}

void
TelemetryImpl::StoreSlowSQL(const nsACString &sql, uint32_t delay,
                            SanitizedState state)
{
  AutoHashtable<SlowSQLEntryType> *slowSQLMap = nullptr;
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

// Slow SQL statements will be automatically
// trimmed to kMaxSlowStatementLength characters.
// This limit doesn't include the ellipsis and DB name,
// that are appended at the end of the stored statement.
const uint32_t kMaxSlowStatementLength = 1000;

void
TelemetryImpl::RecordSlowStatement(const nsACString &sql,
                                   const nsACString &dbName,
                                   uint32_t delay)
{
  if (!sTelemetry || !sTelemetry->mCanRecord)
    return;

  bool isFirefoxDB = sTelemetry->mTrackedDBs.Contains(dbName);
  if (isFirefoxDB) {
    nsAutoCString sanitizedSQL(SanitizeSQL(sql));
    if (sanitizedSQL.Length() > kMaxSlowStatementLength) {
      sanitizedSQL.SetLength(kMaxSlowStatementLength);
      sanitizedSQL += "...";
    }
    sanitizedSQL.AppendPrintf(" /* %s */", nsPromiseFlatCString(dbName).get());
    StoreSlowSQL(sanitizedSQL, delay, Sanitized);
  } else {
    // Report aggregate DB-level statistics for addon DBs
    nsAutoCString aggregate;
    aggregate.AppendPrintf("Untracked SQL for %s",
                           nsPromiseFlatCString(dbName).get());
    StoreSlowSQL(aggregate, delay, Sanitized);
  }

  nsAutoCString fullSQL;
  fullSQL.AppendPrintf("%s /* %s */",
                       nsPromiseFlatCString(sql).get(),
                       nsPromiseFlatCString(dbName).get());
  StoreSlowSQL(fullSQL, delay, Unsanitized);
}

#if defined(MOZ_ENABLE_PROFILER_SPS)
void
TelemetryImpl::RecordChromeHang(uint32_t aDuration,
                                Telemetry::ProcessedStack &aStack,
                                int32_t aSystemUptime,
                                int32_t aFirefoxUptime)
{
  if (!sTelemetry || !sTelemetry->mCanRecord)
    return;

  MutexAutoLock hangReportMutex(sTelemetry->mHangReportsMutex);

  sTelemetry->mHangReports.AddHang(aStack, aDuration,
                                   aSystemUptime, aFirefoxUptime);
}
#endif

void
TelemetryImpl::RecordThreadHangStats(Telemetry::ThreadHangStats& aStats)
{
  if (!sTelemetry || !sTelemetry->mCanRecord)
    return;

  MutexAutoLock autoLock(sTelemetry->mThreadHangStatsMutex);

  sTelemetry->mThreadHangStats.append(Move(aStats));
}

NS_IMPL_ISUPPORTS(TelemetryImpl, nsITelemetry, nsIMemoryReporter)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsITelemetry, TelemetryImpl::CreateTelemetryInstance)

#define NS_TELEMETRY_CID \
  {0xaea477f2, 0xb3a2, 0x469c, {0xaa, 0x29, 0x0a, 0x82, 0xd1, 0x32, 0xb8, 0x29}}
NS_DEFINE_NAMED_CID(NS_TELEMETRY_CID);

const Module::CIDEntry kTelemetryCIDs[] = {
  { &kNS_TELEMETRY_CID, false, nullptr, nsITelemetryConstructor },
  { nullptr }
};

const Module::ContractIDEntry kTelemetryContracts[] = {
  { "@mozilla.org/base/telemetry;1", &kNS_TELEMETRY_CID },
  { nullptr }
};

const Module kTelemetryModule = {
  Module::kVersion,
  kTelemetryCIDs,
  kTelemetryContracts,
  nullptr,
  nullptr,
  nullptr,
  TelemetryImpl::ShutdownTelemetry
};

NS_IMETHODIMP
TelemetryImpl::GetFileIOReports(JSContext *cx, JS::MutableHandleValue ret)
{
  if (sTelemetryIOObserver) {
    JS::Rooted<JSObject*> obj(cx, JS_NewObject(cx, nullptr, JS::NullPtr(),
                                               JS::NullPtr()));
    if (!obj) {
      return NS_ERROR_FAILURE;
    }

    if (!sTelemetryIOObserver->ReflectIntoJS(cx, obj)) {
      return NS_ERROR_FAILURE;
    }
    ret.setObject(*obj);
    return NS_OK;
  }
  ret.setNull();
  return NS_OK;
}

NS_IMETHODIMP
TelemetryImpl::MsSinceProcessStart(double* aResult)
{
  bool error;
  *aResult = (TimeStamp::NowLoRes() -
              TimeStamp::ProcessCreation(error)).ToMilliseconds();
  if (error) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return NS_OK;
}

size_t
TelemetryImpl::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
{
  size_t n = aMallocSizeOf(this);

  // Ignore the hashtables in mAddonMap; they are not significant.
  n += mAddonMap.SizeOfExcludingThis(nullptr, aMallocSizeOf);
  n += mHistogramMap.SizeOfExcludingThis(nullptr, aMallocSizeOf);
  { // Scope for mHashMutex lock
    MutexAutoLock lock(mHashMutex);
    n += mPrivateSQL.SizeOfExcludingThis(aMallocSizeOf);
    n += mSanitizedSQL.SizeOfExcludingThis(aMallocSizeOf);
  }
  n += mTrackedDBs.SizeOfExcludingThis(aMallocSizeOf);
  { // Scope for mHangReportsMutex lock
    MutexAutoLock lock(mHangReportsMutex);
    n += mHangReports.SizeOfExcludingThis();
  }
  { // Scope for mThreadHangStatsMutex lock
    MutexAutoLock lock(mThreadHangStatsMutex);
    n += mThreadHangStats.sizeOfExcludingThis(aMallocSizeOf);
  }

  // It's a bit gross that we measure this other stuff that lives outside of
  // TelemetryImpl... oh well.
  if (sTelemetryIOObserver) {
    n += sTelemetryIOObserver->SizeOfIncludingThis(aMallocSizeOf);
  }
  StatisticsRecorder::Histograms hs;
  StatisticsRecorder::GetHistograms(&hs);
  for (HistogramIterator it = hs.begin(); it != hs.end(); ++it) {
    Histogram *h = *it;
    n += h->SizeOfIncludingThis(aMallocSizeOf);
  }

  return n;
}

} // anonymous namespace

namespace mozilla {
void
RecordShutdownStartTimeStamp() {
#ifdef DEBUG
  // FIXME: this function should only be called once, since it should be called
  // at the earliest point we *know* we are shutting down. Unfortunately
  // this assert has been firing. Given that if we are called multiple times
  // we just keep the last timestamp, the assert is commented for now.
  static bool recorded = false;
  //  MOZ_ASSERT(!recorded);
  (void)recorded; // Silence unused-var warnings (remove when assert re-enabled)
  recorded = true;
#endif

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
  MozillaRegisterDebugFILE(f);

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
Accumulate(const char* name, uint32_t sample)
{
  if (!TelemetryImpl::CanRecord()) {
    return;
  }
  ID id;
  nsresult rv = TelemetryImpl::GetHistogramEnumId(name, &id);
  if (NS_FAILED(rv)) {
    return;
  }

  Histogram *h;
  rv = GetHistogramByEnumId(id, &h);
  if (NS_SUCCEEDED(rv)) {
    h->Add(sample);
  }
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
  Histogram *h = nullptr;
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
                      ProcessedStack &aStack,
                      int32_t aSystemUptime,
                      int32_t aFirefoxUptime)
{
  TelemetryImpl::RecordChromeHang(duration, aStack,
                                  aSystemUptime, aFirefoxUptime);
}
#endif

void RecordThreadHangStats(ThreadHangStats& aStats)
{
  TelemetryImpl::RecordThreadHangStats(aStats);
}

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
    mBreakpadId == aOther.mBreakpadId;
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
    const std::string &name = info.GetName();
    std::string basename = name;
#ifdef XP_MACOSX
    // FIXME: We want to use just the basename as the libname, but the
    // current profiler addon needs the full path name, so we compute the
    // basename in here.
    size_t pos = name.rfind('/');
    if (pos != std::string::npos) {
      basename = name.substr(pos + 1);
    }
#endif
    ProcessedStack::Module module = {
      basename,
      info.GetBreakpadId()
    };
    Ret.AddModule(module);
  }
#endif

  return Ret;
}

void
WriteFailedProfileLock(nsIFile* aProfileDir)
{
  nsCOMPtr<nsIFile> file;
  nsresult rv = GetFailedProfileLockFile(getter_AddRefs(file), aProfileDir);
  NS_ENSURE_SUCCESS_VOID(rv);
  int64_t fileSize = 0;
  rv = file->GetFileSize(&fileSize);
  // It's expected that the file might not exist yet
  if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND) {
    return;
  }
  nsCOMPtr<nsIFileStream> fileStream;
  rv = NS_NewLocalFileStream(getter_AddRefs(fileStream), file,
                             PR_RDWR | PR_CREATE_FILE, 0640);
  NS_ENSURE_SUCCESS_VOID(rv);
  NS_ENSURE_TRUE_VOID(fileSize <= kMaxFailedProfileLockFileSize);
  unsigned int failedLockCount = 0;
  if (fileSize > 0) {
    nsCOMPtr<nsIInputStream> inStream = do_QueryInterface(fileStream);
    NS_ENSURE_TRUE_VOID(inStream);
    if (!GetFailedLockCount(inStream, fileSize, failedLockCount)) {
      failedLockCount = 0;
    }
  }
  ++failedLockCount;
  nsAutoCString bufStr;
  bufStr.AppendInt(static_cast<int>(failedLockCount));
  nsCOMPtr<nsISeekableStream> seekStream = do_QueryInterface(fileStream);
  NS_ENSURE_TRUE_VOID(seekStream);
  // If we read in an existing failed lock count, we need to reset the file ptr
  if (fileSize > 0) {
    rv = seekStream->Seek(nsISeekableStream::NS_SEEK_SET, 0);
    NS_ENSURE_SUCCESS_VOID(rv);
  }
  nsCOMPtr<nsIOutputStream> outStream = do_QueryInterface(fileStream);
  uint32_t bytesLeft = bufStr.Length();
  const char* bytes = bufStr.get();
  do {
    uint32_t written = 0;
    rv = outStream->Write(bytes, bytesLeft, &written);
    if (NS_FAILED(rv)) {
      break;
    }
    bytes += written;
    bytesLeft -= written;
  } while (bytesLeft > 0);
  seekStream->SetEOF();
}

void
InitIOReporting(nsIFile* aXreDir)
{
  // Never initialize twice
  if (sTelemetryIOObserver) {
    return;
  }

  sTelemetryIOObserver = new TelemetryIOInterposeObserver(aXreDir);
  IOInterposer::Register(IOInterposeObserver::OpAllWithStaging,
                         sTelemetryIOObserver);
}

void
SetProfileDir(nsIFile* aProfD)
{
  if (!sTelemetryIOObserver || !aProfD) {
    return;
  }
  nsAutoString profDirPath;
  nsresult rv = aProfD->GetPath(profDirPath);
  if (NS_FAILED(rv)) {
    return;
  }
  sTelemetryIOObserver->AddPath(profDirPath, NS_LITERAL_STRING("{profile}"));
}

void
TimeHistogram::Add(PRIntervalTime aTime)
{
  uint32_t timeMs = PR_IntervalToMilliseconds(aTime);
  size_t index = mozilla::FloorLog2(timeMs);
  operator[](index)++;
}

const char*
HangStack::InfallibleAppendViaBuffer(const char* aText, size_t aLength)
{
  MOZ_ASSERT(this->canAppendWithoutRealloc(1));
  // Include null-terminator in length count.
  MOZ_ASSERT(mBuffer.canAppendWithoutRealloc(aLength + 1));

  const char* const entry = mBuffer.end();
  mBuffer.infallibleAppend(aText, aLength);
  mBuffer.infallibleAppend('\0'); // Explicitly append null-terminator
  this->infallibleAppend(entry);
  return entry;
}

const char*
HangStack::AppendViaBuffer(const char* aText, size_t aLength)
{
  if (!this->reserve(this->length() + 1)) {
    return nullptr;
  }

  // Keep track of the previous buffer in case we need to adjust pointers later.
  const char* const prevStart = mBuffer.begin();
  const char* const prevEnd = mBuffer.end();

  // Include null-terminator in length count.
  if (!mBuffer.reserve(mBuffer.length() + aLength + 1)) {
    return nullptr;
  }

  if (prevStart != mBuffer.begin()) {
    // The buffer has moved; we have to adjust pointers in the stack.
    for (const char** entry = this->begin(); entry != this->end(); entry++) {
      if (*entry >= prevStart && *entry < prevEnd) {
        // Move from old buffer to new buffer.
        *entry += mBuffer.begin() - prevStart;
      }
    }
  }

  return InfallibleAppendViaBuffer(aText, aLength);
}

uint32_t
HangHistogram::GetHash(const HangStack& aStack)
{
  uint32_t hash = 0;
  for (const char* const* label = aStack.begin();
       label != aStack.end(); label++) {
    /* If the string is within our buffer, we need to hash its content.
       Otherwise, the string is statically allocated, and we only need
       to hash the pointer instead of the content. */
    if (aStack.IsInBuffer(*label)) {
      hash = AddToHash(hash, HashString(*label));
    } else {
      hash = AddToHash(hash, *label);
    }
  }
  return hash;
}

bool
HangHistogram::operator==(const HangHistogram& aOther) const
{
  if (mHash != aOther.mHash) {
    return false;
  }
  if (mStack.length() != aOther.mStack.length()) {
    return false;
  }
  return mStack == aOther.mStack;
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
