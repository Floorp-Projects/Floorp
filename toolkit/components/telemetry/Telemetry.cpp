/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include <fstream>

#include <prio.h>
#include <prproces.h>
#ifdef XP_LINUX
#include <time.h>
#else
#include <chrono>
#endif

#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Likely.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Unused.h"

#include "base/pickle.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsThreadManager.h"
#include "nsXPCOMCIDInternal.h"
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
#include "TelemetryCommon.h"
#include "TelemetryHistogram.h"
#include "ipc/TelemetryIPCAccumulator.h"
#include "TelemetryScalar.h"
#include "TelemetryEvent.h"
#include "WebrtcTelemetry.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsBaseHashtable.h"
#include "nsClassHashtable.h"
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
#include "mozilla/HangMonitor.h"
#include "nsNativeCharsetUtils.h"
#include "nsProxyRelease.h"
#include "HangReports.h"

#if defined(MOZ_GECKO_PROFILER)
#include "shared-libraries.h"
#include "KeyedStackCapturer.h"
#endif // MOZ_GECKO_PROFILER

namespace {

using namespace mozilla;
using namespace mozilla::HangMonitor;
using Telemetry::Common::AutoHashtable;
using mozilla::dom::Promise;
using mozilla::dom::AutoJSAPI;
using mozilla::Telemetry::HangReports;
using mozilla::Telemetry::CombinedStacks;
using mozilla::Telemetry::ComputeAnnotationsKey;

#if defined(MOZ_GECKO_PROFILER)
using mozilla::Telemetry::KeyedStackCapturer;
#endif

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
  void Observe(Observation& aOb) override;

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
    size_t size = 0;
    size += mFileStats.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (auto iter = mFileStats.ConstIter(); !iter.Done(); iter.Next()) {
      size += iter.Get()->GetKey().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    }
    size += mSafeDirs.ShallowSizeOfExcludingThis(aMallocSizeOf);
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

class TelemetryImpl final
  : public nsITelemetry
  , public nsIMemoryReporter
{
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITELEMETRY
  NS_DECL_NSIMEMORYREPORTER

public:
  void InitMemoryReporter();

  static already_AddRefed<nsITelemetry> CreateTelemetryInstance();
  static void ShutdownTelemetry();
  static void RecordSlowStatement(const nsACString &sql, const nsACString &dbName,
                                  uint32_t delay);
#if defined(MOZ_GECKO_PROFILER)
  static void RecordChromeHang(uint32_t aDuration,
                               Telemetry::ProcessedStack &aStack,
                               int32_t aSystemUptime,
                               int32_t aFirefoxUptime,
                               HangAnnotationsPtr aAnnotations);
#endif
#if defined(MOZ_GECKO_PROFILER)
  static void DoStackCapture(const nsACString& aKey);
#endif
  static void RecordThreadHangStats(Telemetry::ThreadHangStats&& aStats);
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

  static void RecordIceCandidates(const uint32_t iceCandidateBitmask,
                                  const bool success);
  static bool CanRecordBase();
  static bool CanRecordExtended();
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

  void ReadLateWritesStacks(nsIFile* aProfileDir);

  static TelemetryImpl *sTelemetry;
  AutoHashtable<SlowSQLEntryType> mPrivateSQL;
  AutoHashtable<SlowSQLEntryType> mSanitizedSQL;
  Mutex mHashMutex;
  HangReports mHangReports;
  Mutex mHangReportsMutex;
  Atomic<bool> mCanRecordBase;
  Atomic<bool> mCanRecordExtended;

#if defined(MOZ_GECKO_PROFILER)
  // Stores data about stacks captured on demand.
  KeyedStackCapturer mStackCapturer;
#endif

  // mThreadHangStats stores recorded, inactive thread hang stats
  Vector<Telemetry::ThreadHangStats> mThreadHangStats;
  Mutex mThreadHangStatsMutex;

  CombinedStacks mLateWritesStacks; // This is collected out of the main thread.
  bool mCachedTelemetryData;
  uint32_t mLastShutdownTime;
  uint32_t mFailedLockCount;
  nsCOMArray<nsIFetchTelemetryDataCallback> mCallbacks;
  friend class nsFetchTelemetryData;

  WebrtcTelemetry mWebrtcTelemetry;
};

TelemetryImpl*  TelemetryImpl::sTelemetry = nullptr;

MOZ_DEFINE_MALLOC_SIZE_OF(TelemetryMallocSizeOf)

NS_IMETHODIMP
TelemetryImpl::CollectReports(nsIHandleReportCallback* aHandleReport,
                              nsISupports* aData, bool aAnonymize)
{
  MOZ_COLLECT_REPORT(
    "explicit/telemetry", KIND_HEAP, UNITS_BYTES,
    SizeOfIncludingThis(TelemetryMallocSizeOf),
    "Memory used by the telemetry system.");

  return NS_OK;
}

void
InitHistogramRecordingEnabled()
{
  TelemetryHistogram::InitHistogramRecordingEnabled();
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

class nsFetchTelemetryData : public Runnable
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
  RefPtr<TelemetryImpl> mTelemetry;
  nsCOMPtr<nsIFile> mProfileDir;

public:
  void MainThread() {
    mTelemetry->mCachedTelemetryData = true;
    for (unsigned int i = 0, n = mTelemetry->mCallbacks.Count(); i < n; ++i) {
      mTelemetry->mCallbacks[i]->Complete();
    }
    mTelemetry->mCallbacks.Clear();
  }

  NS_IMETHOD Run() override {
    LoadFailedLockCount(mTelemetry->mFailedLockCount);
    mTelemetry->mLastShutdownTime =
      ReadLastShutdownDuration(mShutdownTimeFilename);
    mTelemetry->ReadLateWritesStacks(mProfileDir);
    nsCOMPtr<nsIRunnable> e =
      NewRunnableMethod(this, &nsFetchTelemetryData::MainThread);
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
  if (!Telemetry::CanRecordExtended()) {
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

TelemetryImpl::TelemetryImpl()
  : mHashMutex("Telemetry::mHashMutex")
  , mHangReportsMutex("Telemetry::mHangReportsMutex")
  , mCanRecordBase(false)
  , mCanRecordExtended(false)
  , mThreadHangStatsMutex("Telemetry::mThreadHangStatsMutex")
  , mCachedTelemetryData(false)
  , mLastShutdownTime(0)
  , mFailedLockCount(0)
{
  // We expect TelemetryHistogram::InitializeGlobalState() to have been
  // called before we get to this point.
  MOZ_ASSERT(TelemetryHistogram::GlobalStateHasBeenInitialized());
}

TelemetryImpl::~TelemetryImpl() {
  UnregisterWeakMemoryReporter(this);

  // This is still racey as access to these collections is guarded using sTelemetry.
  // We will fix this in bug 1367344.
  MutexAutoLock hashLock(mHashMutex);
  MutexAutoLock hangReportsLock(mHangReportsMutex);
  MutexAutoLock threadHangsLock(mThreadHangStatsMutex);
}

void
TelemetryImpl::InitMemoryReporter() {
  RegisterWeakMemoryReporter(this);
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
  return (JS_DefineElement(cx, arrayObj, 0, stat->hitCount, JSPROP_ENUMERATE)
          && JS_DefineElement(cx, arrayObj, 1, stat->totalTime, JSPROP_ENUMERATE)
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
  JS::Rooted<JSObject*> statsObj(cx, JS_NewPlainObject(cx));
  if (!statsObj)
    return false;

  AutoHashtable<SlowSQLEntryType>& sqlMap = (privateSQL ? mPrivateSQL : mSanitizedSQL);
  AutoHashtable<SlowSQLEntryType>::ReflectEntryFunc reflectFunction =
    (mainThread ? ReflectMainThreadSQL : ReflectOtherThreadsSQL);
  if (!sqlMap.ReflectIntoJS(reflectFunction, cx, statsObj)) {
    return false;
  }

  return JS_DefineProperty(cx, rootObj,
                           mainThread ? "mainThread" : "otherThreads",
                           statsObj, JSPROP_ENUMERATE);
}

NS_IMETHODIMP
TelemetryImpl::SetHistogramRecordingEnabled(const nsACString &id, bool aEnabled)
{
  return TelemetryHistogram::SetHistogramRecordingEnabled(id, aEnabled);
}

NS_IMETHODIMP
TelemetryImpl::GetHistogramSnapshots(JSContext *cx, JS::MutableHandle<JS::Value> ret)
{
  return TelemetryHistogram::CreateHistogramSnapshots(cx, ret, false, false);
}

NS_IMETHODIMP
TelemetryImpl::SnapshotSubsessionHistograms(bool clearSubsession,
                                            JSContext *cx,
                                            JS::MutableHandle<JS::Value> ret)
{
#if !defined(MOZ_WIDGET_ANDROID)
  return TelemetryHistogram::CreateHistogramSnapshots(cx, ret, true,
                                                      clearSubsession);
#else
  return NS_OK;
#endif
}

NS_IMETHODIMP
TelemetryImpl::GetKeyedHistogramSnapshots(JSContext *cx, JS::MutableHandle<JS::Value> ret)
{
  return TelemetryHistogram::GetKeyedHistogramSnapshots(cx, ret);
}

bool
TelemetryImpl::GetSQLStats(JSContext *cx, JS::MutableHandle<JS::Value> ret, bool includePrivateSql)
{
  JS::Rooted<JSObject*> root_obj(cx, JS_NewPlainObject(cx));
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
TelemetryImpl::GetWebrtcStats(JSContext *cx, JS::MutableHandle<JS::Value> ret)
{
  if (mWebrtcTelemetry.GetWebrtcStats(cx, ret))
    return NS_OK;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelemetryImpl::GetMaximalNumberOfConcurrentThreads(uint32_t *ret)
{
  *ret = nsThreadManager::get().GetHighestNumberOfThreads();
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
  JS::Rooted<JSObject*> annotationsArray(cx, JS_NewArrayObject(cx, 0));
  if (!durationArray || !systemUptimeArray || !firefoxUptimeArray ||
      !annotationsArray) {
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

  ok = JS_DefineProperty(cx, fullReportObj, "annotations", annotationsArray,
                         JSPROP_ENUMERATE);
  if (!ok) {
    return NS_ERROR_FAILURE;
  }


  const size_t length = stacks.GetStackCount();
  for (size_t i = 0; i < length; ++i) {
    if (!JS_DefineElement(cx, durationArray, i, mHangReports.GetDuration(i),
                          JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
    if (!JS_DefineElement(cx, systemUptimeArray, i, mHangReports.GetSystemUptime(i),
                          JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
    if (!JS_DefineElement(cx, firefoxUptimeArray, i, mHangReports.GetFirefoxUptime(i),
                          JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }

    size_t annotationIndex = 0;
    const nsClassHashtable<nsStringHashKey, HangReports::AnnotationInfo>& annotationInfo =
      mHangReports.GetAnnotationInfo();

    for (auto iter = annotationInfo.ConstIter(); !iter.Done(); iter.Next()) {
      const HangReports::AnnotationInfo* info = iter.Data();

      JS::Rooted<JSObject*> keyValueArray(cx, JS_NewArrayObject(cx, 0));
      if (!keyValueArray) {
        return NS_ERROR_FAILURE;
      }

      // Create an array containing all the indices of the chrome hangs relative to this
      // annotation.
      JS::Rooted<JS::Value> indicesArray(cx);
      if (!mozilla::dom::ToJSValue(cx, info->mHangIndices, &indicesArray)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      // We're saving the annotation as [[indices], {annotation-data}], so add the indices
      // array as the first element of that structure.
      if (!JS_DefineElement(cx, keyValueArray, 0, indicesArray, JSPROP_ENUMERATE)) {
        return NS_ERROR_FAILURE;
      }

      // Create the annotations object...
      JS::Rooted<JSObject*> jsAnnotation(cx, JS_NewPlainObject(cx));
      if (!jsAnnotation) {
        return NS_ERROR_FAILURE;
      }
      UniquePtr<HangAnnotations::Enumerator> annotationsEnum =
        info->mAnnotations->GetEnumerator();
      if (!annotationsEnum) {
        return NS_ERROR_FAILURE;
      }

      // ... fill it with key:value pairs...
      nsAutoString key;
      nsAutoString value;
      while (annotationsEnum->Next(key, value)) {
        JS::RootedValue jsValue(cx);
        jsValue.setString(JS_NewUCStringCopyN(cx, value.get(), value.Length()));
        if (!JS_DefineUCProperty(cx, jsAnnotation, key.get(), key.Length(),
                                 jsValue, JSPROP_ENUMERATE)) {
          return NS_ERROR_FAILURE;
        }
      }

      // ... and append it after the indices array.
      if (!JS_DefineElement(cx, keyValueArray, 1, jsAnnotation, JSPROP_ENUMERATE)) {
        return NS_ERROR_FAILURE;
      }
      if (!JS_DefineElement(cx, annotationsArray, annotationIndex++,
                         keyValueArray, JSPROP_ENUMERATE)) {
        return NS_ERROR_FAILURE;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
TelemetryImpl::SnapshotCapturedStacks(bool clear, JSContext *cx, JS::MutableHandle<JS::Value> ret)
{
#if defined(MOZ_GECKO_PROFILER)
  nsresult rv = mStackCapturer.ReflectCapturedStacks(cx, ret);
  if (clear) {
    mStackCapturer.Clear();
  }
  return rv;
#else
  return NS_OK;
#endif
}

#if defined(MOZ_GECKO_PROFILER)
class GetLoadedModulesResultRunnable final : public Runnable
{
  nsMainThreadPtrHandle<Promise> mPromise;
  SharedLibraryInfo mRawModules;
  nsCOMPtr<nsIThread> mWorkerThread;

public:
  GetLoadedModulesResultRunnable(const nsMainThreadPtrHandle<Promise>& aPromise, const SharedLibraryInfo& rawModules)
    : mPromise(aPromise)
    , mRawModules(rawModules)
    , mWorkerThread(do_GetCurrentThread())
  {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  NS_IMETHOD
  Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    mWorkerThread->Shutdown();

    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(mPromise->GlobalJSObject()))) {
      mPromise->MaybeReject(NS_ERROR_FAILURE);
      return NS_OK;
    }

    JSContext* cx = jsapi.cx();

    JS::RootedObject moduleArray(cx, JS_NewArrayObject(cx, 0));
    if (!moduleArray) {
      mPromise->MaybeReject(NS_ERROR_FAILURE);
      return NS_OK;
    }

    for (unsigned int i = 0, n = mRawModules.GetSize(); i != n; i++) {
      const SharedLibrary &info = mRawModules.GetEntry(i);

      JS::RootedObject moduleObj(cx, JS_NewPlainObject(cx));
      if (!moduleObj) {
        mPromise->MaybeReject(NS_ERROR_FAILURE);
        return NS_OK;
      }

      // Module name.
      JS::RootedString moduleName(cx, JS_NewUCStringCopyZ(cx, info.GetModuleName().get()));
      if (!moduleName || !JS_DefineProperty(cx, moduleObj, "name", moduleName, JSPROP_ENUMERATE)) {
        mPromise->MaybeReject(NS_ERROR_FAILURE);
        return NS_OK;
      }

      // Module debug name.
      JS::RootedValue moduleDebugName(cx);

      if (!info.GetDebugName().IsEmpty()) {
        JS::RootedString str_moduleDebugName(cx, JS_NewUCStringCopyZ(cx, info.GetDebugName().get()));
        if (!str_moduleDebugName) {
          mPromise->MaybeReject(NS_ERROR_FAILURE);
          return NS_OK;
        }
        moduleDebugName.setString(str_moduleDebugName);
      }
      else {
        moduleDebugName.setNull();
      }

      if (!JS_DefineProperty(cx, moduleObj, "debugName", moduleDebugName, JSPROP_ENUMERATE)) {
        mPromise->MaybeReject(NS_ERROR_FAILURE);
        return NS_OK;
      }

      // Module Breakpad identifier.
      JS::RootedValue id(cx);

      if (!info.GetBreakpadId().empty()) {
        JS::RootedString str_id(cx, JS_NewStringCopyZ(cx, info.GetBreakpadId().c_str()));
        if (!str_id) {
          mPromise->MaybeReject(NS_ERROR_FAILURE);
          return NS_OK;
        }
        id.setString(str_id);
      } else {
        id.setNull();
      }

      if (!JS_DefineProperty(cx, moduleObj, "debugID", id, JSPROP_ENUMERATE)) {
        mPromise->MaybeReject(NS_ERROR_FAILURE);
        return NS_OK;
      }

      // Module version.
      JS::RootedValue version(cx);

      if (!info.GetVersion().empty()) {
        JS::RootedString v(cx, JS_NewStringCopyZ(cx, info.GetVersion().c_str()));
        if (!v) {
          mPromise->MaybeReject(NS_ERROR_FAILURE);
          return NS_OK;
        }
        version.setString(v);
      } else {
        version.setNull();
      }

      if (!JS_DefineProperty(cx, moduleObj, "version", version, JSPROP_ENUMERATE)) {
        mPromise->MaybeReject(NS_ERROR_FAILURE);
        return NS_OK;
      }

      if (!JS_DefineElement(cx, moduleArray, i, moduleObj, JSPROP_ENUMERATE)) {
        mPromise->MaybeReject(NS_ERROR_FAILURE);
        return NS_OK;
      }
    }

    mPromise->MaybeResolve(moduleArray);
    return NS_OK;
  }
};

class GetLoadedModulesRunnable final : public Runnable
{
  nsMainThreadPtrHandle<Promise> mPromise;

public:
  explicit GetLoadedModulesRunnable(const nsMainThreadPtrHandle<Promise>& aPromise)
    : mPromise(aPromise)
  { }

  NS_IMETHOD
  Run() override
  {
    nsCOMPtr<nsIRunnable> resultRunnable = new GetLoadedModulesResultRunnable(mPromise, SharedLibraryInfo::GetInfoForSelf());
    return NS_DispatchToMainThread(resultRunnable);
  }
};
#endif // MOZ_GECKO_PROFILER

NS_IMETHODIMP
TelemetryImpl::GetLoadedModules(JSContext *cx, nsISupports** aPromise)
{
#if defined(MOZ_GECKO_PROFILER)
  nsIGlobalObject* global = xpc::NativeGlobal(JS::CurrentGlobalOrNull(cx));
  if (NS_WARN_IF(!global)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(global, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  nsCOMPtr<nsIThreadManager> tm = do_GetService(NS_THREADMANAGER_CONTRACTID);
  nsCOMPtr<nsIThread> getModulesThread;
  nsresult rv = tm->NewThread(0, 0, getter_AddRefs(getModulesThread));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    promise->MaybeReject(NS_ERROR_FAILURE);
    return NS_OK;
  }

  nsMainThreadPtrHandle<Promise> mainThreadPromise(
    new nsMainThreadPtrHolder<Promise>("Promise", promise));
  nsCOMPtr<nsIRunnable> runnable = new GetLoadedModulesRunnable(mainThreadPromise);
  promise.forget(aPromise);

  return getModulesThread->Dispatch(runnable, nsIEventTarget::DISPATCH_NORMAL);
#else // MOZ_GECKO_PROFILER
  return NS_ERROR_NOT_IMPLEMENTED;
#endif // MOZ_GECKO_PROFILER
}

static bool
IsValidBreakpadId(const std::string &breakpadId)
{
  if (breakpadId.size() < 33) {
    return false;
  }
  for (char c : breakpadId) {
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
      NS_ConvertUTF8toUTF16(moduleName.c_str()),
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
  JS::RootedObject ret(cx, JS_NewPlainObject(cx));
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
  // TODO: calculate "sum"
  if (!JS_DefineProperty(cx, ret, "sum", 0, JSPROP_ENUMERATE)) {
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
  if (!JS_DefineElement(cx, ranges, 0, time.GetBucketMin(0), JSPROP_ENUMERATE) ||
      !JS_DefineElement(cx, counts, 0, 0, JSPROP_ENUMERATE)) {
    return nullptr;
  }
  for (size_t i = 0; i < ArrayLength(time); i++) {
    if (!JS_DefineElement(cx, ranges, i + 1, time.GetBucketMax(i),
                          JSPROP_ENUMERATE) ||
        !JS_DefineElement(cx, counts, i + 1, time[i], JSPROP_ENUMERATE)) {
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
    if (!JS_DefineElement(cx, ret, i, string, JSPROP_ENUMERATE)) {
      return nullptr;
    }
  }
  return ret;
}

static void
CreateJSHangAnnotations(JSContext* cx, const HangAnnotationsVector& annotations,
                        JS::MutableHandleObject returnedObject)
{
  JS::RootedObject annotationsArray(cx, JS_NewArrayObject(cx, 0));
  if (!annotationsArray) {
    returnedObject.set(nullptr);
    return;
  }
  // We keep track of the annotations we reported in this hash set, so we can
  // discard duplicated ones.
  nsTHashtable<nsStringHashKey> reportedAnnotations;
  size_t annotationIndex = 0;
  for (const auto & curAnnotations : annotations) {
    JS::RootedObject jsAnnotation(cx, JS_NewPlainObject(cx));
    if (!jsAnnotation) {
      continue;
    }
    // Build a key to index the current annotations in our hash set.
    nsAutoString annotationsKey;
    nsresult rv = ComputeAnnotationsKey(curAnnotations, annotationsKey);
    if (NS_FAILED(rv)) {
      continue;
    }
    // Check if the annotations are in the set. If that's the case, don't double report.
    if (reportedAnnotations.GetEntry(annotationsKey)) {
      continue;
    }
    // If not, report them.
    reportedAnnotations.PutEntry(annotationsKey);
    UniquePtr<HangAnnotations::Enumerator> annotationsEnum =
      curAnnotations->GetEnumerator();
    if (!annotationsEnum) {
      continue;
    }
    nsAutoString key;
    nsAutoString value;
    while (annotationsEnum->Next(key, value)) {
      JS::RootedValue jsValue(cx);
      jsValue.setString(JS_NewUCStringCopyN(cx, value.get(), value.Length()));
      if (!JS_DefineUCProperty(cx, jsAnnotation, key.get(), key.Length(),
                               jsValue, JSPROP_ENUMERATE)) {
        returnedObject.set(nullptr);
        return;
      }
    }
    if (!JS_SetElement(cx, annotationsArray, annotationIndex, jsAnnotation)) {
      continue;
    }
    ++annotationIndex;
  }
  // Return the array using a |MutableHandleObject| to avoid triggering a false
  // positive rooting issue in the hazard analysis build.
  returnedObject.set(annotationsArray);
}

static JSObject*
CreateJSHangHistogram(JSContext* cx, const Telemetry::HangHistogram& hang)
{
  JS::RootedObject ret(cx, JS_NewPlainObject(cx));
  if (!ret) {
    return nullptr;
  }

  JS::RootedObject stack(cx, CreateJSHangStack(cx, hang.GetStack()));
  JS::RootedObject time(cx, CreateJSTimeHistogram(cx, hang));
  auto& hangAnnotations = hang.GetAnnotations();
  JS::RootedObject annotations(cx);
  CreateJSHangAnnotations(cx, hangAnnotations, &annotations);

  if (!stack ||
      !time ||
      !annotations ||
      !JS_DefineProperty(cx, ret, "stack", stack, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, ret, "histogram", time, JSPROP_ENUMERATE) ||
      (!hangAnnotations.empty() && // <-- Only define annotations when nonempty
        !JS_DefineProperty(cx, ret, "annotations", annotations, JSPROP_ENUMERATE))) {
    return nullptr;
  }

  return ret;
}

static JSObject*
CreateJSThreadHangStats(JSContext* cx, const Telemetry::ThreadHangStats& thread)
{
  JS::RootedObject ret(cx, JS_NewPlainObject(cx));
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

  // Process the hangs into a hangs object.
  JS::RootedObject hangs(cx, JS_NewArrayObject(cx, 0));
  if (!hangs) {
    return nullptr;
  }
  for (size_t i = 0; i < thread.mHangs.length(); i++) {
    JS::RootedObject obj(cx, CreateJSHangHistogram(cx, thread.mHangs[i]));
    if (!ret) {
      return nullptr;
    }

    // Check if we have a cached native stack index, and if we do record it.
    uint32_t index = thread.mHangs[i].GetNativeStackIndex();
    if (index != Telemetry::HangHistogram::NO_NATIVE_STACK_INDEX) {
      if (!JS_DefineProperty(cx, obj, "nativeStack", index, JSPROP_ENUMERATE)) {
        return nullptr;
      }
    }

    if (!JS_DefineElement(cx, hangs, i, obj, JSPROP_ENUMERATE)) {
      return nullptr;
    }
  }
  if (!JS_DefineProperty(cx, ret, "hangs", hangs, JSPROP_ENUMERATE)) {
    return nullptr;
  }

  // We should already have a CombinedStacks object on the ThreadHangStats, so
  // add that one.
  JS::RootedObject fullReportObj(cx, CreateJSStackObject(cx, thread.mCombinedStacks));
  if (!fullReportObj) {
    return nullptr;
  }

  if (!JS_DefineProperty(cx, ret, "nativeStacks", fullReportObj, JSPROP_ENUMERATE)) {
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

  if (!BackgroundHangMonitor::IsDisabled()) {
    /* First add active threads; we need to hold |iter| (and its lock)
       throughout this method to avoid a race condition where a thread can
       be recorded twice if the thread is destroyed while this method is
       running */
    BackgroundHangMonitor::ThreadHangStatsIterator iter;
    for (Telemetry::ThreadHangStats* histogram = iter.GetNext();
         histogram; histogram = iter.GetNext()) {
      JS::RootedObject obj(cx, CreateJSThreadHangStats(cx, *histogram));
      if (!JS_DefineElement(cx, retObj, threadIndex++, obj, JSPROP_ENUMERATE)) {
        return NS_ERROR_FAILURE;
      }
    }
  }

  // Add saved threads next
  MutexAutoLock autoLock(mThreadHangStatsMutex);
  for (auto & stat : mThreadHangStats) {
    JS::RootedObject obj(cx,
      CreateJSThreadHangStats(cx, stat));
    if (!JS_DefineElement(cx, retObj, threadIndex++, obj, JSPROP_ENUMERATE)) {
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

NS_IMETHODIMP
TelemetryImpl::RegisteredHistograms(uint32_t aDataset, uint32_t *aCount,
                                    char*** aHistograms)
{
  return
    TelemetryHistogram::RegisteredHistograms(aDataset, aCount, aHistograms);
}

NS_IMETHODIMP
TelemetryImpl::RegisteredKeyedHistograms(uint32_t aDataset, uint32_t *aCount,
                                         char*** aHistograms)
{
  return
    TelemetryHistogram::RegisteredKeyedHistograms(aDataset, aCount,
                                                  aHistograms);
}

NS_IMETHODIMP
TelemetryImpl::GetHistogramById(const nsACString &name, JSContext *cx,
                                JS::MutableHandle<JS::Value> ret)
{
  return TelemetryHistogram::GetHistogramById(name, cx, ret);
}

NS_IMETHODIMP
TelemetryImpl::GetKeyedHistogramById(const nsACString &name, JSContext *cx,
                                     JS::MutableHandle<JS::Value> ret)
{
  return TelemetryHistogram::GetKeyedHistogramById(name, cx, ret);
}

/**
 * Indicates if Telemetry can record base data (FHR data). This is true if the
 * FHR data reporting service or self-support are enabled.
 *
 * In the unlikely event that adding a new base probe is needed, please check the data
 * collection wiki at https://wiki.mozilla.org/Firefox/Data_Collection and talk to the
 * Telemetry team.
 */
NS_IMETHODIMP
TelemetryImpl::GetCanRecordBase(bool *ret) {
  *ret = mCanRecordBase;
  return NS_OK;
}

NS_IMETHODIMP
TelemetryImpl::SetCanRecordBase(bool canRecord) {
  if (canRecord != mCanRecordBase) {
    TelemetryHistogram::SetCanRecordBase(canRecord);
    TelemetryScalar::SetCanRecordBase(canRecord);
    TelemetryEvent::SetCanRecordBase(canRecord);
    mCanRecordBase = canRecord;
  }
  return NS_OK;
}

/**
 * Indicates if Telemetry is allowed to record extended data. Returns false if the user
 * hasn't opted into "extended Telemetry" on the Release channel, when the user has
 * explicitly opted out of Telemetry on Nightly/Aurora/Beta or if manually set to false
 * during tests.
 * If the returned value is false, gathering of extended telemetry statistics is disabled.
 */
NS_IMETHODIMP
TelemetryImpl::GetCanRecordExtended(bool *ret) {
  *ret = mCanRecordExtended;
  return NS_OK;
}

NS_IMETHODIMP
TelemetryImpl::SetCanRecordExtended(bool canRecord) {
  if (canRecord != mCanRecordExtended) {
    TelemetryHistogram::SetCanRecordExtended(canRecord);
    TelemetryScalar::SetCanRecordExtended(canRecord);
    TelemetryEvent::SetCanRecordExtended(canRecord);
    mCanRecordExtended = canRecord;
  }
  return NS_OK;
}


NS_IMETHODIMP
TelemetryImpl::GetIsOfficialTelemetry(bool *ret) {
#if defined(MOZILLA_OFFICIAL) && defined(MOZ_TELEMETRY_REPORTING) && !defined(DEBUG)
  *ret = true;
#else
  *ret = false;
#endif
  return NS_OK;
}

already_AddRefed<nsITelemetry>
TelemetryImpl::CreateTelemetryInstance()
{
  MOZ_ASSERT(sTelemetry == nullptr, "CreateTelemetryInstance may only be called once, via GetService()");

  bool useTelemetry = false;
  if (XRE_IsParentProcess() ||
      XRE_IsContentProcess() ||
      XRE_IsGPUProcess())
  {
    useTelemetry = true;
  }

  // First, initialize the TelemetryHistogram and TelemetryScalar global states.
  TelemetryHistogram::InitializeGlobalState(useTelemetry, useTelemetry);
  TelemetryScalar::InitializeGlobalState(useTelemetry, useTelemetry);

  // Only record events from the parent process.
  TelemetryEvent::InitializeGlobalState(XRE_IsParentProcess(), XRE_IsParentProcess());

  // Now, create and initialize the Telemetry global state.
  sTelemetry = new TelemetryImpl();

  // AddRef for the local reference
  NS_ADDREF(sTelemetry);
  // AddRef for the caller
  nsCOMPtr<nsITelemetry> ret = sTelemetry;

  sTelemetry->mCanRecordBase = useTelemetry;
  sTelemetry->mCanRecordExtended = useTelemetry;

  sTelemetry->InitMemoryReporter();
  InitHistogramRecordingEnabled(); // requires sTelemetry to exist

  return ret.forget();
}

void
TelemetryImpl::ShutdownTelemetry()
{
  // No point in collecting IO beyond this point
  ClearIOReporting();
  NS_IF_RELEASE(sTelemetry);

  // Lastly, de-initialise the TelemetryHistogram and TelemetryScalar global states,
  // so as to release any heap storage that would otherwise be kept alive by it.
  TelemetryHistogram::DeInitializeGlobalState();
  TelemetryScalar::DeInitializeGlobalState();
  TelemetryEvent::DeInitializeGlobalState();
  TelemetryIPCAccumulator::DeInitializeGlobalState();
}

void
TelemetryImpl::StoreSlowSQL(const nsACString &sql, uint32_t delay,
                            SanitizedState state)
{
  AutoHashtable<SlowSQLEntryType>* slowSQLMap = nullptr;
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

// A whitelist mechanism to prevent Telemetry reporting on Addon & Thunderbird
// DBs.
struct TrackedDBEntry
{
  const char* mName;
  const uint32_t mNameLength;

  // This struct isn't meant to be used beyond the static arrays below.
  constexpr
  TrackedDBEntry(const char* aName, uint32_t aNameLength)
    : mName(aName)
    , mNameLength(aNameLength)
  { }

  TrackedDBEntry() = delete;
  TrackedDBEntry(TrackedDBEntry&) = delete;
};

#define TRACKEDDB_ENTRY(_name) { _name, (sizeof(_name) - 1) }

// A whitelist of database names. If the database name exactly matches one of
// these then its SQL statements will always be recorded.
static constexpr TrackedDBEntry kTrackedDBs[] = {
  // IndexedDB for about:home, see aboutHome.js
  TRACKEDDB_ENTRY("818200132aebmoouht.sqlite"),
  TRACKEDDB_ENTRY("addons.sqlite"),
  TRACKEDDB_ENTRY("content-prefs.sqlite"),
  TRACKEDDB_ENTRY("cookies.sqlite"),
  TRACKEDDB_ENTRY("extensions.sqlite"),
  TRACKEDDB_ENTRY("favicons.sqlite"),
  TRACKEDDB_ENTRY("formhistory.sqlite"),
  TRACKEDDB_ENTRY("index.sqlite"),
  TRACKEDDB_ENTRY("netpredictions.sqlite"),
  TRACKEDDB_ENTRY("permissions.sqlite"),
  TRACKEDDB_ENTRY("places.sqlite"),
  TRACKEDDB_ENTRY("reading-list.sqlite"),
  TRACKEDDB_ENTRY("search.sqlite"),
  TRACKEDDB_ENTRY("signons.sqlite"),
  TRACKEDDB_ENTRY("urlclassifier3.sqlite"),
  TRACKEDDB_ENTRY("webappsstore.sqlite")
};

// A whitelist of database name prefixes. If the database name begins with
// one of these prefixes then its SQL statements will always be recorded.
static const TrackedDBEntry kTrackedDBPrefixes[] = {
  TRACKEDDB_ENTRY("indexedDB-")
};

#undef TRACKEDDB_ENTRY

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
  MOZ_ASSERT(!sql.IsEmpty());
  MOZ_ASSERT(!dbName.IsEmpty());

  if (!sTelemetry || !TelemetryHistogram::CanRecordExtended())
    return;

  bool recordStatement = false;

  for (const TrackedDBEntry& nameEntry : kTrackedDBs) {
    MOZ_ASSERT(nameEntry.mNameLength);
    const nsDependentCString name(nameEntry.mName, nameEntry.mNameLength);
    if (dbName == name) {
      recordStatement = true;
      break;
    }
  }

  if (!recordStatement) {
    for (const TrackedDBEntry& prefixEntry : kTrackedDBPrefixes) {
      MOZ_ASSERT(prefixEntry.mNameLength);
      const nsDependentCString prefix(prefixEntry.mName,
                                      prefixEntry.mNameLength);
      if (StringBeginsWith(dbName, prefix)) {
        recordStatement = true;
        break;
      }
    }
  }

  if (recordStatement) {
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

void
TelemetryImpl::RecordIceCandidates(const uint32_t iceCandidateBitmask,
                                   const bool success)
{
  if (!sTelemetry || !TelemetryHistogram::CanRecordExtended())
    return;

  sTelemetry->mWebrtcTelemetry.RecordIceCandidateMask(iceCandidateBitmask, success);
}

#if defined(MOZ_GECKO_PROFILER)
void
TelemetryImpl::RecordChromeHang(uint32_t aDuration,
                                Telemetry::ProcessedStack &aStack,
                                int32_t aSystemUptime,
                                int32_t aFirefoxUptime,
                                HangAnnotationsPtr aAnnotations)
{
  if (!sTelemetry || !TelemetryHistogram::CanRecordExtended())
    return;

  HangAnnotationsPtr annotations;
  // We only pass aAnnotations if it is not empty.
  if (aAnnotations && !aAnnotations->IsEmpty()) {
    annotations = Move(aAnnotations);
  }

  MutexAutoLock hangReportMutex(sTelemetry->mHangReportsMutex);

  sTelemetry->mHangReports.AddHang(aStack, aDuration,
                                   aSystemUptime, aFirefoxUptime,
                                   Move(annotations));
}

#if defined(MOZ_GECKO_PROFILER)
void
TelemetryImpl::DoStackCapture(const nsACString& aKey) {
  if (Telemetry::CanRecordExtended() && XRE_IsParentProcess()) {
    sTelemetry->mStackCapturer.Capture(aKey);
  }
}
#endif
#endif

nsresult
TelemetryImpl::CaptureStack(const nsACString& aKey) {
#if defined(MOZ_GECKO_PROFILER)
  TelemetryImpl::DoStackCapture(aKey);
#endif
  return NS_OK;
}

void
TelemetryImpl::RecordThreadHangStats(Telemetry::ThreadHangStats&& aStats)
{
  if (!sTelemetry || !TelemetryHistogram::CanRecordExtended())
    return;

  MutexAutoLock autoLock(sTelemetry->mThreadHangStatsMutex);

  // Ignore OOM.
  mozilla::Unused << sTelemetry->mThreadHangStats.append(Move(aStats));
}

bool
TelemetryImpl::CanRecordBase()
{
  if (!sTelemetry) {
    return false;
  }
  bool canRecordBase;
  nsresult rv = sTelemetry->GetCanRecordBase(&canRecordBase);
  return NS_SUCCEEDED(rv) && canRecordBase;
}

bool
TelemetryImpl::CanRecordExtended()
{
  if (!sTelemetry) {
    return false;
  }
  bool canRecordExtended;
  nsresult rv = sTelemetry->GetCanRecordExtended(&canRecordExtended);
  return NS_SUCCEEDED(rv) && canRecordExtended;
}

NS_IMPL_ISUPPORTS(TelemetryImpl, nsITelemetry, nsIMemoryReporter)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsITelemetry, TelemetryImpl::CreateTelemetryInstance)

#define NS_TELEMETRY_CID \
  {0xaea477f2, 0xb3a2, 0x469c, {0xaa, 0x29, 0x0a, 0x82, 0xd1, 0x32, 0xb8, 0x29}}
NS_DEFINE_NAMED_CID(NS_TELEMETRY_CID);

const Module::CIDEntry kTelemetryCIDs[] = {
  { &kNS_TELEMETRY_CID, false, nullptr, nsITelemetryConstructor, Module::ALLOW_IN_GPU_PROCESS },
  { nullptr }
};

const Module::ContractIDEntry kTelemetryContracts[] = {
  { "@mozilla.org/base/telemetry;1", &kNS_TELEMETRY_CID, Module::ALLOW_IN_GPU_PROCESS },
  { nullptr }
};

const Module kTelemetryModule = {
  Module::kVersion,
  kTelemetryCIDs,
  kTelemetryContracts,
  nullptr,
  nullptr,
  nullptr,
  TelemetryImpl::ShutdownTelemetry,
  Module::ALLOW_IN_GPU_PROCESS
};

NS_IMETHODIMP
TelemetryImpl::GetFileIOReports(JSContext *cx, JS::MutableHandleValue ret)
{
  if (sTelemetryIOObserver) {
    JS::Rooted<JSObject*> obj(cx, JS_NewPlainObject(cx));
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
  return Telemetry::Common::MsSinceProcessStart(aResult);
}

NS_IMETHODIMP
TelemetryImpl::MsSystemNow(double* aResult)
{
#ifdef XP_LINUX
  timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  *aResult = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#else
  using namespace std::chrono;
  milliseconds ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
  *aResult = static_cast<double>(ms.count());
#endif // XP_LINUX

  return NS_OK;
}

// Telemetry Scalars IDL Implementation

NS_IMETHODIMP
TelemetryImpl::ScalarAdd(const nsACString& aName, JS::HandleValue aVal, JSContext* aCx)
{
  return TelemetryScalar::Add(aName, aVal, aCx);
}

NS_IMETHODIMP
TelemetryImpl::ScalarSet(const nsACString& aName, JS::HandleValue aVal, JSContext* aCx)
{
  return TelemetryScalar::Set(aName, aVal, aCx);
}

NS_IMETHODIMP
TelemetryImpl::ScalarSetMaximum(const nsACString& aName, JS::HandleValue aVal, JSContext* aCx)
{
  return TelemetryScalar::SetMaximum(aName, aVal, aCx);
}

NS_IMETHODIMP
TelemetryImpl::SnapshotScalars(unsigned int aDataset, bool aClearScalars, JSContext* aCx,
                               uint8_t optional_argc, JS::MutableHandleValue aResult)
{
  return TelemetryScalar::CreateSnapshots(aDataset, aClearScalars, aCx, optional_argc, aResult);
}

NS_IMETHODIMP
TelemetryImpl::KeyedScalarAdd(const nsACString& aName, const nsAString& aKey,
                              JS::HandleValue aVal, JSContext* aCx)
{
  return TelemetryScalar::Add(aName, aKey, aVal, aCx);
}

NS_IMETHODIMP
TelemetryImpl::KeyedScalarSet(const nsACString& aName, const nsAString& aKey,
                              JS::HandleValue aVal, JSContext* aCx)
{
  return TelemetryScalar::Set(aName, aKey, aVal, aCx);
}

NS_IMETHODIMP
TelemetryImpl::KeyedScalarSetMaximum(const nsACString& aName, const nsAString& aKey,
                              JS::HandleValue aVal, JSContext* aCx)
{
  return TelemetryScalar::SetMaximum(aName, aKey, aVal, aCx);
}

NS_IMETHODIMP
TelemetryImpl::SnapshotKeyedScalars(unsigned int aDataset, bool aClearScalars, JSContext* aCx,
                                    uint8_t optional_argc, JS::MutableHandleValue aResult)
{
  return TelemetryScalar::CreateKeyedSnapshots(aDataset, aClearScalars, aCx, optional_argc,
                                               aResult);
}

NS_IMETHODIMP
TelemetryImpl::ClearScalars()
{
  TelemetryScalar::ClearScalars();
  return NS_OK;
}

// Telemetry Event IDL implementation.

NS_IMETHODIMP
TelemetryImpl::RecordEvent(const nsACString & aCategory, const nsACString & aMethod,
                           const nsACString & aObject, JS::HandleValue aValue,
                           JS::HandleValue aExtra, JSContext* aCx, uint8_t optional_argc)
{
  return TelemetryEvent::RecordEvent(aCategory, aMethod, aObject, aValue, aExtra, aCx, optional_argc);
}

NS_IMETHODIMP
TelemetryImpl::SnapshotBuiltinEvents(uint32_t aDataset, bool aClear, JSContext* aCx,
                                     uint8_t optional_argc, JS::MutableHandleValue aResult)
{
  return TelemetryEvent::CreateSnapshots(aDataset, aClear, aCx, optional_argc, aResult);
}

NS_IMETHODIMP
TelemetryImpl::ClearEvents()
{
  TelemetryEvent::ClearEvents();
  return NS_OK;
}

NS_IMETHODIMP
TelemetryImpl::SetEventRecordingEnabled(const nsACString& aCategory, bool aEnabled)
{
  TelemetryEvent::SetEventRecordingEnabled(aCategory, aEnabled);
  return NS_OK;
}

NS_IMETHODIMP
TelemetryImpl::FlushBatchedChildTelemetry()
{
  TelemetryIPCAccumulator::IPCTimerFired(nullptr, nullptr);
  return NS_OK;
}

size_t
TelemetryImpl::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
{
  size_t n = aMallocSizeOf(this);

  n += TelemetryHistogram::GetMapShallowSizesOfExcludingThis(aMallocSizeOf);
  n += TelemetryScalar::GetMapShallowSizesOfExcludingThis(aMallocSizeOf);
  n += mWebrtcTelemetry.SizeOfExcludingThis(aMallocSizeOf);
  { // Scope for mHashMutex lock
    MutexAutoLock lock(mHashMutex);
    n += mPrivateSQL.SizeOfExcludingThis(aMallocSizeOf);
    n += mSanitizedSQL.SizeOfExcludingThis(aMallocSizeOf);
  }
  { // Scope for mHangReportsMutex lock
    MutexAutoLock lock(mHangReportsMutex);
    n += mHangReports.SizeOfExcludingThis(aMallocSizeOf);
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

  n += TelemetryHistogram::GetHistogramSizesofIncludingThis(aMallocSizeOf);
  n += TelemetryScalar::GetScalarSizesOfIncludingThis(aMallocSizeOf);
  n += TelemetryEvent::SizeOfIncludingThis(aMallocSizeOf);

  return n;
}

struct StackFrame
{
  uintptr_t mPC;      // The program counter at this position in the call stack.
  uint16_t mIndex;    // The number of this frame in the call stack.
  uint16_t mModIndex; // The index of module that has this program counter.
};

#ifdef MOZ_GECKO_PROFILER
static bool CompareByPC(const StackFrame &a, const StackFrame &b)
{
  return a.mPC < b.mPC;
}

static bool CompareByIndex(const StackFrame &a, const StackFrame &b)
{
  return a.mIndex < b.mIndex;
}
#endif

} // namespace


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// EXTERNALLY VISIBLE FUNCTIONS in no name space
// These are NOT listed in Telemetry.h

NSMODULE_DEFN(nsTelemetryModule) = &kTelemetryModule;

/**
 * The XRE_TelemetryAdd function is to be used by embedding applications
 * that can't use mozilla::Telemetry::Accumulate() directly.
 */
void
XRE_TelemetryAccumulate(int aID, uint32_t aSample)
{
  mozilla::Telemetry::Accumulate((mozilla::Telemetry::HistogramID) aID, aSample);
}


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// EXTERNALLY VISIBLE FUNCTIONS in mozilla::
// These are NOT listed in Telemetry.h

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

  if (!Telemetry::CanRecordExtended())
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

  if (gRecordedShutdownStartTime.IsNull()) {
    // If |CanRecordExtended()| is true before |AsyncFetchTelemetryData| is called and
    // then disabled before shutdown, |RecordShutdownStartTimeStamp| will bail out and
    // we will end up with a null |gRecordedShutdownStartTime| here. This can happen
    // during tests.
    return;
  }

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

} // namespace mozilla


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// EXTERNALLY VISIBLE FUNCTIONS in mozilla::Telemetry::
// These are NOT listed in Telemetry.h

namespace mozilla {
namespace Telemetry {

const size_t kMaxChromeStackDepth = 50;

ProcessedStack::ProcessedStack() = default;

size_t ProcessedStack::GetStackSize() const
{
  return mStack.size();
}

size_t ProcessedStack::GetNumModules() const
{
  return mModules.size();
}

bool ProcessedStack::Module::operator==(const Module& aOther) const {
  return  mName == aOther.mName &&
    mBreakpadId == aOther.mBreakpadId;
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

ProcessedStack
GetStackAndModules(const std::vector<uintptr_t>& aPCs)
{
  std::vector<StackFrame> rawStack;
  auto stackEnd = aPCs.begin() + std::min(aPCs.size(), kMaxChromeStackDepth);
  for (auto i = aPCs.begin(); i != stackEnd; ++i) {
    uintptr_t aPC = *i;
    StackFrame Frame = {aPC, static_cast<uint16_t>(rawStack.size()),
                        std::numeric_limits<uint16_t>::max()};
    rawStack.push_back(Frame);
  }

#ifdef MOZ_GECKO_PROFILER
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
  for (auto & rawFrame : rawStack) {
    mozilla::Telemetry::ProcessedStack::Frame frame = { rawFrame.mPC, rawFrame.mModIndex };
    Ret.AddFrame(frame);
  }

#ifdef MOZ_GECKO_PROFILER
  for (unsigned i = 0, n = rawModules.GetSize(); i != n; ++i) {
    const SharedLibrary &info = rawModules.GetEntry(i);
    mozilla::Telemetry::ProcessedStack::Module module = {
      info.GetDebugName(),
      info.GetBreakpadId()
    };
    Ret.AddModule(module);
  }
#endif

  return Ret;
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
    for (auto & entry : *this) {
      if (entry >= prevStart && entry < prevEnd) {
        // Move from old buffer to new buffer.
        entry += mBuffer.begin() - prevStart;
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


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// EXTERNALLY VISIBLE FUNCTIONS in mozilla::Telemetry::
// These are listed in Telemetry.h

namespace mozilla {
namespace Telemetry {

// The external API for controlling recording state
void
SetHistogramRecordingEnabled(HistogramID aID, bool aEnabled)
{
  TelemetryHistogram::SetHistogramRecordingEnabled(aID, aEnabled);
}

void
Accumulate(HistogramID aHistogram, uint32_t aSample)
{
  TelemetryHistogram::Accumulate(aHistogram, aSample);
}

void
Accumulate(HistogramID aID, const nsCString& aKey, uint32_t aSample)
{
  TelemetryHistogram::Accumulate(aID, aKey, aSample);
}

void
Accumulate(const char* name, uint32_t sample)
{
  TelemetryHistogram::Accumulate(name, sample);
}

void
Accumulate(const char *name, const nsCString& key, uint32_t sample)
{
  TelemetryHistogram::Accumulate(name, key, sample);
}

void
AccumulateCategorical(HistogramID id, const nsCString& label)
{
  TelemetryHistogram::AccumulateCategorical(id, label);
}

void
AccumulateTimeDelta(HistogramID aHistogram, TimeStamp start, TimeStamp end)
{
  Accumulate(aHistogram,
             static_cast<uint32_t>((end - start).ToMilliseconds()));
}

const char*
GetHistogramName(HistogramID id)
{
  return TelemetryHistogram::GetHistogramName(id);
}

bool
CanRecordBase()
{
  return TelemetryImpl::CanRecordBase();
}

bool
CanRecordExtended()
{
  return TelemetryImpl::CanRecordExtended();
}

void
RecordSlowSQLStatement(const nsACString &statement,
                       const nsACString &dbName,
                       uint32_t delay)
{
  TelemetryImpl::RecordSlowStatement(statement, dbName, delay);
}

void
RecordWebrtcIceCandidates(const uint32_t iceCandidateBitmask,
                          const bool success)
{
  TelemetryImpl::RecordIceCandidates(iceCandidateBitmask, success);
}

void Init()
{
  // Make the service manager hold a long-lived reference to the service
  nsCOMPtr<nsITelemetry> telemetryService =
    do_GetService("@mozilla.org/base/telemetry;1");
  MOZ_ASSERT(telemetryService);
}

#if defined(MOZ_GECKO_PROFILER)
void RecordChromeHang(uint32_t duration,
                      ProcessedStack &aStack,
                      int32_t aSystemUptime,
                      int32_t aFirefoxUptime,
                      HangAnnotationsPtr aAnnotations)
{
  TelemetryImpl::RecordChromeHang(duration, aStack,
                                  aSystemUptime, aFirefoxUptime,
                                  Move(aAnnotations));
}

void CaptureStack(const nsACString& aKey)
{
#if defined(MOZ_GECKO_PROFILER)
  TelemetryImpl::DoStackCapture(aKey);
#endif
}
#endif

void RecordThreadHangStats(ThreadHangStats&& aStats)
{
  TelemetryImpl::RecordThreadHangStats(Move(aStats));
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

void CreateStatisticsRecorder()
{
  TelemetryHistogram::CreateStatisticsRecorder();
}

void DestroyStatisticsRecorder()
{
  TelemetryHistogram::DestroyStatisticsRecorder();
}

// Scalar API C++ Endpoints

void
ScalarAdd(mozilla::Telemetry::ScalarID aId, uint32_t aVal)
{
  TelemetryScalar::Add(aId, aVal);
}

void
ScalarSet(mozilla::Telemetry::ScalarID aId, uint32_t aVal)
{
  TelemetryScalar::Set(aId, aVal);
}

void
ScalarSet(mozilla::Telemetry::ScalarID aId, bool aVal)
{
  TelemetryScalar::Set(aId, aVal);
}

void
ScalarSet(mozilla::Telemetry::ScalarID aId, const nsAString& aVal)
{
  TelemetryScalar::Set(aId, aVal);
}

void
ScalarSetMaximum(mozilla::Telemetry::ScalarID aId, uint32_t aVal)
{
  TelemetryScalar::SetMaximum(aId, aVal);
}

void
ScalarAdd(mozilla::Telemetry::ScalarID aId, const nsAString& aKey, uint32_t aVal)
{
  TelemetryScalar::Add(aId, aKey, aVal);
}

void
ScalarSet(mozilla::Telemetry::ScalarID aId, const nsAString& aKey, uint32_t aVal)
{
  TelemetryScalar::Set(aId, aKey, aVal);
}

void
ScalarSet(mozilla::Telemetry::ScalarID aId, const nsAString& aKey, bool aVal)
{
  TelemetryScalar::Set(aId, aKey, aVal);
}

void
ScalarSetMaximum(mozilla::Telemetry::ScalarID aId, const nsAString& aKey, uint32_t aVal)
{
  TelemetryScalar::SetMaximum(aId, aKey, aVal);
}

} // namespace Telemetry
} // namespace mozilla
