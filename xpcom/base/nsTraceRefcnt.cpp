/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTraceRefcnt.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Path.h"
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
#include "mozilla/StackWalk.h"
#include "nsThreadUtils.h"
#include "CodeAddressService.h"

#include "nsXULAppAPI.h"
#ifdef XP_WIN
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

#include "mozilla/Atomics.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/BlockingResourceBase.h"
#include "mozilla/PoisonIOInterposer.h"
#include "mozilla/UniquePtr.h"

#include <string>
#include <vector>

#ifdef HAVE_DLOPEN
#include <dlfcn.h>
#endif

#ifdef MOZ_DMD
#include "base/process_util.h"
#include "nsMemoryInfoDumper.h"
#endif

////////////////////////////////////////////////////////////////////////////////

#include "plhash.h"

#include "prthread.h"

// We use a spin lock instead of a regular mutex because this lock is usually
// only held for a very short time, and gets grabbed at a very high frequency
// (~100000 times per second). On Mac, the overhead of using a regular lock
// is very high, see bug 1137963.
static mozilla::Atomic<uintptr_t, mozilla::ReleaseAcquire> gTraceLogLocked;

struct MOZ_STACK_CLASS AutoTraceLogLock final
{
  bool doRelease;
  AutoTraceLogLock()
    : doRelease(true)
  {
    uintptr_t currentThread = reinterpret_cast<uintptr_t>(PR_GetCurrentThread());
    if (gTraceLogLocked == currentThread) {
      doRelease = false;
    } else {
      while (!gTraceLogLocked.compareExchange(0, currentThread)) {
        PR_Sleep(PR_INTERVAL_NO_WAIT); /* yield */
      }
    }
  }
  ~AutoTraceLogLock() { if (doRelease) gTraceLogLocked = 0; }
};

static PLHashTable* gBloatView;
static PLHashTable* gTypesToLog;
static PLHashTable* gObjectsToLog;
static PLHashTable* gSerialNumbers;
static intptr_t gNextSerialNumber;
static bool gDumpedStatistics = false;
static bool gLogJSStacks = false;

// By default, debug builds only do bloat logging. Bloat logging
// only tries to record when an object is created or destroyed, so we
// optimize the common case in NS_LogAddRef and NS_LogRelease where
// only bloat logging is enabled and no logging needs to be done.
enum LoggingType
{
  NoLogging,
  OnlyBloatLogging,
  FullLogging
};

static LoggingType gLogging;

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
static FILE* gCOMPtrLog = nullptr;

static void
WalkTheStackSavingLocations(std::vector<void*>& aLocations);

struct SerialNumberRecord
{
  SerialNumberRecord()
    : serialNumber(++gNextSerialNumber)
    , refCount(0)
    , COMPtrCount(0)
  {}

  intptr_t serialNumber;
  int32_t refCount;
  int32_t COMPtrCount;
  // We use std:: classes here rather than the XPCOM equivalents because the
  // XPCOM equivalents do leak-checking, and if you try to leak-check while
  // leak-checking, you're gonna have a bad time.
  std::vector<void*> allocationStack;
  mozilla::UniquePtr<char[]> jsStack;

  void SaveJSStack() {
    // If this thread isn't running JS, there's nothing to do.
    if (!CycleCollectedJSContext::Get()) {
      return;
    }

    JSContext* cx = nsContentUtils::GetCurrentJSContext();
    if (!cx) {
      return;
    }

    JS::UniqueChars chars = xpc_PrintJSStack(cx,
                                             /*showArgs=*/ false,
                                             /*showLocals=*/ false,
                                             /*showThisProps=*/ false);
    size_t len = strlen(chars.get());
    jsStack = MakeUnique<char[]>(len + 1);
    memcpy(jsStack.get(), chars.get(), len + 1);
  }
};

struct nsTraceRefcntStats
{
  uint64_t mCreates;
  uint64_t mDestroys;

  bool HaveLeaks() const
  {
    return mCreates != mDestroys;
  }

  void Clear()
  {
    mCreates = 0;
    mDestroys = 0;
  }

  int64_t NumLeaked() const
  {
    return (int64_t)(mCreates - mDestroys);
  }
};

#ifdef DEBUG
static const char kStaticCtorDtorWarning[] =
  "XPCOM objects created/destroyed from static ctor/dtor";

static void
AssertActivityIsLegal()
{
  if (gActivityTLS == BAD_TLS_INDEX || PR_GetThreadPrivate(gActivityTLS)) {
    if (PR_GetEnv("MOZ_FATAL_STATIC_XPCOM_CTORS_DTORS")) {
      MOZ_CRASH_UNSAFE_OOL(kStaticCtorDtorWarning);
    } else {
      NS_WARNING(kStaticCtorDtorWarning);
    }
  }
}
#  define ASSERT_ACTIVITY_IS_LEGAL              \
  do {                                          \
    AssertActivityIsLegal();                    \
  } while(0)
#else
#  define ASSERT_ACTIVITY_IS_LEGAL do { } while(0)
#endif // DEBUG

// These functions are copied from nsprpub/lib/ds/plhash.c, with changes
// to the functions not called Default* to free the SerialNumberRecord or
// the BloatEntry.

static void*
DefaultAllocTable(void* aPool, size_t aSize)
{
  return malloc(aSize);
}

static void
DefaultFreeTable(void* aPool, void* aItem)
{
  free(aItem);
}

static PLHashEntry*
DefaultAllocEntry(void* aPool, const void* aKey)
{
  return (PLHashEntry*) malloc(sizeof(PLHashEntry));
}

static void
SerialNumberFreeEntry(void* aPool, PLHashEntry* aHashEntry, unsigned aFlag)
{
  if (aFlag == HT_FREE_ENTRY) {
    delete static_cast<SerialNumberRecord*>(aHashEntry->value);
    free(aHashEntry);
  }
}

static void
TypesToLogFreeEntry(void* aPool, PLHashEntry* aHashEntry, unsigned aFlag)
{
  if (aFlag == HT_FREE_ENTRY) {
    free(const_cast<char*>(static_cast<const char*>(aHashEntry->key)));
    free(aHashEntry);
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

class CodeAddressServiceStringTable final
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

struct CodeAddressServiceStringAlloc final
{
  static char* copy(const char* aStr) { return strdup(aStr); }
  static void free(char* aPtr) { ::free(aPtr); }
};

// WalkTheStack does not hold any locks needed by MozDescribeCodeAddress, so
// this class does not need to do anything.
struct CodeAddressServiceLock final
{
  static void Unlock() {}
  static void Lock() {}
  static bool IsLocked() { return true; }
};

typedef mozilla::CodeAddressService<CodeAddressServiceStringTable,
                                    CodeAddressServiceStringAlloc,
                                    CodeAddressServiceLock> WalkTheStackCodeAddressService;

mozilla::StaticAutoPtr<WalkTheStackCodeAddressService> gCodeAddressService;

////////////////////////////////////////////////////////////////////////////////

class BloatEntry
{
public:
  BloatEntry(const char* aClassName, uint32_t aClassSize)
    : mClassSize(aClassSize)
  {
    MOZ_ASSERT(strlen(aClassName) > 0, "BloatEntry name must be non-empty");
    mClassName = PL_strdup(aClassName);
    mStats.Clear();
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

  void Ctor()
  {
    mStats.mCreates++;
  }

  void Dtor()
  {
    mStats.mDestroys++;
  }

  static int DumpEntry(PLHashEntry* aHashEntry, int aIndex, void* aArg)
  {
    BloatEntry* entry = (BloatEntry*)aHashEntry->value;
    if (entry) {
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
    aTotal->mStats.mCreates += mStats.mCreates;
    aTotal->mStats.mDestroys += mStats.mDestroys;
    aTotal->mClassSize += mClassSize * mStats.mCreates;    // adjust for average in DumpTotal
    aTotal->mTotalLeaked += mClassSize * mStats.NumLeaked();
  }

  void DumpTotal(FILE* aOut)
  {
    mClassSize /= mStats.mCreates;
    Dump(-1, aOut);
  }

  bool PrintDumpHeader(FILE* aOut, const char* aMsg)
  {
    fprintf(aOut, "\n== BloatView: %s, %s process %d\n", aMsg,
            XRE_ChildProcessTypeToString(XRE_GetProcessType()), getpid());
    if (gLogLeaksOnly && !mStats.HaveLeaks()) {
      return false;
    }

    fprintf(aOut,
            "\n" \
            "     |<----------------Class--------------->|<-----Bytes------>|<----Objects---->|\n" \
            "     |                                      | Per-Inst   Leaked|   Total      Rem|\n");

    this->DumpTotal(aOut);

    return true;
  }

  void Dump(int aIndex, FILE* aOut)
  {
    if (gLogLeaksOnly && !mStats.HaveLeaks()) {
      return;
    }

    if (mStats.HaveLeaks() || mStats.mCreates != 0) {
      fprintf(aOut, "%4d |%-38.38s| %8d %8" PRId64 "|%8" PRIu64 " %8" PRId64"|\n",
              aIndex + 1, mClassName,
              GetClassSize(),
              nsCRT::strcmp(mClassName, "TOTAL") ? (mStats.NumLeaked() * GetClassSize()) : mTotalLeaked,
              mStats.mCreates,
              mStats.NumLeaked());
    }
  }

protected:
  char* mClassName;
  double mClassSize; // This is stored as a double because of the way we compute the avg class size for total bloat.
  int64_t mTotalLeaked; // Used only for TOTAL entry.
  nsTraceRefcntStats mStats;
};

static void
BloatViewFreeEntry(void* aPool, PLHashEntry* aHashEntry, unsigned aFlag)
{
  if (aFlag == HT_FREE_ENTRY) {
    BloatEntry* entry = static_cast<BloatEntry*>(aHashEntry->value);
    delete entry;
    free(aHashEntry);
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
      MOZ_ASSERT(aInstanceSize == 0 || entry->GetClassSize() == aInstanceSize,
                 "Mismatched sizes were recorded in the memory leak logging table. "
                 "The usual cause of this is having a templated class that uses "
                 "MOZ_COUNT_{C,D}TOR in the constructor or destructor, respectively. "
                 "As a workaround, the MOZ_COUNT_{C,D}TOR calls can be moved to a "
                 "non-templated base class. Another possible cause is a runnable with "
                 "an mName that matches another refcounted class.");
    }
  }
  return entry;
}

static int
DumpSerialNumbers(PLHashEntry* aHashEntry, int aIndex, void* aClosure)
{
  SerialNumberRecord* record =
    static_cast<SerialNumberRecord*>(aHashEntry->value);
  auto* outputFile = static_cast<FILE*>(aClosure);
#ifdef HAVE_CPP_DYNAMIC_CAST_TO_VOID_PTR
  fprintf(outputFile, "%" PRIdPTR
          " @%p (%d references; %d from COMPtrs)\n",
          record->serialNumber,
          aHashEntry->key,
          record->refCount,
          record->COMPtrCount);
#else
  fprintf(outputFile, "%" PRIdPTR
          " @%p (%d references)\n",
          record->serialNumber,
          aHashEntry->key,
          record->refCount);
#endif
  if (!record->allocationStack.empty()) {
    static const size_t bufLen = 1024;
    char buf[bufLen];
    fprintf(outputFile, "allocation stack:\n");
    for (size_t i = 0, length = record->allocationStack.size();
         i < length;
         ++i) {
      gCodeAddressService->GetLocation(i, record->allocationStack[i],
                                       buf, bufLen);
      fprintf(outputFile, "%s\n", buf);
    }
  }

  if (gLogJSStacks) {
    if (record->jsStack) {
      fprintf(outputFile, "JS allocation stack:\n%s\n", record->jsStack.get());
    } else {
      fprintf(outputFile, "There is no JS context on the stack.\n");
    }
  }

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


nsresult
nsTraceRefcnt::DumpStatistics()
{
  if (!gBloatLog || !gBloatView) {
    return NS_ERROR_FAILURE;
  }

  AutoTraceLogLock lock;

  MOZ_ASSERT(!gDumpedStatistics,
             "Calling DumpStatistics more than once may result in "
             "bogus positive or negative leaks being reported");
  gDumpedStatistics = true;

  // Don't try to log while we hold the lock, we'd deadlock.
  AutoRestore<LoggingType> saveLogging(gLogging);
  gLogging = NoLogging;

  BloatEntry total("TOTAL", 0);
  PL_HashTableEnumerateEntries(gBloatView, BloatEntry::TotalEntries, &total);
  const char* msg;
  if (gLogLeaksOnly) {
    msg = "ALL (cumulative) LEAK STATISTICS";
  } else {
    msg = "ALL (cumulative) LEAK AND BLOAT STATISTICS";
  }
  const bool leaked = total.PrintDumpHeader(gBloatLog, msg);

  nsTArray<BloatEntry*> entries;
  PL_HashTableEnumerateEntries(gBloatView, BloatEntry::DumpEntry, &entries);
  const uint32_t count = entries.Length();

  if (!gLogLeaksOnly || leaked) {
    // Sort the entries alphabetically by classname.
    entries.Sort();

    for (uint32_t i = 0; i < count; ++i) {
      BloatEntry* entry = entries[i];
      entry->Dump(i, gBloatLog);
    }

    fprintf(gBloatLog, "\n");
  }

  fprintf(gBloatLog, "nsTraceRefcnt::DumpStatistics: %d entries\n", count);

  if (gSerialNumbers) {
    fprintf(gBloatLog, "\nSerial Numbers of Leaked Objects:\n");
    PL_HashTableEnumerateEntries(gSerialNumbers, DumpSerialNumbers, gBloatLog);
  }

  return NS_OK;
}

void
nsTraceRefcnt::ResetStatistics()
{
  AutoTraceLogLock lock;
  if (gBloatView) {
    PL_HashTableDestroy(gBloatView);
    gBloatView = nullptr;
  }
}

static bool
LogThisType(const char* aTypeName)
{
  void* he = PL_HashTableLookup(gTypesToLog, aTypeName);
  return he != nullptr;
}

static PLHashNumber
HashNumber(const void* aKey)
{
  return PLHashNumber(NS_PTR_TO_INT32(aKey));
}

static intptr_t
GetSerialNumber(void* aPtr, bool aCreate)
{
  PLHashEntry** hep = PL_HashTableRawLookup(gSerialNumbers,
                                            HashNumber(aPtr),
                                            aPtr);
  if (hep && *hep) {
    MOZ_RELEASE_ASSERT(!aCreate, "If an object already has a serial number, we should be destroying it.");
    return static_cast<SerialNumberRecord*>((*hep)->value)->serialNumber;
  }

  if (!aCreate) {
    return 0;
  }

  SerialNumberRecord* record = new SerialNumberRecord();
  WalkTheStackSavingLocations(record->allocationStack);
  PL_HashTableRawAdd(gSerialNumbers, hep, HashNumber(aPtr),
                     aPtr, static_cast<void*>(record));
  if (gLogJSStacks) {
    record->SaveJSStack();
  }
  return gNextSerialNumber;
}

static int32_t*
GetRefCount(void* aPtr)
{
  PLHashEntry** hep = PL_HashTableRawLookup(gSerialNumbers,
                                            HashNumber(aPtr),
                                            aPtr);
  if (hep && *hep) {
    return &(static_cast<SerialNumberRecord*>((*hep)->value)->refCount);
  } else {
    return nullptr;
  }
}

#ifdef HAVE_CPP_DYNAMIC_CAST_TO_VOID_PTR
static int32_t*
GetCOMPtrCount(void* aPtr)
{
  PLHashEntry** hep = PL_HashTableRawLookup(gSerialNumbers,
                                            HashNumber(aPtr),
                                            aPtr);
  if (hep && *hep) {
    return &(static_cast<SerialNumberRecord*>((*hep)->value)->COMPtrCount);
  }
  return nullptr;
}
#endif // HAVE_CPP_DYNAMIC_CAST_TO_VOID_PTR

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

using EnvCharType = mozilla::filesystem::Path::value_type;

static bool
InitLog(const EnvCharType* aEnvVar, const char* aMsg, FILE** aResult)
{
#ifdef XP_WIN
  // This is gross, I know.
  const wchar_t* envvar = reinterpret_cast<const wchar_t*>(aEnvVar);
  const char16_t* value = reinterpret_cast<const char16_t*>(::_wgetenv(envvar));
#define ENVVAR_PRINTF "%S"
#else
  const char* envvar = aEnvVar;
  const char* value = ::getenv(aEnvVar);
#define ENVVAR_PRINTF "%s"
#endif

  if (value) {
    nsTDependentString<EnvCharType> fname(value);
    if (fname.EqualsLiteral("1")) {
      *aResult = stdout;
      fprintf(stdout, "### " ENVVAR_PRINTF " defined -- logging %s to stdout\n",
              envvar, aMsg);
      return true;
    } else if (fname.EqualsLiteral("2")) {
      *aResult = stderr;
      fprintf(stdout, "### " ENVVAR_PRINTF " defined -- logging %s to stderr\n",
              envvar, aMsg);
      return true;
    } else {
      if (!XRE_IsParentProcess()) {
        bool hasLogExtension =
          fname.RFind(".log", true, -1, 4) == kNotFound ? false : true;
        if (hasLogExtension) {
          fname.Cut(fname.Length() - 4, 4);
        }
        fname.Append('_');
        const char* processType = XRE_ChildProcessTypeToString(XRE_GetProcessType());
        fname.AppendASCII(processType);
        fname.AppendLiteral("_pid");
        fname.AppendInt((uint32_t)getpid());
        if (hasLogExtension) {
          fname.AppendLiteral(".log");
        }
      }
#ifdef XP_WIN
      FILE* stream = ::_wfopen(fname.get(), L"wN");
      const wchar_t* fp = (const wchar_t*)fname.get();
#else
      FILE* stream = ::fopen(fname.get(), "w");
      const char* fp = fname.get();
#endif
      if (stream) {
        MozillaRegisterDebugFD(fileno(stream));
        *aResult = stream;
        fprintf(stderr, "### " ENVVAR_PRINTF " defined -- logging %s to " ENVVAR_PRINTF "\n",
                envvar, aMsg, fp);
      } else {
        fprintf(stderr, "### " ENVVAR_PRINTF " defined -- unable to log %s to " ENVVAR_PRINTF "\n",
                envvar, aMsg, fp);
        MOZ_ASSERT(false, "Tried and failed to create an XPCOM log");
      }
#undef ENVVAR_PRINTF
      return stream != nullptr;
    }
  }
  return false;
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
#ifdef XP_WIN
#define ENVVAR(x) u"" x
#else
#define ENVVAR(x) x
#endif

  if (gInitialized) {
    return;
  }
  gInitialized = true;

  bool defined = InitLog(ENVVAR("XPCOM_MEM_BLOAT_LOG"), "bloat/leaks", &gBloatLog);
  if (!defined) {
    gLogLeaksOnly = InitLog(ENVVAR("XPCOM_MEM_LEAK_LOG"), "leaks", &gBloatLog);
  }
  if (defined || gLogLeaksOnly) {
    RecreateBloatView();
    if (!gBloatView) {
      NS_WARNING("out of memory");
      maybeUnregisterAndCloseFile(gBloatLog);
      gLogLeaksOnly = false;
    }
  }

  InitLog(ENVVAR("XPCOM_MEM_REFCNT_LOG"), "refcounts", &gRefcntsLog);

  InitLog(ENVVAR("XPCOM_MEM_ALLOC_LOG"), "new/delete", &gAllocLog);

  const char* classes = getenv("XPCOM_MEM_LOG_CLASSES");

#ifdef HAVE_CPP_DYNAMIC_CAST_TO_VOID_PTR
  if (classes) {
    InitLog(ENVVAR("XPCOM_MEM_COMPTR_LOG"), "nsCOMPtr", &gCOMPtrLog);
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
#endif // HAVE_CPP_DYNAMIC_CAST_TO_VOID_PTR

#undef ENVVAR

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

  if (getenv("XPCOM_MEM_LOG_JS_STACK")) {
    fprintf(stdout, "### XPCOM_MEM_LOG_JS_STACK defined\n");
    gLogJSStacks = true;
  }

  if (gBloatLog) {
    gLogging = OnlyBloatLogging;
  }

  if (gRefcntsLog || gAllocLog || gCOMPtrLog) {
    gLogging = FullLogging;
  }
}


extern "C" {

static void
PrintStackFrame(uint32_t aFrameNumber, void* aPC, void* aSP, void* aClosure)
{
  FILE* stream = (FILE*)aClosure;
  MozCodeAddressDetails details;
  char buf[1024];

  MozDescribeCodeAddress(aPC, &details);
  MozFormatCodeAddressDetails(buf, sizeof(buf), aFrameNumber, aPC, &details);
  fprintf(stream, "%s\n", buf);
  fflush(stream);
}

static void
PrintStackFrameCached(uint32_t aFrameNumber, void* aPC, void* aSP,
                      void* aClosure)
{
  auto stream = static_cast<FILE*>(aClosure);
  static const size_t buflen = 1024;
  char buf[buflen];
  gCodeAddressService->GetLocation(aFrameNumber, aPC, buf, buflen);
  fprintf(stream, "    %s\n", buf);
  fflush(stream);
}

static void
RecordStackFrame(uint32_t /*aFrameNumber*/, void* aPC, void* /*aSP*/,
                 void* aClosure)
{
  auto locations = static_cast<std::vector<void*>*>(aClosure);
  locations->push_back(aPC);
}

}

void
nsTraceRefcnt::WalkTheStack(FILE* aStream)
{
  MozStackWalk(PrintStackFrame, /* skipFrames */ 2, /* maxFrames */ 0, aStream);
}

/**
 * This is a variant of |WalkTheStack| that uses |CodeAddressService| to cache
 * the results of |NS_DescribeCodeAddress|. If |WalkTheStackCached| is being
 * called frequently, it will be a few orders of magnitude faster than
 * |WalkTheStack|. However, the cache uses a lot of memory, which can cause
 * OOM crashes. Therefore, this should only be used for things like refcount
 * logging which walk the stack extremely frequently.
 */
static void
WalkTheStackCached(FILE* aStream)
{
  if (!gCodeAddressService) {
    gCodeAddressService = new WalkTheStackCodeAddressService();
  }
  MozStackWalk(PrintStackFrameCached, /* skipFrames */ 2, /* maxFrames */ 0,
               aStream);
}

static void
WalkTheStackSavingLocations(std::vector<void*>& aLocations)
{
  if (!gCodeAddressService) {
    gCodeAddressService = new WalkTheStackCodeAddressService();
  }
  static const int kFramesToSkip =
    0 +                         // this frame gets inlined
    1 +                         // GetSerialNumber
    1;                          // NS_LogCtor
  MozStackWalk(RecordStackFrame, kFramesToSkip, /* maxFrames */ 0, &aLocations);
}

//----------------------------------------------------------------------

EXPORT_XPCOM_API(void)
NS_LogInit()
{
  NS_SetMainThread();

  // FIXME: This is called multiple times, we should probably not allow that.
  if (++gInitCount) {
    nsTraceRefcnt::SetActivityIsLegal(true);
  }
}

EXPORT_XPCOM_API(void)
NS_LogTerm()
{
  mozilla::LogTerm();
}

#ifdef MOZ_DMD
// If MOZ_DMD_SHUTDOWN_LOG is set, dump a DMD report to a file.
// The value of this environment variable is used as the prefix
// of the file name, so you probably want something like "/tmp/".
// By default, this is run in all processes, but you can record a
// log only for a specific process type by setting MOZ_DMD_LOG_PROCESS
// to the process type you want to log, such as "default" or "tab".
// This method can't use the higher level XPCOM file utilities
// because it is run very late in shutdown to avoid recording
// information about refcount logging entries.
static void
LogDMDFile()
{
  const char* dmdFilePrefix = PR_GetEnv("MOZ_DMD_SHUTDOWN_LOG");
  if (!dmdFilePrefix) {
    return;
  }

  const char* logProcessEnv = PR_GetEnv("MOZ_DMD_LOG_PROCESS");
  if (logProcessEnv && !!strcmp(logProcessEnv, XRE_ChildProcessTypeToString(XRE_GetProcessType()))) {
    return;
  }

  nsPrintfCString fileName("%sdmd-%d.log.gz", dmdFilePrefix, base::GetCurrentProcId());
  FILE* logFile = fopen(fileName.get(), "w");
  if (NS_WARN_IF(!logFile)) {
    return;
  }

  nsMemoryInfoDumper::DumpDMDToFile(logFile);
}
#endif // MOZ_DMD

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
    nsTraceRefcnt::SetActivityIsLegal(false);
    gActivityTLS = BAD_TLS_INDEX;

#ifdef MOZ_DMD
    LogDMDFile();
#endif
  }
}

} // namespace mozilla

EXPORT_XPCOM_API(void)
NS_LogAddRef(void* aPtr, nsrefcnt aRefcnt,
             const char* aClass, uint32_t aClassSize)
{
  ASSERT_ACTIVITY_IS_LEGAL;
  if (!gInitialized) {
    InitTraceLog();
  }
  if (gLogging == NoLogging) {
    return;
  }
  if (aRefcnt == 1 || gLogging == FullLogging) {
    AutoTraceLogLock lock;

    if (aRefcnt == 1 && gBloatLog) {
      BloatEntry* entry = GetBloatEntry(aClass, aClassSize);
      if (entry) {
        entry->Ctor();
      }
    }

    // Here's the case where MOZ_COUNT_CTOR was not used,
    // yet we still want to see creation information:

    bool loggingThisType = (!gTypesToLog || LogThisType(aClass));
    intptr_t serialno = 0;
    if (gSerialNumbers && loggingThisType) {
      serialno = GetSerialNumber(aPtr, aRefcnt == 1);
      MOZ_ASSERT(serialno != 0,
                 "Serial number requested for unrecognized pointer!  "
                 "Are you memmoving a refcounted object?");
      int32_t* count = GetRefCount(aPtr);
      if (count) {
        (*count)++;
      }

    }

    bool loggingThisObject = (!gObjectsToLog || LogThisObj(serialno));
    if (aRefcnt == 1 && gAllocLog && loggingThisType && loggingThisObject) {
      fprintf(gAllocLog, "\n<%s> %p %" PRIdPTR " Create [thread %p]\n", aClass, aPtr, serialno, PR_GetCurrentThread());
      WalkTheStackCached(gAllocLog);
    }

    if (gRefcntsLog && loggingThisType && loggingThisObject) {
      // Can't use MOZ_LOG(), b/c it truncates the line
      fprintf(gRefcntsLog, "\n<%s> %p %" PRIuPTR " AddRef %" PRIuPTR " [thread %p]\n",
              aClass, aPtr, serialno, aRefcnt, PR_GetCurrentThread());
      WalkTheStackCached(gRefcntsLog);
      fflush(gRefcntsLog);
    }
  }
}

EXPORT_XPCOM_API(void)
NS_LogRelease(void* aPtr, nsrefcnt aRefcnt, const char* aClass)
{
  ASSERT_ACTIVITY_IS_LEGAL;
  if (!gInitialized) {
    InitTraceLog();
  }
  if (gLogging == NoLogging) {
    return;
  }
  if (aRefcnt == 0 || gLogging == FullLogging) {
    AutoTraceLogLock lock;

    if (aRefcnt == 0 && gBloatLog) {
      BloatEntry* entry = GetBloatEntry(aClass, 0);
      if (entry) {
        entry->Dtor();
      }
    }

    bool loggingThisType = (!gTypesToLog || LogThisType(aClass));
    intptr_t serialno = 0;
    if (gSerialNumbers && loggingThisType) {
      serialno = GetSerialNumber(aPtr, false);
      MOZ_ASSERT(serialno != 0,
                 "Serial number requested for unrecognized pointer!  "
                 "Are you memmoving a refcounted object?");
      int32_t* count = GetRefCount(aPtr);
      if (count) {
        (*count)--;
      }

    }

    bool loggingThisObject = (!gObjectsToLog || LogThisObj(serialno));
    if (gRefcntsLog && loggingThisType && loggingThisObject) {
      // Can't use MOZ_LOG(), b/c it truncates the line
      fprintf(gRefcntsLog,
              "\n<%s> %p %" PRIuPTR " Release %" PRIuPTR " [thread %p]\n",
              aClass, aPtr, serialno, aRefcnt, PR_GetCurrentThread());
      WalkTheStackCached(gRefcntsLog);
      fflush(gRefcntsLog);
    }

    // Here's the case where MOZ_COUNT_DTOR was not used,
    // yet we still want to see deletion information:

    if (aRefcnt == 0 && gAllocLog && loggingThisType && loggingThisObject) {
      fprintf(gAllocLog, "\n<%s> %p %" PRIdPTR " Destroy [thread %p]\n", aClass, aPtr, serialno, PR_GetCurrentThread());
      WalkTheStackCached(gAllocLog);
    }

    if (aRefcnt == 0 && gSerialNumbers && loggingThisType) {
      RecycleSerialNumberPtr(aPtr);
    }
  }
}

EXPORT_XPCOM_API(void)
NS_LogCtor(void* aPtr, const char* aType, uint32_t aInstanceSize)
{
  ASSERT_ACTIVITY_IS_LEGAL;
  if (!gInitialized) {
    InitTraceLog();
  }

  if (gLogging == NoLogging) {
    return;
  }

  AutoTraceLogLock lock;

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
    MOZ_ASSERT(serialno != 0, "GetSerialNumber should never return 0 when passed true");
  }

  bool loggingThisObject = (!gObjectsToLog || LogThisObj(serialno));
  if (gAllocLog && loggingThisType && loggingThisObject) {
    fprintf(gAllocLog, "\n<%s> %p %" PRIdPTR " Ctor (%d)\n",
            aType, aPtr, serialno, aInstanceSize);
    WalkTheStackCached(gAllocLog);
  }
}


EXPORT_XPCOM_API(void)
NS_LogDtor(void* aPtr, const char* aType, uint32_t aInstanceSize)
{
  ASSERT_ACTIVITY_IS_LEGAL;
  if (!gInitialized) {
    InitTraceLog();
  }

  if (gLogging == NoLogging) {
    return;
  }

  AutoTraceLogLock lock;

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
    MOZ_ASSERT(serialno != 0,
               "Serial number requested for unrecognized pointer!  "
               "Are you memmoving a MOZ_COUNT_CTOR-tracked object?");
    RecycleSerialNumberPtr(aPtr);
  }

  bool loggingThisObject = (!gObjectsToLog || LogThisObj(serialno));

  // (If we're on a losing architecture, don't do this because we'll be
  // using LogDeleteXPCOM instead to get file and line numbers.)
  if (gAllocLog && loggingThisType && loggingThisObject) {
    fprintf(gAllocLog, "\n<%s> %p %" PRIdPTR " Dtor (%d)\n",
            aType, aPtr, serialno, aInstanceSize);
    WalkTheStackCached(gAllocLog);
  }
}


EXPORT_XPCOM_API(void)
NS_LogCOMPtrAddRef(void* aCOMPtr, nsISupports* aObject)
{
#ifdef HAVE_CPP_DYNAMIC_CAST_TO_VOID_PTR
  // Get the most-derived object.
  void* object = dynamic_cast<void*>(aObject);

  // This is a very indirect way of finding out what the class is
  // of the object being logged.  If we're logging a specific type,
  // then
  if (!gTypesToLog || !gSerialNumbers) {
    return;
  }
  if (!gInitialized) {
    InitTraceLog();
  }
  if (gLogging == FullLogging) {
    AutoTraceLogLock lock;

    intptr_t serialno = GetSerialNumber(object, false);
    if (serialno == 0) {
      return;
    }

    int32_t* count = GetCOMPtrCount(object);
    if (count) {
      (*count)++;
    }

    bool loggingThisObject = (!gObjectsToLog || LogThisObj(serialno));

    if (gCOMPtrLog && loggingThisObject) {
      fprintf(gCOMPtrLog, "\n<?> %p %" PRIdPTR " nsCOMPtrAddRef %d %p\n",
              object, serialno, count ? (*count) : -1, aCOMPtr);
      WalkTheStackCached(gCOMPtrLog);
    }
  }
#endif // HAVE_CPP_DYNAMIC_CAST_TO_VOID_PTR
}


EXPORT_XPCOM_API(void)
NS_LogCOMPtrRelease(void* aCOMPtr, nsISupports* aObject)
{
#ifdef HAVE_CPP_DYNAMIC_CAST_TO_VOID_PTR
  // Get the most-derived object.
  void* object = dynamic_cast<void*>(aObject);

  // This is a very indirect way of finding out what the class is
  // of the object being logged.  If we're logging a specific type,
  // then
  if (!gTypesToLog || !gSerialNumbers) {
    return;
  }
  if (!gInitialized) {
    InitTraceLog();
  }
  if (gLogging == FullLogging) {
    AutoTraceLogLock lock;

    intptr_t serialno = GetSerialNumber(object, false);
    if (serialno == 0) {
      return;
    }

    int32_t* count = GetCOMPtrCount(object);
    if (count) {
      (*count)--;
    }

    bool loggingThisObject = (!gObjectsToLog || LogThisObj(serialno));

    if (gCOMPtrLog && loggingThisObject) {
      fprintf(gCOMPtrLog, "\n<?> %p %" PRIdPTR " nsCOMPtrRelease %d %p\n",
              object, serialno, count ? (*count) : -1, aCOMPtr);
      WalkTheStackCached(gCOMPtrLog);
    }
  }
#endif // HAVE_CPP_DYNAMIC_CAST_TO_VOID_PTR
}

void
nsTraceRefcnt::Shutdown()
{
  gCodeAddressService = nullptr;
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
  maybeUnregisterAndCloseFile(gCOMPtrLog);
}

void
nsTraceRefcnt::SetActivityIsLegal(bool aLegal)
{
  if (gActivityTLS == BAD_TLS_INDEX) {
    PR_NewThreadPrivateIndex(&gActivityTLS, nullptr);
  }

  PR_SetThreadPrivate(gActivityTLS, reinterpret_cast<void*>(!aLegal));
}
