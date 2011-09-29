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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsTraceRefcntImpl.h"
#include "nsXPCOMPrivate.h"
#include "nscore.h"
#include "nsISupports.h"
#include "nsTArray.h"
#include "prenv.h"
#include "prprf.h"
#include "prlog.h"
#include "plstr.h"
#include "prlink.h"
#include <stdlib.h>
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include <math.h>
#include "nsStackWalk.h"
#include "nsString.h"

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

#ifdef HAVE_DLOPEN
#include <dlfcn.h>
#endif

////////////////////////////////////////////////////////////////////////////////

void
NS_MeanAndStdDev(double n, double sumOfValues, double sumOfSquaredValues,
                 double *meanResult, double *stdDevResult)
{
  double mean = 0.0, var = 0.0, stdDev = 0.0;
  if (n > 0.0 && sumOfValues >= 0) {
    mean = sumOfValues / n;
    double temp = (n * sumOfSquaredValues) - (sumOfValues * sumOfValues);
    if (temp < 0.0 || n <= 1)
      var = 0.0;
    else
      var = temp / (n * (n - 1));
    // for some reason, Windows says sqrt(0.0) is "-1.#J" (?!) so do this:
    stdDev = var != 0.0 ? sqrt(var) : 0.0;
  }
  *meanResult = mean;
  *stdDevResult = stdDev;
}

////////////////////////////////////////////////////////////////////////////////

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
static PRInt32 gNextSerialNumber;

static bool gLogging;
static bool gLogToLeaky;
static bool gLogLeaksOnly;

static void (*leakyLogAddRef)(void* p, int oldrc, int newrc);
static void (*leakyLogRelease)(void* p, int oldrc, int newrc);

#define BAD_TLS_INDEX ((PRUintn) -1)

// if gActivityTLS == BAD_TLS_INDEX, then we're
// unitialized... otherwise this points to a NSPR TLS thread index
// indicating whether addref activity is legal. If the PTR_TO_INT32 is 0 then
// activity is ok, otherwise not!
static PRUintn gActivityTLS = BAD_TLS_INDEX;

static bool gInitialized;
static nsrefcnt gInitCount;

static FILE *gBloatLog = nsnull;
static FILE *gRefcntsLog = nsnull;
static FILE *gAllocLog = nsnull;
static FILE *gLeakyLog = nsnull;
static FILE *gCOMPtrLog = nsnull;

struct serialNumberRecord {
  PRInt32 serialNumber;
  PRInt32 refCount;
  PRInt32 COMPtrCount;
};

struct nsTraceRefcntStats {
  PRUint64 mAddRefs;
  PRUint64 mReleases;
  PRUint64 mCreates;
  PRUint64 mDestroys;
  double mRefsOutstandingTotal;
  double mRefsOutstandingSquared;
  double mObjsOutstandingTotal;
  double mObjsOutstandingSquared;
};

  // I hope to turn this on for everybody once we hit it a little less.
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

static void *
DefaultAllocTable(void *pool, PRSize size)
{
    return PR_MALLOC(size);
}

static void
DefaultFreeTable(void *pool, void *item)
{
    PR_Free(item);
}

static PLHashEntry *
DefaultAllocEntry(void *pool, const void *key)
{
    return PR_NEW(PLHashEntry);
}

static void
SerialNumberFreeEntry(void *pool, PLHashEntry *he, PRUintn flag)
{
    if (flag == HT_FREE_ENTRY) {
        PR_Free(reinterpret_cast<serialNumberRecord*>(he->value));
        PR_Free(he);
    }
}

static void
TypesToLogFreeEntry(void *pool, PLHashEntry *he, PRUintn flag)
{
    if (flag == HT_FREE_ENTRY) {
        nsCRT::free(const_cast<char*>
                              (reinterpret_cast<const char*>(he->key)));
        PR_Free(he);
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

class BloatEntry {
public:
  BloatEntry(const char* className, PRUint32 classSize)
    : mClassSize(classSize) {
    mClassName = PL_strdup(className);
    Clear(&mNewStats);
    Clear(&mAllStats);
    mTotalLeaked = 0;
  }

  ~BloatEntry() {
    PL_strfree(mClassName);
  }

  PRUint32 GetClassSize() { return (PRUint32)mClassSize; }
  const char* GetClassName() { return mClassName; }

  static void Clear(nsTraceRefcntStats* stats) {
    stats->mAddRefs = 0;
    stats->mReleases = 0;
    stats->mCreates = 0;
    stats->mDestroys = 0;
    stats->mRefsOutstandingTotal = 0;
    stats->mRefsOutstandingSquared = 0;
    stats->mObjsOutstandingTotal = 0;
    stats->mObjsOutstandingSquared = 0;
  }

  void Accumulate() {
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

  void AddRef(nsrefcnt refcnt) {
    mNewStats.mAddRefs++;
    if (refcnt == 1) {
      Ctor();
    }
    AccountRefs();
  }

  void Release(nsrefcnt refcnt) {
    mNewStats.mReleases++;
    if (refcnt == 0) {
      Dtor();
    }
    AccountRefs();
  }

  void Ctor() {
    mNewStats.mCreates++;
    AccountObjs();
  }

  void Dtor() {
    mNewStats.mDestroys++;
    AccountObjs();
  }

  void AccountRefs() {
    PRUint64 cnt = (mNewStats.mAddRefs - mNewStats.mReleases);
    mNewStats.mRefsOutstandingTotal += cnt;
    mNewStats.mRefsOutstandingSquared += cnt * cnt;
  }

  void AccountObjs() {
    PRUint64 cnt = (mNewStats.mCreates - mNewStats.mDestroys);
    mNewStats.mObjsOutstandingTotal += cnt;
    mNewStats.mObjsOutstandingSquared += cnt * cnt;
  }

  static PRIntn DumpEntry(PLHashEntry *he, PRIntn i, void *arg) {
    BloatEntry* entry = (BloatEntry*)he->value;
    if (entry) {
      entry->Accumulate();
      static_cast<nsTArray<BloatEntry*>*>(arg)->AppendElement(entry);
    }
    return HT_ENUMERATE_NEXT;
  }

  static PRIntn TotalEntries(PLHashEntry *he, PRIntn i, void *arg) {
    BloatEntry* entry = (BloatEntry*)he->value;
    if (entry && nsCRT::strcmp(entry->mClassName, "TOTAL") != 0) {
      entry->Total((BloatEntry*)arg);
    }
    return HT_ENUMERATE_NEXT;
  }

  void Total(BloatEntry* total) {
    total->mAllStats.mAddRefs += mNewStats.mAddRefs + mAllStats.mAddRefs;
    total->mAllStats.mReleases += mNewStats.mReleases + mAllStats.mReleases;
    total->mAllStats.mCreates += mNewStats.mCreates + mAllStats.mCreates;
    total->mAllStats.mDestroys += mNewStats.mDestroys + mAllStats.mDestroys;
    total->mAllStats.mRefsOutstandingTotal += mNewStats.mRefsOutstandingTotal + mAllStats.mRefsOutstandingTotal;
    total->mAllStats.mRefsOutstandingSquared += mNewStats.mRefsOutstandingSquared + mAllStats.mRefsOutstandingSquared;
    total->mAllStats.mObjsOutstandingTotal += mNewStats.mObjsOutstandingTotal + mAllStats.mObjsOutstandingTotal;
    total->mAllStats.mObjsOutstandingSquared += mNewStats.mObjsOutstandingSquared + mAllStats.mObjsOutstandingSquared;
    PRUint64 count = (mNewStats.mCreates + mAllStats.mCreates);
    total->mClassSize += mClassSize * count;    // adjust for average in DumpTotal
    total->mTotalLeaked += (PRUint64)(mClassSize *
                                     ((mNewStats.mCreates + mAllStats.mCreates)
                                      -(mNewStats.mDestroys + mAllStats.mDestroys)));
  }

  void DumpTotal(FILE* out) {
    mClassSize /= mAllStats.mCreates;
    Dump(-1, out, nsTraceRefcntImpl::ALL_STATS);
  }

  static bool HaveLeaks(nsTraceRefcntStats* stats) {
    return ((stats->mAddRefs != stats->mReleases) ||
            (stats->mCreates != stats->mDestroys));
  }

  bool PrintDumpHeader(FILE* out, const char* msg, nsTraceRefcntImpl::StatisticsType type) {
    fprintf(out, "\n== BloatView: %s, %s process %d\n", msg,
            XRE_ChildProcessTypeToString(XRE_GetProcessType()), getpid());
    nsTraceRefcntStats& stats =
      (type == nsTraceRefcntImpl::NEW_STATS) ? mNewStats : mAllStats;
    if (gLogLeaksOnly && !HaveLeaks(&stats))
      return PR_FALSE;

    fprintf(out,
        "\n" \
        "     |<----------------Class--------------->|<-----Bytes------>|<----------------Objects---------------->|<--------------References-------------->|\n" \
        "                                              Per-Inst   Leaked    Total      Rem      Mean       StdDev     Total      Rem      Mean       StdDev\n");

    this->DumpTotal(out);

    return PR_TRUE;
  }

  void Dump(PRIntn i, FILE* out, nsTraceRefcntImpl::StatisticsType type) {
    nsTraceRefcntStats* stats = (type == nsTraceRefcntImpl::NEW_STATS) ? &mNewStats : &mAllStats;
    if (gLogLeaksOnly && !HaveLeaks(stats)) {
      return;
    }

    double meanRefs, stddevRefs;
    NS_MeanAndStdDev(stats->mAddRefs + stats->mReleases,
                     stats->mRefsOutstandingTotal,
                     stats->mRefsOutstandingSquared,
                     &meanRefs, &stddevRefs);

    double meanObjs, stddevObjs;
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
      fprintf(out, "%4d %-40.40s %8d %8llu %8llu %8llu (%8.2f +/- %8.2f) %8llu %8llu (%8.2f +/- %8.2f)\n",
              i+1, mClassName,
              (PRInt32)mClassSize,
              (nsCRT::strcmp(mClassName, "TOTAL"))
                  ?(PRUint64)((stats->mCreates - stats->mDestroys) * mClassSize)
                  :mTotalLeaked,
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
  PRUint64      mTotalLeaked; // used only for TOTAL entry
  nsTraceRefcntStats mNewStats;
  nsTraceRefcntStats mAllStats;
};

static void
BloatViewFreeEntry(void *pool, PLHashEntry *he, PRUintn flag)
{
    if (flag == HT_FREE_ENTRY) {
        BloatEntry* entry = reinterpret_cast<BloatEntry*>(he->value);
        delete entry;
        PR_Free(he);
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
                               &bloatViewHashAllocOps, NULL);
}

static BloatEntry*
GetBloatEntry(const char* aTypeName, PRUint32 aInstanceSize)
{
  if (!gBloatView) {
    RecreateBloatView();
  }
  BloatEntry* entry = NULL;
  if (gBloatView) {
    entry = (BloatEntry*)PL_HashTableLookup(gBloatView, aTypeName);
    if (entry == NULL && aInstanceSize > 0) {

      entry = new BloatEntry(aTypeName, aInstanceSize);
      PLHashEntry* e = PL_HashTableAdd(gBloatView, aTypeName, entry);
      if (e == NULL) {
        delete entry;
        entry = NULL;
      }
    } else {
      NS_ASSERTION(aInstanceSize == 0 ||
                   entry->GetClassSize() == aInstanceSize,
                   "bad size recorded");
    }
  }
  return entry;
}

static PRIntn DumpSerialNumbers(PLHashEntry* aHashEntry, PRIntn aIndex, void* aClosure)
{
  serialNumberRecord* record = reinterpret_cast<serialNumberRecord *>(aHashEntry->value);
#ifdef HAVE_CPP_DYNAMIC_CAST_TO_VOID_PTR
  fprintf((FILE*) aClosure, "%d @%p (%d references; %d from COMPtrs)\n",
                            record->serialNumber,
                            NS_INT32_TO_PTR(aHashEntry->key),
                            record->refCount,
                            record->COMPtrCount);
#else
  fprintf((FILE*) aClosure, "%d @%p (%d references)\n",
                            record->serialNumber,
                            NS_INT32_TO_PTR(aHashEntry->key),
                            record->refCount);
#endif
  return HT_ENUMERATE_NEXT;
}


NS_SPECIALIZE_TEMPLATE
class nsDefaultComparator <BloatEntry*, BloatEntry*> {
  public:
    bool Equals(BloatEntry* const& aA, BloatEntry* const& aB) const {
      return PL_strcmp(aA->GetClassName(), aB->GetClassName()) == 0;
    }
    bool LessThan(BloatEntry* const& aA, BloatEntry* const& aB) const {
      return PL_strcmp(aA->GetClassName(), aB->GetClassName()) < 0;
    }
};

#endif /* NS_IMPL_REFCNT_LOGGING */

nsresult
nsTraceRefcntImpl::DumpStatistics(StatisticsType type, FILE* out)
{
#ifdef NS_IMPL_REFCNT_LOGGING
  if (gBloatLog == nsnull || gBloatView == nsnull) {
    return NS_ERROR_FAILURE;
  }
  if (out == nsnull) {
    out = gBloatLog;
  }

  LOCK_TRACELOG();

  bool wasLogging = gLogging;
  gLogging = PR_FALSE;  // turn off logging for this method

  BloatEntry total("TOTAL", 0);
  PL_HashTableEnumerateEntries(gBloatView, BloatEntry::TotalEntries, &total);
  const char* msg;
  if (type == NEW_STATS) {
    if (gLogLeaksOnly)
      msg = "NEW (incremental) LEAK STATISTICS";
    else
      msg = "NEW (incremental) LEAK AND BLOAT STATISTICS";
  }
  else {
    if (gLogLeaksOnly)
      msg = "ALL (cumulative) LEAK STATISTICS";
    else
      msg = "ALL (cumulative) LEAK AND BLOAT STATISTICS";
  }
  const bool leaked = total.PrintDumpHeader(out, msg, type);

  nsTArray<BloatEntry*> entries;
  PL_HashTableEnumerateEntries(gBloatView, BloatEntry::DumpEntry, &entries);
  const PRUint32 count = entries.Length();

  if (!gLogLeaksOnly || leaked) {
    // Sort the entries alphabetically by classname.
    entries.Sort();

    for (PRUint32 i = 0; i < count; ++i) {
      BloatEntry* entry = entries[i];
      entry->Dump(i, out, type);
    }

    fprintf(out, "\n");
  }

  fprintf(out, "nsTraceRefcntImpl::DumpStatistics: %d entries\n", count);

  if (gSerialNumbers) {
    fprintf(out, "\nSerial Numbers of Leaked Objects:\n");
    PL_HashTableEnumerateEntries(gSerialNumbers, DumpSerialNumbers, out);
  }

  gLogging = wasLogging;
  UNLOCK_TRACELOG();
#endif

  return NS_OK;
}

void
nsTraceRefcntImpl::ResetStatistics()
{
#ifdef NS_IMPL_REFCNT_LOGGING
  LOCK_TRACELOG();
  if (gBloatView) {
    PL_HashTableDestroy(gBloatView);
    gBloatView = nsnull;
  }
  UNLOCK_TRACELOG();
#endif
}

#ifdef NS_IMPL_REFCNT_LOGGING
static bool LogThisType(const char* aTypeName)
{
  void* he = PL_HashTableLookup(gTypesToLog, aTypeName);
  return nsnull != he;
}

static PRInt32 GetSerialNumber(void* aPtr, bool aCreate)
{
  PLHashEntry** hep = PL_HashTableRawLookup(gSerialNumbers, PLHashNumber(NS_PTR_TO_INT32(aPtr)), aPtr);
  if (hep && *hep) {
    return PRInt32((reinterpret_cast<serialNumberRecord*>((*hep)->value))->serialNumber);
  }
  else if (aCreate) {
    serialNumberRecord *record = PR_NEW(serialNumberRecord);
    record->serialNumber = ++gNextSerialNumber;
    record->refCount = 0;
    record->COMPtrCount = 0;
    PL_HashTableRawAdd(gSerialNumbers, hep, PLHashNumber(NS_PTR_TO_INT32(aPtr)), aPtr, reinterpret_cast<void*>(record));
    return gNextSerialNumber;
  }
  else {
    return 0;
  }
}

static PRInt32* GetRefCount(void* aPtr)
{
  PLHashEntry** hep = PL_HashTableRawLookup(gSerialNumbers, PLHashNumber(NS_PTR_TO_INT32(aPtr)), aPtr);
  if (hep && *hep) {
    return &((reinterpret_cast<serialNumberRecord*>((*hep)->value))->refCount);
  } else {
    return nsnull;
  }
}

static PRInt32* GetCOMPtrCount(void* aPtr)
{
  PLHashEntry** hep = PL_HashTableRawLookup(gSerialNumbers, PLHashNumber(NS_PTR_TO_INT32(aPtr)), aPtr);
  if (hep && *hep) {
    return &((reinterpret_cast<serialNumberRecord*>((*hep)->value))->COMPtrCount);
  } else {
    return nsnull;
  }
}

static void RecycleSerialNumberPtr(void* aPtr)
{
  PL_HashTableRemove(gSerialNumbers, aPtr);
}

static bool LogThisObj(PRInt32 aSerialNumber)
{
  return nsnull != PL_HashTableLookup(gObjectsToLog, (const void*)(aSerialNumber));
}

#ifdef XP_WIN
#define FOPEN_NO_INHERIT "N"
#else
#define FOPEN_NO_INHERIT
#endif

static bool InitLog(const char* envVar, const char* msg, FILE* *result)
{
  const char* value = getenv(envVar);
  if (value) {
    if (nsCRT::strcmp(value, "1") == 0) {
      *result = stdout;
      fprintf(stdout, "### %s defined -- logging %s to stdout\n",
              envVar, msg);
      return PR_TRUE;
    }
    else if (nsCRT::strcmp(value, "2") == 0) {
      *result = stderr;
      fprintf(stdout, "### %s defined -- logging %s to stderr\n",
              envVar, msg);
      return PR_TRUE;
    }
    else {
      FILE *stream;
      nsCAutoString fname(value);
      if (XRE_GetProcessType() != GeckoProcessType_Default) {
        bool hasLogExtension = 
            fname.RFind(".log", PR_TRUE, -1, 4) == kNotFound ? false : true;
        if (hasLogExtension)
          fname.Cut(fname.Length() - 4, 4);
        fname.AppendLiteral("_");
        fname.Append((char*)XRE_ChildProcessTypeToString(XRE_GetProcessType()));
        fname.AppendLiteral("_pid");
        fname.AppendInt((PRUint32)getpid());
        if (hasLogExtension)
          fname.AppendLiteral(".log");
      }
      stream = ::fopen(fname.get(), "w" FOPEN_NO_INHERIT);
      if (stream != NULL) {
        *result = stream;
        fprintf(stdout, "### %s defined -- logging %s to %s\n",
                envVar, msg, fname.get());
      }
      else {
        fprintf(stdout, "### %s defined -- unable to log %s to %s\n",
                envVar, msg, fname.get());
      }
      return stream != NULL;
    }
  }
  return PR_FALSE;
}


static PLHashNumber HashNumber(const void* aKey)
{
  return PLHashNumber(NS_PTR_TO_INT32(aKey));
}

static void InitTraceLog(void)
{
  if (gInitialized) return;
  gInitialized = PR_TRUE;

  bool defined;
  defined = InitLog("XPCOM_MEM_BLOAT_LOG", "bloat/leaks", &gBloatLog);
  if (!defined)
    gLogLeaksOnly = InitLog("XPCOM_MEM_LEAK_LOG", "leaks", &gBloatLog);
  if (defined || gLogLeaksOnly) {
    RecreateBloatView();
    if (!gBloatView) {
      NS_WARNING("out of memory");
      gBloatLog = nsnull;
      gLogLeaksOnly = PR_FALSE;
    }
  }

  (void)InitLog("XPCOM_MEM_REFCNT_LOG", "refcounts", &gRefcntsLog);

  (void)InitLog("XPCOM_MEM_ALLOC_LOG", "new/delete", &gAllocLog);

  defined = InitLog("XPCOM_MEM_LEAKY_LOG", "for leaky", &gLeakyLog);
  if (defined) {
    gLogToLeaky = PR_TRUE;
    PRFuncPtr p = nsnull, q = nsnull;
#ifdef HAVE_DLOPEN
    {
      PRLibrary *lib = nsnull;
      p = PR_FindFunctionSymbolAndLibrary("__log_addref", &lib);
      if (lib) {
        PR_UnloadLibrary(lib);
        lib = nsnull;
      }
      q = PR_FindFunctionSymbolAndLibrary("__log_release", &lib);
      if (lib) {
        PR_UnloadLibrary(lib);
      }
    }
#endif
    if (p && q) {
      leakyLogAddRef = (void (*)(void*,int,int)) p;
      leakyLogRelease = (void (*)(void*,int,int)) q;
    }
    else {
      gLogToLeaky = PR_FALSE;
      fprintf(stdout, "### ERROR: XPCOM_MEM_LEAKY_LOG defined, but can't locate __log_addref and __log_release symbols\n");
      fflush(stdout);
    }
  }

  const char* classes = getenv("XPCOM_MEM_LOG_CLASSES");

#ifdef HAVE_CPP_DYNAMIC_CAST_TO_VOID_PTR
  if (classes) {
    (void)InitLog("XPCOM_MEM_COMPTR_LOG", "nsCOMPtr", &gCOMPtrLog);
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
                                  &typesToLogHashAllocOps, NULL);
    if (!gTypesToLog) {
      NS_WARNING("out of memory");
      fprintf(stdout, "### XPCOM_MEM_LOG_CLASSES defined -- unable to log specific classes\n");
    }
    else {
      fprintf(stdout, "### XPCOM_MEM_LOG_CLASSES defined -- only logging these classes: ");
      const char* cp = classes;
      for (;;) {
        char* cm = (char*) strchr(cp, ',');
        if (cm) {
          *cm = '\0';
        }
        PL_HashTableAdd(gTypesToLog, nsCRT::strdup(cp), (void*)1);
        fprintf(stdout, "%s ", cp);
        if (!cm) break;
        *cm = ',';
        cp = cm + 1;
      }
      fprintf(stdout, "\n");
    }

    gSerialNumbers = PL_NewHashTable(256,
                                     HashNumber,
                                     PL_CompareValues,
                                     PL_CompareValues,
                                     &serialNumberHashAllocOps, NULL);


  }

  const char* objects = getenv("XPCOM_MEM_LOG_OBJECTS");
  if (objects) {
    gObjectsToLog = PL_NewHashTable(256,
                                    HashNumber,
                                    PL_CompareValues,
                                    PL_CompareValues,
                                    NULL, NULL);

    if (!gObjectsToLog) {
      NS_WARNING("out of memory");
      fprintf(stdout, "### XPCOM_MEM_LOG_OBJECTS defined -- unable to log specific objects\n");
    }
    else if (! (gRefcntsLog || gAllocLog || gCOMPtrLog)) {
      fprintf(stdout, "### XPCOM_MEM_LOG_OBJECTS defined -- but none of XPCOM_MEM_(REFCNT|ALLOC|COMPTR)_LOG is defined\n");
    }
    else {
      fprintf(stdout, "### XPCOM_MEM_LOG_OBJECTS defined -- only logging these objects: ");
      const char* cp = objects;
      for (;;) {
        char* cm = (char*) strchr(cp, ',');
        if (cm) {
          *cm = '\0';
        }
        PRInt32 top = 0;
        PRInt32 bottom = 0;
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
        for(PRInt32 serialno = bottom; serialno <= top; serialno++) {
          PL_HashTableAdd(gObjectsToLog, (const void*)serialno, (void*)1);
          fprintf(stdout, "%d ", serialno);
        }
        if (!cm) break;
        *cm = ',';
        cp = cm + 1;
      }
      fprintf(stdout, "\n");
    }
  }


  if (gBloatLog || gRefcntsLog || gAllocLog || gLeakyLog || gCOMPtrLog) {
    gLogging = PR_TRUE;
  }

  gTraceLock = PR_NewLock();
}

#endif

extern "C" {

static void PrintStackFrame(void *aPC, void *aClosure)
{
  FILE *stream = (FILE*)aClosure;
  nsCodeAddressDetails details;
  char buf[1024];

  NS_DescribeCodeAddress(aPC, &details);
  NS_FormatCodeAddressDetails(aPC, &details, buf, sizeof(buf));
  fputs(buf, stream);
}

}

void
nsTraceRefcntImpl::WalkTheStack(FILE* aStream)
{
  NS_StackWalk(PrintStackFrame, 2, aStream);
}

//----------------------------------------------------------------------

// This thing is exported by libstdc++
// Yes, this is a gcc only hack
#if defined(MOZ_DEMANGLE_SYMBOLS)
#include <cxxabi.h>
#include <stdlib.h> // for free()
#endif // MOZ_DEMANGLE_SYMBOLS

void
nsTraceRefcntImpl::DemangleSymbol(const char * aSymbol,
                              char * aBuffer,
                              int aBufLen)
{
  NS_ASSERTION(nsnull != aSymbol,"null symbol");
  NS_ASSERTION(nsnull != aBuffer,"null buffer");
  NS_ASSERTION(aBufLen >= 32 ,"pulled 32 out of you know where");

  aBuffer[0] = '\0';

#if defined(MOZ_DEMANGLE_SYMBOLS)
 /* See demangle.h in the gcc source for the voodoo */
  char * demangled = abi::__cxa_demangle(aSymbol,0,0,0);

  if (demangled)
  {
    strncpy(aBuffer,demangled,aBufLen);
    free(demangled);
  }
#endif // MOZ_DEMANGLE_SYMBOLS
}


//----------------------------------------------------------------------

EXPORT_XPCOM_API(void)
NS_LogInit()
{
#ifdef NS_IMPL_REFCNT_LOGGING
  if (++gInitCount)
    nsTraceRefcntImpl::SetActivityIsLegal(PR_TRUE);
#endif

#ifdef NS_TRACE_MALLOC
  // XXX we don't have to worry about shutting down trace-malloc; it
  // handles this itself, through an atexit() callback.
  if (!NS_TraceMallocHasStarted())
    NS_TraceMallocStartup(-1);  // -1 == no logging
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
      nsTraceRefcntImpl::DumpStatistics();
      nsTraceRefcntImpl::ResetStatistics();
    }
    nsTraceRefcntImpl::Shutdown();
#ifdef NS_IMPL_REFCNT_LOGGING
    nsTraceRefcntImpl::SetActivityIsLegal(PR_FALSE);
    gActivityTLS = BAD_TLS_INDEX;
#endif
  }
}

} // namespace mozilla

EXPORT_XPCOM_API(void)
NS_LogAddRef(void* aPtr, nsrefcnt aRefcnt,
             const char* aClazz, PRUint32 classSize)
{
#ifdef NS_IMPL_REFCNT_LOGGING
  ASSERT_ACTIVITY_IS_LEGAL;
  if (!gInitialized)
    InitTraceLog();
  if (gLogging) {
    LOCK_TRACELOG();

    if (gBloatLog) {
      BloatEntry* entry = GetBloatEntry(aClazz, classSize);
      if (entry) {
        entry->AddRef(aRefcnt);
      }
    }

    // Here's the case where MOZ_COUNT_CTOR was not used,
    // yet we still want to see creation information:

    bool loggingThisType = (!gTypesToLog || LogThisType(aClazz));
    PRInt32 serialno = 0;
    if (gSerialNumbers && loggingThisType) {
      serialno = GetSerialNumber(aPtr, aRefcnt == 1);
      NS_ASSERTION(serialno != 0,
                   "Serial number requested for unrecognized pointer!  "
                   "Are you memmoving a refcounted object?");
      PRInt32* count = GetRefCount(aPtr);
      if(count)
        (*count)++;

    }

    bool loggingThisObject = (!gObjectsToLog || LogThisObj(serialno));
    if (aRefcnt == 1 && gAllocLog && loggingThisType && loggingThisObject) {
      fprintf(gAllocLog, "\n<%s> 0x%08X %d Create\n",
              aClazz, NS_PTR_TO_INT32(aPtr), serialno);
      nsTraceRefcntImpl::WalkTheStack(gAllocLog);
    }

    if (gRefcntsLog && loggingThisType && loggingThisObject) {
      if (gLogToLeaky) {
        (*leakyLogAddRef)(aPtr, aRefcnt - 1, aRefcnt);
      }
      else {
          // Can't use PR_LOG(), b/c it truncates the line
          fprintf(gRefcntsLog,
                  "\n<%s> 0x%08X %d AddRef %d\n", aClazz, NS_PTR_TO_INT32(aPtr), serialno, aRefcnt);
          nsTraceRefcntImpl::WalkTheStack(gRefcntsLog);
          fflush(gRefcntsLog);
      }
    }
    UNLOCK_TRACELOG();
  }
#endif
}

EXPORT_XPCOM_API(void)
NS_LogRelease(void* aPtr, nsrefcnt aRefcnt, const char* aClazz)
{
#ifdef NS_IMPL_REFCNT_LOGGING
  ASSERT_ACTIVITY_IS_LEGAL;
  if (!gInitialized)
    InitTraceLog();
  if (gLogging) {
    LOCK_TRACELOG();

    if (gBloatLog) {
      BloatEntry* entry = GetBloatEntry(aClazz, 0);
      if (entry) {
        entry->Release(aRefcnt);
      }
    }

    bool loggingThisType = (!gTypesToLog || LogThisType(aClazz));
    PRInt32 serialno = 0;
    if (gSerialNumbers && loggingThisType) {
      serialno = GetSerialNumber(aPtr, PR_FALSE);
      NS_ASSERTION(serialno != 0,
                   "Serial number requested for unrecognized pointer!  "
                   "Are you memmoving a refcounted object?");
      PRInt32* count = GetRefCount(aPtr);
      if(count)
        (*count)--;

    }

    bool loggingThisObject = (!gObjectsToLog || LogThisObj(serialno));
    if (gRefcntsLog && loggingThisType && loggingThisObject) {
      if (gLogToLeaky) {
        (*leakyLogRelease)(aPtr, aRefcnt + 1, aRefcnt);
      }
      else {
          // Can't use PR_LOG(), b/c it truncates the line
          fprintf(gRefcntsLog,
                  "\n<%s> 0x%08X %d Release %d\n", aClazz, NS_PTR_TO_INT32(aPtr), serialno, aRefcnt);
          nsTraceRefcntImpl::WalkTheStack(gRefcntsLog);
          fflush(gRefcntsLog);
      }
    }

    // Here's the case where MOZ_COUNT_DTOR was not used,
    // yet we still want to see deletion information:

    if (aRefcnt == 0 && gAllocLog && loggingThisType && loggingThisObject) {
      fprintf(gAllocLog,
              "\n<%s> 0x%08X %d Destroy\n",
              aClazz, NS_PTR_TO_INT32(aPtr), serialno);
      nsTraceRefcntImpl::WalkTheStack(gAllocLog);
    }

    if (aRefcnt == 0 && gSerialNumbers && loggingThisType) {
      RecycleSerialNumberPtr(aPtr);
    }

    UNLOCK_TRACELOG();
  }
#endif
}

EXPORT_XPCOM_API(void)
NS_LogCtor(void* aPtr, const char* aType, PRUint32 aInstanceSize)
{
#ifdef NS_IMPL_REFCNT_LOGGING
  ASSERT_ACTIVITY_IS_LEGAL;
  if (!gInitialized)
    InitTraceLog();

  if (gLogging) {
    LOCK_TRACELOG();

    if (gBloatLog) {
      BloatEntry* entry = GetBloatEntry(aType, aInstanceSize);
      if (entry) {
        entry->Ctor();
      }
    }

    bool loggingThisType = (!gTypesToLog || LogThisType(aType));
    PRInt32 serialno = 0;
    if (gSerialNumbers && loggingThisType) {
      serialno = GetSerialNumber(aPtr, PR_TRUE);
    }

    bool loggingThisObject = (!gObjectsToLog || LogThisObj(serialno));
    if (gAllocLog && loggingThisType && loggingThisObject) {
      fprintf(gAllocLog, "\n<%s> 0x%08X %d Ctor (%d)\n",
             aType, NS_PTR_TO_INT32(aPtr), serialno, aInstanceSize);
      nsTraceRefcntImpl::WalkTheStack(gAllocLog);
    }

    UNLOCK_TRACELOG();
  }
#endif
}


EXPORT_XPCOM_API(void)
NS_LogDtor(void* aPtr, const char* aType, PRUint32 aInstanceSize)
{
#ifdef NS_IMPL_REFCNT_LOGGING
  ASSERT_ACTIVITY_IS_LEGAL;
  if (!gInitialized)
    InitTraceLog();

  if (gLogging) {
    LOCK_TRACELOG();

    if (gBloatLog) {
      BloatEntry* entry = GetBloatEntry(aType, aInstanceSize);
      if (entry) {
        entry->Dtor();
      }
    }

    bool loggingThisType = (!gTypesToLog || LogThisType(aType));
    PRInt32 serialno = 0;
    if (gSerialNumbers && loggingThisType) {
      serialno = GetSerialNumber(aPtr, PR_FALSE);
      RecycleSerialNumberPtr(aPtr);
    }

    bool loggingThisObject = (!gObjectsToLog || LogThisObj(serialno));

    // (If we're on a losing architecture, don't do this because we'll be
    // using LogDeleteXPCOM instead to get file and line numbers.)
    if (gAllocLog && loggingThisType && loggingThisObject) {
      fprintf(gAllocLog, "\n<%s> 0x%08X %d Dtor (%d)\n",
             aType, NS_PTR_TO_INT32(aPtr), serialno, aInstanceSize);
      nsTraceRefcntImpl::WalkTheStack(gAllocLog);
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
  void *object = dynamic_cast<void *>(aObject);

  // This is a very indirect way of finding out what the class is
  // of the object being logged.  If we're logging a specific type,
  // then
  if (!gTypesToLog || !gSerialNumbers) {
    return;
  }
  PRInt32 serialno = GetSerialNumber(object, PR_FALSE);
  if (serialno == 0) {
    return;
  }

  if (!gInitialized)
    InitTraceLog();
  if (gLogging) {
    LOCK_TRACELOG();

    PRInt32* count = GetCOMPtrCount(object);
    if(count)
      (*count)++;

    bool loggingThisObject = (!gObjectsToLog || LogThisObj(serialno));

    if (gCOMPtrLog && loggingThisObject) {
      fprintf(gCOMPtrLog, "\n<?> 0x%08X %d nsCOMPtrAddRef %d 0x%08X\n",
              NS_PTR_TO_INT32(object), serialno, count?(*count):-1, NS_PTR_TO_INT32(aCOMPtr));
      nsTraceRefcntImpl::WalkTheStack(gCOMPtrLog);
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
  void *object = dynamic_cast<void *>(aObject);

  // This is a very indirect way of finding out what the class is
  // of the object being logged.  If we're logging a specific type,
  // then
  if (!gTypesToLog || !gSerialNumbers) {
    return;
  }
  PRInt32 serialno = GetSerialNumber(object, PR_FALSE);
  if (serialno == 0) {
    return;
  }

  if (!gInitialized)
    InitTraceLog();
  if (gLogging) {
    LOCK_TRACELOG();

    PRInt32* count = GetCOMPtrCount(object);
    if(count)
      (*count)--;

    bool loggingThisObject = (!gObjectsToLog || LogThisObj(serialno));

    if (gCOMPtrLog && loggingThisObject) {
      fprintf(gCOMPtrLog, "\n<?> 0x%08X %d nsCOMPtrRelease %d 0x%08X\n",
              NS_PTR_TO_INT32(object), serialno, count?(*count):-1, NS_PTR_TO_INT32(aCOMPtr));
      nsTraceRefcntImpl::WalkTheStack(gCOMPtrLog);
    }

    UNLOCK_TRACELOG();
  }
#endif
}

void
nsTraceRefcntImpl::Startup()
{
}

void
nsTraceRefcntImpl::Shutdown()
{
#ifdef NS_IMPL_REFCNT_LOGGING

  if (gBloatView) {
    PL_HashTableDestroy(gBloatView);
    gBloatView = nsnull;
  }
  if (gTypesToLog) {
    PL_HashTableDestroy(gTypesToLog);
    gTypesToLog = nsnull;
  }
  if (gObjectsToLog) {
    PL_HashTableDestroy(gObjectsToLog);
    gObjectsToLog = nsnull;
  }
  if (gSerialNumbers) {
    PL_HashTableDestroy(gSerialNumbers);
    gSerialNumbers = nsnull;
  }
  if (gBloatLog) {
    fclose(gBloatLog);
    gBloatLog = nsnull;
  }
  if (gRefcntsLog) {
    fclose(gRefcntsLog);
    gRefcntsLog = nsnull;
  }
  if (gAllocLog) {
    fclose(gAllocLog);
    gAllocLog = nsnull;
  }
  if (gLeakyLog) {
    fclose(gLeakyLog);
    gLeakyLog = nsnull;
  }
  if (gCOMPtrLog) {
    fclose(gCOMPtrLog);
    gCOMPtrLog = nsnull;
  }
#endif
}

void
nsTraceRefcntImpl::SetActivityIsLegal(bool aLegal)
{
#ifdef NS_IMPL_REFCNT_LOGGING
  if (gActivityTLS == BAD_TLS_INDEX)
    PR_NewThreadPrivateIndex(&gActivityTLS, nsnull);

  PR_SetThreadPrivate(gActivityTLS, NS_INT32_TO_PTR(!aLegal));
#endif
}

NS_IMPL_QUERY_INTERFACE1(nsTraceRefcntImpl, nsITraceRefcnt)

NS_IMETHODIMP_(nsrefcnt) nsTraceRefcntImpl::AddRef(void)
{
  return 2;
}

NS_IMETHODIMP_(nsrefcnt) nsTraceRefcntImpl::Release(void)
{
  return 1;
}

NS_IMETHODIMP
nsTraceRefcntImpl::LogAddRef(void *aPtr, nsrefcnt aNewRefcnt,
                             const char *aTypeName, PRUint32 aSize)
{
  NS_LogAddRef(aPtr, aNewRefcnt, aTypeName, aSize);
  return NS_OK;
}

NS_IMETHODIMP
nsTraceRefcntImpl::LogRelease(void *aPtr, nsrefcnt aNewRefcnt,
                              const char *aTypeName)
{
  NS_LogRelease(aPtr, aNewRefcnt, aTypeName);
  return NS_OK;
}

NS_IMETHODIMP
nsTraceRefcntImpl::LogCtor(void *aPtr, const char *aTypeName, PRUint32 aSize)
{
  NS_LogCtor(aPtr, aTypeName, aSize);
  return NS_OK;
}

NS_IMETHODIMP
nsTraceRefcntImpl::LogDtor(void *aPtr, const char *aTypeName, PRUint32 aSize)
{
  NS_LogDtor(aPtr, aTypeName, aSize);
  return NS_OK;
}

NS_IMETHODIMP
nsTraceRefcntImpl::LogAddCOMPtr(void *aCOMPtr, nsISupports* aObject)
{
  NS_LogCOMPtrAddRef(aCOMPtr, aObject);
  return NS_OK;
}

NS_IMETHODIMP
nsTraceRefcntImpl::LogReleaseCOMPtr(void *aCOMPtr, nsISupports* aObject)
{
  NS_LogCOMPtrRelease(aCOMPtr, aObject);
  return NS_OK;
}

static const nsTraceRefcntImpl kTraceRefcntImpl;

NS_METHOD
nsTraceRefcntImpl::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
  return const_cast<nsTraceRefcntImpl*>(&kTraceRefcntImpl)->
    QueryInterface(aIID, aInstancePtr);
}
