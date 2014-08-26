/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTraceRefcnt.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/StaticPtr.h"
#include "nsXPCOMPrivate.h"
#include "nscore.h"
#include "nsISupports.h"
#include "nsTArray.h"
#include "nsTHashtable.h"
#include "prenv.h"
#include "plstr.h"
#include "prlink.h"
#include "nsCRT.h"
#include <math.h>
#include "nsHashKeys.h"
#include "nsStackWalkPrivate.h"
#include "nsStackWalk.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "CodeAddressService.h"

#include "nsXULAppAPI.h"
#ifdef XP_WIN
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

#ifdef NS_TRACE_MALLOC
#include "nsTraceMalloc.h"
#endif

#include "mozilla/BlockingResourceBase.h"
#include "mozilla/PoisonIOInterposer.h"

#ifdef HAVE_DLOPEN
#include <dlfcn.h>
#endif

////////////////////////////////////////////////////////////////////////////////

void
NS_MeanAndStdDev(double aNumberOfValues,
                 double aSumOfValues, double aSumOfSquaredValues,
                 double* aMeanResult, double* aStdDevResult)
{
  double mean = 0.0, var = 0.0, stdDev = 0.0;
  if (aNumberOfValues > 0.0 && aSumOfValues >= 0) {
    mean = aSumOfValues / aNumberOfValues;
    double temp =
      (aNumberOfValues * aSumOfSquaredValues) - (aSumOfValues * aSumOfValues);
    if (temp < 0.0 || aNumberOfValues <= 1) {
      var = 0.0;
    } else {
      var = temp / (aNumberOfValues * (aNumberOfValues - 1));
    }
    // for some reason, Windows says sqrt(0.0) is "-1.#J" (?!) so do this:
    stdDev = var != 0.0 ? sqrt(var) : 0.0;
  }
  *aMeanResult = mean;
  *aStdDevResult = stdDev;
}

////////////////////////////////////////////////////////////////////////////////

#if !defined(XP_WIN) || (!defined(MOZ_OPTIMIZE) || defined(MOZ_PROFILING) || defined(DEBUG))
#define STACKWALKING_AVAILABLE
#endif

#define NS_IMPL_REFCNT_LOGGING

#ifdef NS_IMPL_REFCNT_LOGGING
#include "plhash.h"
#include "prmem.h"

#include "prlock.h"

// TraceRefcnt has to use bare PRLock instead of mozilla::Mutex
// because TraceRefcnt can be used very early in startup.
static PRLock* gTraceLock;

#define LOCK_TRACELOG()   PR_Lock(gTraceLock)
#define UNLOCK_TRACELOG() PR_Unlock(gTraceLock)

static PLHashTable* gBloatView;
static PLHashTable* gTypesToLog;
static PLHashTable* gObjectsToLog;
static PLHashTable* gSerialNumbers;
static intptr_t gNextSerialNumber;

static bool gLogging;
static bool gLogLeaksOnly;

#define BAD_TLS_INDEX ((unsigned)-1)

// if gActivityTLS == BAD_TLS_INDEX, then we're
// unitialized... otherwise this points to a NSPR TLS thread index
// indicating whether addref activity is legal. If the PTR_TO_INT32 is 0 then
// activity is ok, otherwise not!
static unsigned gActivityTLS = BAD_TLS_INDEX;

static bool gInitialized;
static nsrefcnt gInitCount;

static FILE* gBloatLog = nullptr;
static FILE* gRefcntsLog = nullptr;
static FILE* gAllocLog = nullptr;
static FILE* gLeakyLog = nullptr;
static FILE* gCOMPtrLog = nullptr;

struct serialNumberRecord
{
  intptr_t serialNumber;
  int32_t refCount;
  int32_t COMPtrCount;
};

struct nsTraceRefcntStats
{
  uint64_t mAddRefs;
  uint64_t mReleases;
  uint64_t mCreates;
  uint64_t mDestroys;
  double mRefsOutstandingTotal;
  double mRefsOutstandingSquared;
  double mObjsOutstandingTotal;
  double mObjsOutstandingSquared;
};

#ifdef DEBUG
static const char kStaticCtorDtorWarning[] =
  "XPCOM objects created/destroyed from static ctor/dtor";

static void
AssertActivityIsLegal()
{
  if (gActivityTLS == BAD_TLS_INDEX ||
      NS_PTR_TO_INT32(PR_GetThreadPrivate(gActivityTLS)) != 0) {
    if (PR_GetEnv("MOZ_FATAL_STATIC_XPCOM_CTORS_DTORS")) {
      NS_RUNTIMEABORT(kStaticCtorDtorWarning);
    } else {
      NS_WARNING(kStaticCtorDtorWarning);
    }
  }
}
#  define ASSERT_ACTIVITY_IS_LEGAL              \
  PR_BEGIN_MACRO                                \
    AssertActivityIsLegal();                    \
  PR_END_MACRO
#else
#  define ASSERT_ACTIVITY_IS_LEGAL PR_BEGIN_MACRO PR_END_MACRO
#endif  // DEBUG

// These functions are copied from nsprpub/lib/ds/plhash.c, with changes
// to the functions not called Default* to free the serialNumberRecord or
// the BloatEntry.

static void*
DefaultAllocTable(void* aPool, size_t aSize)
{
  return PR_MALLOC(aSize);
}

static void
DefaultFreeTable(void* aPool, void* aItem)
{
  PR_Free(aItem);
}

static PLHashEntry*
DefaultAllocEntry(void* aPool, const void* aKey)
{
  return PR_NEW(PLHashEntry);
}

static void
SerialNumberFreeEntry(void* aPool, PLHashEntry* aHashEntry, unsigned aFlag)
{
  if (aFlag == HT_FREE_ENTRY) {
    PR_Free(reinterpret_cast<serialNumberRecord*>(aHashEntry->value));
    PR_Free(aHashEntry);
  }
}

static void
TypesToLogFreeEntry(void* aPool, PLHashEntry* aHashEntry, unsigned aFlag)
{
  if (aFlag == HT_FREE_ENTRY) {
    free(const_cast<char*>(reinterpret_cast<const char*>(aHashEntry->key)));
    PR_Free(aHashEntry);
  }
}

static const PLHashAllocOps serialNumberHashAllocOps = {
  DefaultAllocTable, DefaultFreeTable,
  DefaultAllocEntry, SerialNumberFreeEntry
};

static const PLHashAllocOps typesToLogHashAllocOps = {
  DefaultAllocTable, DefaultFreeTable,
  DefaultAllocEntry, TypesToLogFreeEntry
};

////////////////////////////////////////////////////////////////////////////////

#ifdef STACKWALKING_AVAILABLE

class CodeAddressServiceStringTable MOZ_FINAL
{
public:
  CodeAddressServiceStringTable() : mSet(32) {}

  const char* Intern(const char* aString)
  {
    nsCharPtrHashKey* e = mSet.PutEntry(aString);
    return e->GetKey();
  }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    return mSet.SizeOfExcludingThis(aMallocSizeOf);
  }

private:
  typedef nsTHashtable<nsCharPtrHashKey> StringSet;
  StringSet mSet;
};

struct CodeAddressServiceStringAlloc MOZ_FINAL
{
  static char* copy(const char* aStr) { return strdup(aStr); }
  static void free(char* aPtr) { ::free(aPtr); }
};

class CodeAddressServiceWriter MOZ_FINAL
{
public:
  explicit CodeAddressServiceWriter(FILE* aFile)
    : mFile(aFile)
  {
  }

  void Write(const char* aFmt, ...) const
  {
    va_list ap;
    va_start(ap, aFmt);
    vfprintf(mFile, aFmt, ap);
    va_end(ap);
  }

private:
  FILE* mFile;
};

// WalkTheStack does not hold any locks needed by NS_DescribeCodeAddress, so
// this class does not need to do anything.
struct CodeAddressServiceLock MOZ_FINAL
{
  static void Unlock() {}
  static void Lock() {}
  static bool IsLocked() { return true; }
};

typedef mozilla::CodeAddressService<CodeAddressServiceStringTable,
                                    CodeAddressServiceStringAlloc,
                                    CodeAddressServiceWriter,
                                    CodeAddressServiceLock> WalkTheStackCodeAddressService;

mozilla::StaticAutoPtr<WalkTheStackCodeAddressService> gCodeAddressService;

#endif // STACKWALKING_AVAILABLE

////////////////////////////////////////////////////////////////////////////////

class BloatEntry
{
public:
  BloatEntry(const char* aClassName, uint32_t aClassSize)
    : mClassSize(aClassSize)
  {
    mClassName = PL_strdup(aClassName);
    Clear(&mNewStats);
    Clear(&mAllStats);
    mTotalLeaked = 0;
  }

  ~BloatEntry()
  {
    PL_strfree(mClassName);
  }

  uint32_t GetClassSize()
  {
    return (uint32_t)mClassSize;
  }
  const char* GetClassName()
  {
    return mClassName;
  }

  static void Clear(nsTraceRefcntStats* aStats)
  {
    aStats->mAddRefs = 0;
    aStats->mReleases = 0;
    aStats->mCreates = 0;
    aStats->mDestroys = 0;
    aStats->mRefsOutstandingTotal = 0;
    aStats->mRefsOutstandingSquared = 0;
    aStats->mObjsOutstandingTotal = 0;
    aStats->mObjsOutstandingSquared = 0;
  }

  void Accumulate()
  {
    mAllStats.mAddRefs += mNewStats.mAddRefs;
    mAllStats.mReleases += mNewStats.mReleases;
    mAllStats.mCreates += mNewStats.mCreates;
    mAllStats.mDestroys += mNewStats.mDestroys;
    mAllStats.mRefsOutstandingTotal += mNewStats.mRefsOutstandingTotal;
    mAllStats.mRefsOutstandingSquared += mNewStats.mRefsOutstandingSquared;
    mAllStats.mObjsOutstandingTotal += mNewStats.mObjsOutstandingTotal;
    mAllStats.mObjsOutstandingSquared += mNewStats.mObjsOutstandingSquared;
    Clear(&mNewStats);
  }

  void AddRef(nsrefcnt aRefcnt)
  {
    mNewStats.mAddRefs++;
    if (aRefcnt == 1) {
      Ctor();
    }
    AccountRefs();
  }

  void Release(nsrefcnt aRefcnt)
  {
    mNewStats.mReleases++;
    if (aRefcnt == 0) {
      Dtor();
    }
    AccountRefs();
  }

  void Ctor()
  {
    mNewStats.mCreates++;
    AccountObjs();
  }

  void Dtor()
  {
    mNewStats.mDestroys++;
    AccountObjs();
  }

  void AccountRefs()
  {
    uint64_t cnt = (mNewStats.mAddRefs - mNewStats.mReleases);
    mNewStats.mRefsOutstandingTotal += cnt;
    mNewStats.mRefsOutstandingSquared += cnt * cnt;
  }

  void AccountObjs()
  {
    uint64_t cnt = (mNewStats.mCreates - mNewStats.mDestroys);
    mNewStats.mObjsOutstandingTotal += cnt;
    mNewStats.mObjsOutstandingSquared += cnt * cnt;
  }

  static int DumpEntry(PLHashEntry* aHashEntry, int aIndex, void* aArg)
  {
    BloatEntry* entry = (BloatEntry*)aHashEntry->value;
    if (entry) {
      entry->Accumulate();
      static_cast<nsTArray<BloatEntry*>*>(aArg)->AppendElement(entry);
    }
    return HT_ENUMERATE_NEXT;
  }

  static int TotalEntries(PLHashEntry* aHashEntry, int aIndex, void* aArg)
  {
    BloatEntry* entry = (BloatEntry*)aHashEntry->value;
    if (entry && nsCRT::strcmp(entry->mClassName, "TOTAL") != 0) {
      entry->Total((BloatEntry*)aArg);
    }
    return HT_ENUMERATE_NEXT;
  }

  void Total(BloatEntry* aTotal)
  {
    aTotal->mAllStats.mAddRefs += mNewStats.mAddRefs + mAllStats.mAddRefs;
    aTotal->mAllStats.mReleases += mNewStats.mReleases + mAllStats.mReleases;
    aTotal->mAllStats.mCreates += mNewStats.mCreates + mAllStats.mCreates;
    aTotal->mAllStats.mDestroys += mNewStats.mDestroys + mAllStats.mDestroys;
    aTotal->mAllStats.mRefsOutstandingTotal +=
      mNewStats.mRefsOutstandingTotal + mAllStats.mRefsOutstandingTotal;
    aTotal->mAllStats.mRefsOutstandingSquared +=
      mNewStats.mRefsOutstandingSquared + mAllStats.mRefsOutstandingSquared;
    aTotal->mAllStats.mObjsOutstandingTotal +=
      mNewStats.mObjsOutstandingTotal + mAllStats.mObjsOutstandingTotal;
    aTotal->mAllStats.mObjsOutstandingSquared +=
      mNewStats.mObjsOutstandingSquared + mAllStats.mObjsOutstandingSquared;
    uint64_t count = (mNewStats.mCreates + mAllStats.mCreates);
    aTotal->mClassSize += mClassSize * count;    // adjust for average in DumpTotal
    aTotal->mTotalLeaked += (uint64_t)(mClassSize *
                                       ((mNewStats.mCreates + mAllStats.mCreates)
                                        - (mNewStats.mDestroys + mAllStats.mDestroys)));
  }

  void DumpTotal(FILE* aOut)
  {
    mClassSize /= mAllStats.mCreates;
    Dump(-1, aOut, nsTraceRefcnt::ALL_STATS);
  }

  static bool HaveLeaks(nsTraceRefcntStats* aStats)
  {
    return ((aStats->mAddRefs != aStats->mReleases) ||
            (aStats->mCreates != aStats->mDestroys));
  }

  bool PrintDumpHeader(FILE* aOut, const char* aMsg,
                       nsTraceRefcnt::StatisticsType aType)
  {
    fprintf(aOut, "\n== BloatView: %s, %s process %d\n", aMsg,
            XRE_ChildProcessTypeToString(XRE_GetProcessType()), getpid());
    nsTraceRefcntStats& stats =
      (aType == nsTraceRefcnt::NEW_STATS) ? mNewStats : mAllStats;
    if (gLogLeaksOnly && !HaveLeaks(&stats)) {
      return false;
    }

    fprintf(aOut,
            "\n" \
            "     |<----------------Class--------------->|<-----Bytes------>|<----------------Objects---------------->|<--------------References-------------->|\n" \
            "                                              Per-Inst   Leaked    Total      Rem      Mean       StdDev     Total      Rem      Mean       StdDev\n");

    this->DumpTotal(aOut);

    return true;
  }

  void Dump(int aIndex, FILE* aOut, nsTraceRefcnt::StatisticsType aType)
  {
    nsTraceRefcntStats* stats =
      (aType == nsTraceRefcnt::NEW_STATS) ? &mNewStats : &mAllStats;
    if (gLogLeaksOnly && !HaveLeaks(stats)) {
      return;
    }

    double meanRefs;
    double stddevRefs;
    NS_MeanAndStdDev(stats->mAddRefs + stats->mReleases,
                     stats->mRefsOutstandingTotal,
                     stats->mRefsOutstandingSquared,
                     &meanRefs, &stddevRefs);

    double meanObjs;
    double stddevObjs;
    NS_MeanAndStdDev(stats->mCreates + stats->mDestroys,
                     stats->mObjsOutstandingTotal,
                     stats->mObjsOutstandingSquared,
                     &meanObjs, &stddevObjs);

    if ((stats->mAddRefs - stats->mReleases) != 0 ||
        stats->mAddRefs != 0 ||
        meanRefs != 0 ||
        stddevRefs != 0 ||
        (stats->mCreates - stats->mDestroys) != 0 ||
        stats->mCreates != 0 ||
        meanObjs != 0 ||
        stddevObjs != 0) {
      fprintf(aOut, "%4d %-40.40s %8d %8" PRIu64 " %8" PRIu64 " %8" PRIu64 " (%8.2f +/- %8.2f) %8" PRIu64 " %8" PRIu64 " (%8.2f +/- %8.2f)\n",
              aIndex + 1, mClassName,
              (int32_t)mClassSize,
              (nsCRT::strcmp(mClassName, "TOTAL"))
                ? (uint64_t)((stats->mCreates - stats->mDestroys) * mClassSize)
                : mTotalLeaked,
              stats->mCreates,
              (stats->mCreates - stats->mDestroys),
              meanObjs,
              stddevObjs,
              stats->mAddRefs,
              (stats->mAddRefs - stats->mReleases),
              meanRefs,
              stddevRefs);
    }
  }

protected:
  char*         mClassName;
  double        mClassSize;     // this is stored as a double because of the way we compute the avg class size for total bloat
  uint64_t      mTotalLeaked; // used only for TOTAL entry
  nsTraceRefcntStats mNewStats;
  nsTraceRefcntStats mAllStats;
};

static void
BloatViewFreeEntry(void* aPool, PLHashEntry* aHashEntry, unsigned aFlag)
{
  if (aFlag == HT_FREE_ENTRY) {
    BloatEntry* entry = reinterpret_cast<BloatEntry*>(aHashEntry->value);
    delete entry;
    PR_Free(aHashEntry);
  }
}

const static PLHashAllocOps bloatViewHashAllocOps = {
  DefaultAllocTable, DefaultFreeTable,
  DefaultAllocEntry, BloatViewFreeEntry
};

static void
RecreateBloatView()
{
  gBloatView = PL_NewHashTable(256,
                               PL_HashString,
                               PL_CompareStrings,
                               PL_CompareValues,
                               &bloatViewHashAllocOps, nullptr);
}

static BloatEntry*
GetBloatEntry(const char* aTypeName, uint32_t aInstanceSize)
{
  if (!gBloatView) {
    RecreateBloatView();
  }
  BloatEntry* entry = nullptr;
  if (gBloatView) {
    entry = (BloatEntry*)PL_HashTableLookup(gBloatView, aTypeName);
    if (!entry && aInstanceSize > 0) {

      entry = new BloatEntry(aTypeName, aInstanceSize);
      PLHashEntry* e = PL_HashTableAdd(gBloatView, aTypeName, entry);
      if (!e) {
        delete entry;
        entry = nullptr;
      }
    } else {
      NS_ASSERTION(aInstanceSize == 0 ||
                   entry->GetClassSize() == aInstanceSize,
                   "bad size recorded");
    }
  }
  return entry;
}

static int
DumpSerialNumbers(PLHashEntry* aHashEntry, int aIndex, void* aClosure)
{
  serialNumberRecord* record =
    reinterpret_cast<serialNumberRecord*>(aHashEntry->value);
#ifdef HAVE_CPP_DYNAMIC_CAST_TO_VOID_PTR
  fprintf((FILE*)aClosure, "%" PRIdPTR
          " @%p (%d references; %d from COMPtrs)\n",
          record->serialNumber,
          NS_INT32_TO_PTR(aHashEntry->key),
          record->refCount,
          record->COMPtrCount);
#else
  fprintf((FILE*)aClosure, "%" PRIdPTR
          " @%p (%d references)\n",
          record->serialNumber,
          NS_INT32_TO_PTR(aHashEntry->key),
          record->refCount);
#endif
  return HT_ENUMERATE_NEXT;
}


template<>
class nsDefaultComparator<BloatEntry*, BloatEntry*>
{
public:
  bool Equals(BloatEntry* const& aEntry1, BloatEntry* const& aEntry2) const
  {
    return PL_strcmp(aEntry1->GetClassName(), aEntry2->GetClassName()) == 0;
  }
  bool LessThan(BloatEntry* const& aEntry1, BloatEntry* const& aEntry2) const
  {
    return PL_strcmp(aEntry1->GetClassName(), aEntry2->GetClassName()) < 0;
  }
};

#endif /* NS_IMPL_REFCNT_LOGGING */

nsresult
nsTraceRefcnt::DumpStatistics(StatisticsType aType, FILE* aOut)
{
#ifdef NS_IMPL_REFCNT_LOGGING
  if (!gBloatLog || !gBloatView) {
    return NS_ERROR_FAILURE;
  }
  if (!aOut) {
    aOut = gBloatLog;
  }

  LOCK_TRACELOG();

  bool wasLogging = gLogging;
  gLogging = false;  // turn off logging for this method

  BloatEntry total("TOTAL", 0);
  PL_HashTableEnumerateEntries(gBloatView, BloatEntry::TotalEntries, &total);
  const char* msg;
  if (aType == NEW_STATS) {
    if (gLogLeaksOnly) {
      msg = "NEW (incremental) LEAK STATISTICS";
    } else {
      msg = "NEW (incremental) LEAK AND BLOAT STATISTICS";
    }
  } else {
    if (gLogLeaksOnly) {
      msg = "ALL (cumulative) LEAK STATISTICS";
    } else {
      msg = "ALL (cumulative) LEAK AND BLOAT STATISTICS";
    }
  }
  const bool leaked = total.PrintDumpHeader(aOut, msg, aType);

  nsTArray<BloatEntry*> entries;
  PL_HashTableEnumerateEntries(gBloatView, BloatEntry::DumpEntry, &entries);
  const uint32_t count = entries.Length();

  if (!gLogLeaksOnly || leaked) {
    // Sort the entries alphabetically by classname.
    entries.Sort();

    for (uint32_t i = 0; i < count; ++i) {
      BloatEntry* entry = entries[i];
      entry->Dump(i, aOut, aType);
    }

    fprintf(aOut, "\n");
  }

  fprintf(aOut, "nsTraceRefcnt::DumpStatistics: %d entries\n", count);

  if (gSerialNumbers) {
    fprintf(aOut, "\nSerial Numbers of Leaked Objects:\n");
    PL_HashTableEnumerateEntries(gSerialNumbers, DumpSerialNumbers, aOut);
  }

  gLogging = wasLogging;
  UNLOCK_TRACELOG();
#endif

  return NS_OK;
}

void
nsTraceRefcnt::ResetStatistics()
{
#ifdef NS_IMPL_REFCNT_LOGGING
  LOCK_TRACELOG();
  if (gBloatView) {
    PL_HashTableDestroy(gBloatView);
    gBloatView = nullptr;
  }
  UNLOCK_TRACELOG();
#endif
}

#ifdef NS_IMPL_REFCNT_LOGGING
static bool
LogThisType(const char* aTypeName)
{
  void* he = PL_HashTableLookup(gTypesToLog, aTypeName);
  return he != nullptr;
}

static intptr_t
GetSerialNumber(void* aPtr, bool aCreate)
{
  PLHashEntry** hep = PL_HashTableRawLookup(gSerialNumbers,
                                            PLHashNumber(NS_PTR_TO_INT32(aPtr)),
                                            aPtr);
  if (hep && *hep) {
    return reinterpret_cast<serialNumberRecord*>((*hep)->value)->serialNumber;
  } else if (aCreate) {
    serialNumberRecord* record = PR_NEW(serialNumberRecord);
    record->serialNumber = ++gNextSerialNumber;
    record->refCount = 0;
    record->COMPtrCount = 0;
    PL_HashTableRawAdd(gSerialNumbers, hep, PLHashNumber(NS_PTR_TO_INT32(aPtr)),
                       aPtr, reinterpret_cast<void*>(record));
    return gNextSerialNumber;
  }
  return 0;
}

static int32_t*
GetRefCount(void* aPtr)
{
  PLHashEntry** hep = PL_HashTableRawLookup(gSerialNumbers,
                                            PLHashNumber(NS_PTR_TO_INT32(aPtr)),
                                            aPtr);
  if (hep && *hep) {
    return &((reinterpret_cast<serialNumberRecord*>((*hep)->value))->refCount);
  } else {
    return nullptr;
  }
}

#if defined(NS_IMPL_REFCNT_LOGGING) && defined(HAVE_CPP_DYNAMIC_CAST_TO_VOID_PTR)
static int32_t*
GetCOMPtrCount(void* aPtr)
{
  PLHashEntry** hep = PL_HashTableRawLookup(gSerialNumbers,
                                            PLHashNumber(NS_PTR_TO_INT32(aPtr)),
                                            aPtr);
  if (hep && *hep) {
    return &((reinterpret_cast<serialNumberRecord*>((*hep)->value))->COMPtrCount);
  }
  return nullptr;
}
#endif

static void
RecycleSerialNumberPtr(void* aPtr)
{
  PL_HashTableRemove(gSerialNumbers, aPtr);
}

static bool
LogThisObj(intptr_t aSerialNumber)
{
  return (bool)PL_HashTableLookup(gObjectsToLog, (const void*)aSerialNumber);
}

#ifdef XP_WIN
#define FOPEN_NO_INHERIT "N"
#else
#define FOPEN_NO_INHERIT
#endif

static bool
InitLog(const char* aEnvVar, const char* aMsg, FILE** aResult)
{
  const char* value = getenv(aEnvVar);
  if (value) {
    if (nsCRT::strcmp(value, "1") == 0) {
      *aResult = stdout;
      fprintf(stdout, "### %s defined -- logging %s to stdout\n",
              aEnvVar, aMsg);
      return true;
    } else if (nsCRT::strcmp(value, "2") == 0) {
      *aResult = stderr;
      fprintf(stdout, "### %s defined -- logging %s to stderr\n",
              aEnvVar, aMsg);
      return true;
    } else {
      FILE* stream;
      nsAutoCString fname(value);
      if (XRE_GetProcessType() != GeckoProcessType_Default) {
        bool hasLogExtension =
          fname.RFind(".log", true, -1, 4) == kNotFound ? false : true;
        if (hasLogExtension) {
          fname.Cut(fname.Length() - 4, 4);
        }
        fname.Append('_');
        fname.Append((char*)XRE_ChildProcessTypeToString(XRE_GetProcessType()));
        fname.AppendLiteral("_pid");
        fname.AppendInt((uint32_t)getpid());
        if (hasLogExtension) {
          fname.AppendLiteral(".log");
        }
      }
      stream = ::fopen(fname.get(), "w" FOPEN_NO_INHERIT);
      if (stream) {
        MozillaRegisterDebugFD(fileno(stream));
        *aResult = stream;
        fprintf(stdout, "### %s defined -- logging %s to %s\n",
                aEnvVar, aMsg, fname.get());
      } else {
        fprintf(stdout, "### %s defined -- unable to log %s to %s\n",
                aEnvVar, aMsg, fname.get());
      }
      return stream != nullptr;
    }
  }
  return false;
}


static PLHashNumber
HashNumber(const void* aKey)
{
  return PLHashNumber(NS_PTR_TO_INT32(aKey));
}

static void
maybeUnregisterAndCloseFile(FILE*& aFile)
{
  if (!aFile) {
    return;
  }

  MozillaUnRegisterDebugFILE(aFile);
  fclose(aFile);
  aFile = nullptr;
}


static void
InitTraceLog()
{
  if (gInitialized) {
    return;
  }
  gInitialized = true;

  bool defined = InitLog("XPCOM_MEM_BLOAT_LOG", "bloat/leaks", &gBloatLog);
  if (!defined) {
    gLogLeaksOnly = InitLog("XPCOM_MEM_LEAK_LOG", "leaks", &gBloatLog);
  }
  if (defined || gLogLeaksOnly) {
    RecreateBloatView();
    if (!gBloatView) {
      NS_WARNING("out of memory");
      maybeUnregisterAndCloseFile(gBloatLog);
      gLogLeaksOnly = false;
    }
  }

  InitLog("XPCOM_MEM_REFCNT_LOG", "refcounts", &gRefcntsLog);

  InitLog("XPCOM_MEM_ALLOC_LOG", "new/delete", &gAllocLog);

  const char* classes = getenv("XPCOM_MEM_LOG_CLASSES");

#ifdef HAVE_CPP_DYNAMIC_CAST_TO_VOID_PTR
  if (classes) {
    InitLog("XPCOM_MEM_COMPTR_LOG", "nsCOMPtr", &gCOMPtrLog);
  } else {
    if (getenv("XPCOM_MEM_COMPTR_LOG")) {
      fprintf(stdout, "### XPCOM_MEM_COMPTR_LOG defined -- but XPCOM_MEM_LOG_CLASSES is not defined\n");
    }
  }
#else
  const char* comptr_log = getenv("XPCOM_MEM_COMPTR_LOG");
  if (comptr_log) {
    fprintf(stdout, "### XPCOM_MEM_COMPTR_LOG defined -- but it will not work without dynamic_cast\n");
  }
#endif

  if (classes) {
    // if XPCOM_MEM_LOG_CLASSES was set to some value, the value is interpreted
    // as a list of class names to track
    gTypesToLog = PL_NewHashTable(256,
                                  PL_HashString,
                                  PL_CompareStrings,
                                  PL_CompareValues,
                                  &typesToLogHashAllocOps, nullptr);
    if (!gTypesToLog) {
      NS_WARNING("out of memory");
      fprintf(stdout, "### XPCOM_MEM_LOG_CLASSES defined -- unable to log specific classes\n");
    } else {
      fprintf(stdout, "### XPCOM_MEM_LOG_CLASSES defined -- only logging these classes: ");
      const char* cp = classes;
      for (;;) {
        char* cm = (char*)strchr(cp, ',');
        if (cm) {
          *cm = '\0';
        }
        PL_HashTableAdd(gTypesToLog, strdup(cp), (void*)1);
        fprintf(stdout, "%s ", cp);
        if (!cm) {
          break;
        }
        *cm = ',';
        cp = cm + 1;
      }
      fprintf(stdout, "\n");
    }

    gSerialNumbers = PL_NewHashTable(256,
                                     HashNumber,
                                     PL_CompareValues,
                                     PL_CompareValues,
                                     &serialNumberHashAllocOps, nullptr);


  }

  const char* objects = getenv("XPCOM_MEM_LOG_OBJECTS");
  if (objects) {
    gObjectsToLog = PL_NewHashTable(256,
                                    HashNumber,
                                    PL_CompareValues,
                                    PL_CompareValues,
                                    nullptr, nullptr);

    if (!gObjectsToLog) {
      NS_WARNING("out of memory");
      fprintf(stdout, "### XPCOM_MEM_LOG_OBJECTS defined -- unable to log specific objects\n");
    } else if (!(gRefcntsLog || gAllocLog || gCOMPtrLog)) {
      fprintf(stdout, "### XPCOM_MEM_LOG_OBJECTS defined -- but none of XPCOM_MEM_(REFCNT|ALLOC|COMPTR)_LOG is defined\n");
    } else {
      fprintf(stdout, "### XPCOM_MEM_LOG_OBJECTS defined -- only logging these objects: ");
      const char* cp = objects;
      for (;;) {
        char* cm = (char*)strchr(cp, ',');
        if (cm) {
          *cm = '\0';
        }
        intptr_t top = 0;
        intptr_t bottom = 0;
        while (*cp) {
          if (*cp == '-') {
            bottom = top;
            top = 0;
            ++cp;
          }
          top *= 10;
          top += *cp - '0';
          ++cp;
        }
        if (!bottom) {
          bottom = top;
        }
        for (intptr_t serialno = bottom; serialno <= top; serialno++) {
          PL_HashTableAdd(gObjectsToLog, (const void*)serialno, (void*)1);
          fprintf(stdout, "%" PRIdPTR " ", serialno);
        }
        if (!cm) {
          break;
        }
        *cm = ',';
        cp = cm + 1;
      }
      fprintf(stdout, "\n");
    }
  }


  if (gBloatLog || gRefcntsLog || gAllocLog || gLeakyLog || gCOMPtrLog) {
    gLogging = true;
  }

  gTraceLock = PR_NewLock();
}

#endif

extern "C" {

#ifdef STACKWALKING_AVAILABLE
static void
PrintStackFrame(void* aPC, void* aSP, void* aClosure)
{
  FILE* stream = (FILE*)aClosure;
  nsCodeAddressDetails details;
  char buf[1024];

  NS_DescribeCodeAddress(aPC, &details);
  NS_FormatCodeAddressDetails(aPC, &details, buf, sizeof(buf));
  fputs(buf, stream);
}

static void
PrintStackFrameCached(void* aPC, void* aSP, void* aClosure)
{
  auto writer = static_cast<CodeAddressServiceWriter*>(aClosure);
  gCodeAddressService->WriteLocation(*writer, aPC);
}
#endif

}

void
nsTraceRefcnt::WalkTheStack(FILE* aStream)
{
#ifdef STACKWALKING_AVAILABLE
  NS_StackWalk(PrintStackFrame, /* skipFrames */ 2, /* maxFrames */ 0, aStream,
               0, nullptr);
#endif
}

void
nsTraceRefcnt::WalkTheStackCached(FILE* aStream)
{
#ifdef STACKWALKING_AVAILABLE
  if (!gCodeAddressService) {
    gCodeAddressService = new WalkTheStackCodeAddressService();
  }
  CodeAddressServiceWriter writer(aStream);
  NS_StackWalk(PrintStackFrameCached, /* skipFrames */ 2, /* maxFrames */ 0,
               &writer, 0, nullptr);
#endif
}

//----------------------------------------------------------------------

// This thing is exported by libstdc++
// Yes, this is a gcc only hack
#if defined(MOZ_DEMANGLE_SYMBOLS)
#include <cxxabi.h>
#include <stdlib.h> // for free()
#endif // MOZ_DEMANGLE_SYMBOLS

void
nsTraceRefcnt::DemangleSymbol(const char* aSymbol,
                              char* aBuffer,
                              int aBufLen)
{
  NS_ASSERTION(aSymbol, "null symbol");
  NS_ASSERTION(aBuffer, "null buffer");
  NS_ASSERTION(aBufLen >= 32 , "pulled 32 out of you know where");

  aBuffer[0] = '\0';

#if defined(MOZ_DEMANGLE_SYMBOLS)
  /* See demangle.h in the gcc source for the voodoo */
  char* demangled = abi::__cxa_demangle(aSymbol, 0, 0, 0);

  if (demangled) {
    strncpy(aBuffer, demangled, aBufLen);
    free(demangled);
  }
#endif // MOZ_DEMANGLE_SYMBOLS
}


//----------------------------------------------------------------------

EXPORT_XPCOM_API(void)
NS_LogInit()
{
  NS_SetMainThread();

  // FIXME: This is called multiple times, we should probably not allow that.
#ifdef STACKWALKING_AVAILABLE
  StackWalkInitCriticalAddress();
#endif
#ifdef NS_IMPL_REFCNT_LOGGING
  if (++gInitCount) {
    nsTraceRefcnt::SetActivityIsLegal(true);
  }
#endif

#ifdef NS_TRACE_MALLOC
  // XXX we don't have to worry about shutting down trace-malloc; it
  // handles this itself, through an atexit() callback.
  if (!NS_TraceMallocHasStarted()) {
    NS_TraceMallocStartup(-1);  // -1 == no logging
  }
#endif
}

EXPORT_XPCOM_API(void)
NS_LogTerm()
{
  mozilla::LogTerm();
}

namespace mozilla {
void
LogTerm()
{
  NS_ASSERTION(gInitCount > 0,
               "NS_LogTerm without matching NS_LogInit");

  if (--gInitCount == 0) {
#ifdef DEBUG
    /* FIXME bug 491977: This is only going to operate on the
     * BlockingResourceBase which is compiled into
     * libxul/libxpcom_core.so. Anyone using external linkage will
     * have their own copy of BlockingResourceBase statics which will
     * not be freed by this method.
     *
     * It sounds like what we really want is to be able to register a
     * callback function to call at XPCOM shutdown.  Note that with
     * this solution, however, we need to guarantee that
     * BlockingResourceBase::Shutdown() runs after all other shutdown
     * functions.
     */
    BlockingResourceBase::Shutdown();
#endif

    if (gInitialized) {
      nsTraceRefcnt::DumpStatistics();
      nsTraceRefcnt::ResetStatistics();
    }
    nsTraceRefcnt::Shutdown();
#ifdef NS_IMPL_REFCNT_LOGGING
    nsTraceRefcnt::SetActivityIsLegal(false);
    gActivityTLS = BAD_TLS_INDEX;
#endif
  }
}

} // namespace mozilla

EXPORT_XPCOM_API(void)
NS_LogAddRef(void* aPtr, nsrefcnt aRefcnt,
             const char* aClass, uint32_t aClassSize)
{
#ifdef NS_IMPL_REFCNT_LOGGING
  ASSERT_ACTIVITY_IS_LEGAL;
  if (!gInitialized) {
    InitTraceLog();
  }
  if (gLogging) {
    LOCK_TRACELOG();

    if (gBloatLog) {
      BloatEntry* entry = GetBloatEntry(aClass, aClassSize);
      if (entry) {
        entry->AddRef(aRefcnt);
      }
    }

    // Here's the case where MOZ_COUNT_CTOR was not used,
    // yet we still want to see creation information:

    bool loggingThisType = (!gTypesToLog || LogThisType(aClass));
    intptr_t serialno = 0;
    if (gSerialNumbers && loggingThisType) {
      serialno = GetSerialNumber(aPtr, aRefcnt == 1);
      NS_ASSERTION(serialno != 0,
                   "Serial number requested for unrecognized pointer!  "
                   "Are you memmoving a refcounted object?");
      int32_t* count = GetRefCount(aPtr);
      if (count) {
        (*count)++;
      }

    }

    bool loggingThisObject = (!gObjectsToLog || LogThisObj(serialno));
    if (aRefcnt == 1 && gAllocLog && loggingThisType && loggingThisObject) {
      fprintf(gAllocLog, "\n<%s> 0x%08X %" PRIdPTR " Create\n",
              aClass, NS_PTR_TO_INT32(aPtr), serialno);
      nsTraceRefcnt::WalkTheStackCached(gAllocLog);
    }

    if (gRefcntsLog && loggingThisType && loggingThisObject) {
      // Can't use PR_LOG(), b/c it truncates the line
      fprintf(gRefcntsLog,
              "\n<%s> 0x%08X %" PRIuPTR " AddRef %" PRIuPTR "\n",
              aClass, NS_PTR_TO_INT32(aPtr), serialno, aRefcnt);
      nsTraceRefcnt::WalkTheStackCached(gRefcntsLog);
      fflush(gRefcntsLog);
    }
    UNLOCK_TRACELOG();
  }
#endif
}

EXPORT_XPCOM_API(void)
NS_LogRelease(void* aPtr, nsrefcnt aRefcnt, const char* aClass)
{
#ifdef NS_IMPL_REFCNT_LOGGING
  ASSERT_ACTIVITY_IS_LEGAL;
  if (!gInitialized) {
    InitTraceLog();
  }
  if (gLogging) {
    LOCK_TRACELOG();

    if (gBloatLog) {
      BloatEntry* entry = GetBloatEntry(aClass, 0);
      if (entry) {
        entry->Release(aRefcnt);
      }
    }

    bool loggingThisType = (!gTypesToLog || LogThisType(aClass));
    intptr_t serialno = 0;
    if (gSerialNumbers && loggingThisType) {
      serialno = GetSerialNumber(aPtr, false);
      NS_ASSERTION(serialno != 0,
                   "Serial number requested for unrecognized pointer!  "
                   "Are you memmoving a refcounted object?");
      int32_t* count = GetRefCount(aPtr);
      if (count) {
        (*count)--;
      }

    }

    bool loggingThisObject = (!gObjectsToLog || LogThisObj(serialno));
    if (gRefcntsLog && loggingThisType && loggingThisObject) {
      // Can't use PR_LOG(), b/c it truncates the line
      fprintf(gRefcntsLog,
              "\n<%s> 0x%08X %" PRIuPTR " Release %" PRIuPTR "\n", aClass,
              NS_PTR_TO_INT32(aPtr), serialno, aRefcnt);
      nsTraceRefcnt::WalkTheStackCached(gRefcntsLog);
      fflush(gRefcntsLog);
    }

    // Here's the case where MOZ_COUNT_DTOR was not used,
    // yet we still want to see deletion information:

    if (aRefcnt == 0 && gAllocLog && loggingThisType && loggingThisObject) {
      fprintf(gAllocLog,
              "\n<%s> 0x%08X %" PRIdPTR " Destroy\n",
              aClass, NS_PTR_TO_INT32(aPtr), serialno);
      nsTraceRefcnt::WalkTheStackCached(gAllocLog);
    }

    if (aRefcnt == 0 && gSerialNumbers && loggingThisType) {
      RecycleSerialNumberPtr(aPtr);
    }

    UNLOCK_TRACELOG();
  }
#endif
}

EXPORT_XPCOM_API(void)
NS_LogCtor(void* aPtr, const char* aType, uint32_t aInstanceSize)
{
#ifdef NS_IMPL_REFCNT_LOGGING
  ASSERT_ACTIVITY_IS_LEGAL;
  if (!gInitialized) {
    InitTraceLog();
  }

  if (gLogging) {
    LOCK_TRACELOG();

    if (gBloatLog) {
      BloatEntry* entry = GetBloatEntry(aType, aInstanceSize);
      if (entry) {
        entry->Ctor();
      }
    }

    bool loggingThisType = (!gTypesToLog || LogThisType(aType));
    intptr_t serialno = 0;
    if (gSerialNumbers && loggingThisType) {
      serialno = GetSerialNumber(aPtr, true);
    }

    bool loggingThisObject = (!gObjectsToLog || LogThisObj(serialno));
    if (gAllocLog && loggingThisType && loggingThisObject) {
      fprintf(gAllocLog, "\n<%s> 0x%08X %" PRIdPTR " Ctor (%d)\n",
              aType, NS_PTR_TO_INT32(aPtr), serialno, aInstanceSize);
      nsTraceRefcnt::WalkTheStackCached(gAllocLog);
    }

    UNLOCK_TRACELOG();
  }
#endif
}


EXPORT_XPCOM_API(void)
NS_LogDtor(void* aPtr, const char* aType, uint32_t aInstanceSize)
{
#ifdef NS_IMPL_REFCNT_LOGGING
  ASSERT_ACTIVITY_IS_LEGAL;
  if (!gInitialized) {
    InitTraceLog();
  }

  if (gLogging) {
    LOCK_TRACELOG();

    if (gBloatLog) {
      BloatEntry* entry = GetBloatEntry(aType, aInstanceSize);
      if (entry) {
        entry->Dtor();
      }
    }

    bool loggingThisType = (!gTypesToLog || LogThisType(aType));
    intptr_t serialno = 0;
    if (gSerialNumbers && loggingThisType) {
      serialno = GetSerialNumber(aPtr, false);
      RecycleSerialNumberPtr(aPtr);
    }

    bool loggingThisObject = (!gObjectsToLog || LogThisObj(serialno));

    // (If we're on a losing architecture, don't do this because we'll be
    // using LogDeleteXPCOM instead to get file and line numbers.)
    if (gAllocLog && loggingThisType && loggingThisObject) {
      fprintf(gAllocLog, "\n<%s> 0x%08X %" PRIdPTR " Dtor (%d)\n",
              aType, NS_PTR_TO_INT32(aPtr), serialno, aInstanceSize);
      nsTraceRefcnt::WalkTheStackCached(gAllocLog);
    }

    UNLOCK_TRACELOG();
  }
#endif
}


EXPORT_XPCOM_API(void)
NS_LogCOMPtrAddRef(void* aCOMPtr, nsISupports* aObject)
{
#if defined(NS_IMPL_REFCNT_LOGGING) && defined(HAVE_CPP_DYNAMIC_CAST_TO_VOID_PTR)
  // Get the most-derived object.
  void* object = dynamic_cast<void*>(aObject);

  // This is a very indirect way of finding out what the class is
  // of the object being logged.  If we're logging a specific type,
  // then
  if (!gTypesToLog || !gSerialNumbers) {
    return;
  }
  intptr_t serialno = GetSerialNumber(object, false);
  if (serialno == 0) {
    return;
  }

  if (!gInitialized) {
    InitTraceLog();
  }
  if (gLogging) {
    LOCK_TRACELOG();

    int32_t* count = GetCOMPtrCount(object);
    if (count) {
      (*count)++;
    }

    bool loggingThisObject = (!gObjectsToLog || LogThisObj(serialno));

    if (gCOMPtrLog && loggingThisObject) {
      fprintf(gCOMPtrLog, "\n<?> 0x%08X %" PRIdPTR " nsCOMPtrAddRef %d 0x%08X\n",
              NS_PTR_TO_INT32(object), serialno, count ? (*count) : -1,
              NS_PTR_TO_INT32(aCOMPtr));
      nsTraceRefcnt::WalkTheStackCached(gCOMPtrLog);
    }

    UNLOCK_TRACELOG();
  }
#endif
}


EXPORT_XPCOM_API(void)
NS_LogCOMPtrRelease(void* aCOMPtr, nsISupports* aObject)
{
#if defined(NS_IMPL_REFCNT_LOGGING) && defined(HAVE_CPP_DYNAMIC_CAST_TO_VOID_PTR)
  // Get the most-derived object.
  void* object = dynamic_cast<void*>(aObject);

  // This is a very indirect way of finding out what the class is
  // of the object being logged.  If we're logging a specific type,
  // then
  if (!gTypesToLog || !gSerialNumbers) {
    return;
  }
  intptr_t serialno = GetSerialNumber(object, false);
  if (serialno == 0) {
    return;
  }

  if (!gInitialized) {
    InitTraceLog();
  }
  if (gLogging) {
    LOCK_TRACELOG();

    int32_t* count = GetCOMPtrCount(object);
    if (count) {
      (*count)--;
    }

    bool loggingThisObject = (!gObjectsToLog || LogThisObj(serialno));

    if (gCOMPtrLog && loggingThisObject) {
      fprintf(gCOMPtrLog, "\n<?> 0x%08X %" PRIdPTR " nsCOMPtrRelease %d 0x%08X\n",
              NS_PTR_TO_INT32(object), serialno, count ? (*count) : -1,
              NS_PTR_TO_INT32(aCOMPtr));
      nsTraceRefcnt::WalkTheStackCached(gCOMPtrLog);
    }

    UNLOCK_TRACELOG();
  }
#endif
}

void
nsTraceRefcnt::Shutdown()
{
#ifdef NS_IMPL_REFCNT_LOGGING
#ifdef STACKWALKING_AVAILABLE
  gCodeAddressService = nullptr;
#endif
  if (gBloatView) {
    PL_HashTableDestroy(gBloatView);
    gBloatView = nullptr;
  }
  if (gTypesToLog) {
    PL_HashTableDestroy(gTypesToLog);
    gTypesToLog = nullptr;
  }
  if (gObjectsToLog) {
    PL_HashTableDestroy(gObjectsToLog);
    gObjectsToLog = nullptr;
  }
  if (gSerialNumbers) {
    PL_HashTableDestroy(gSerialNumbers);
    gSerialNumbers = nullptr;
  }
  maybeUnregisterAndCloseFile(gBloatLog);
  maybeUnregisterAndCloseFile(gRefcntsLog);
  maybeUnregisterAndCloseFile(gAllocLog);
  maybeUnregisterAndCloseFile(gLeakyLog);
  maybeUnregisterAndCloseFile(gCOMPtrLog);
#endif
}

void
nsTraceRefcnt::SetActivityIsLegal(bool aLegal)
{
#ifdef NS_IMPL_REFCNT_LOGGING
  if (gActivityTLS == BAD_TLS_INDEX) {
    PR_NewThreadPrivateIndex(&gActivityTLS, nullptr);
  }

  PR_SetThreadPrivate(gActivityTLS, NS_INT32_TO_PTR(!aLegal));
#endif
}
