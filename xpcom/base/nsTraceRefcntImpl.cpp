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
#include "nscore.h"
#include "nsISupports.h"
#include "nsVoidArray.h"
#include "prprf.h"
#include "prlog.h"
#include "plstr.h"
#include <stdlib.h>
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include <math.h>

#if defined(_WIN32)
#include <windows.h>
#elif defined(linux) && defined(__GLIBC__) && (defined(__i386) || defined(PPC))
#include <setjmp.h>

//
// On glibc 2.1, the Dl_info api defined in <dlfcn.h> is only exposed
// if __USE_GNU is defined.  I suppose its some kind of standards
// adherence thing.
//
#if (__GLIBC_MINOR__ >= 1) && !defined(__USE_GNU)
#define __USE_GNU
#endif

#include <dlfcn.h>
#endif

#ifdef HAVE_LIBDL
#include <dlfcn.h>
#endif

#if defined(XP_MAC) && !TARGET_CARBON
#include "macstdlibextras.h"
#endif

////////////////////////////////////////////////////////////////////////////////

NS_COM void 
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

#ifdef NS_BUILD_REFCNT_LOGGING
#include "plhash.h"
#include "prmem.h"

#include "prlock.h"

static PRLock* gTraceLock;

#define LOCK_TRACELOG()   PR_Lock(gTraceLock)
#define UNLOCK_TRACELOG() PR_Unlock(gTraceLock)

static PLHashTable* gBloatView;
static PLHashTable* gTypesToLog;
static PLHashTable* gObjectsToLog;
static PLHashTable* gSerialNumbers;
static PRInt32 gNextSerialNumber;

static PRBool gLogging;
static PRBool gLogToLeaky;
static PRBool gLogLeaksOnly;

static void (*leakyLogAddRef)(void* p, int oldrc, int newrc);
static void (*leakyLogRelease)(void* p, int oldrc, int newrc);

static PRBool gInitialized = PR_FALSE;
static FILE *gBloatLog = nsnull;
static FILE *gRefcntsLog = nsnull;
static FILE *gAllocLog = nsnull;
static FILE *gLeakyLog = nsnull;
static FILE *gCOMPtrLog = nsnull;
static PRBool gActivityIsLegal = PR_FALSE;

struct serialNumberRecord {
  PRInt32 serialNumber;
  PRInt32 refCount;
  PRInt32 COMPtrCount;
};

struct nsTraceRefcntStats {
  nsrefcnt mAddRefs;
  nsrefcnt mReleases;
  nsrefcnt mCreates;
  nsrefcnt mDestroys;
  double mRefsOutstandingTotal;
  double mRefsOutstandingSquared;
  double mObjsOutstandingTotal;
  double mObjsOutstandingSquared;
};

#ifdef DEBUG_dbaron_off
  // I hope to turn this on for everybody once we hit it a little less.
#define ASSERT_ACTIVITY_IS_LEGAL                                                \
  NS_WARN_IF_FALSE(gActivityIsLegal,                                         \
                   "XPCOM objects created/destroyed from static ctor/dtor")
#else
#define ASSERT_ACTIVITY_IS_LEGAL
#endif


// These functions are copied from nsprpub/lib/ds/plhash.c, with changes
// to the functions not called Default* to free the serialNumberRecord or
// the BloatEntry.

static void * PR_CALLBACK
DefaultAllocTable(void *pool, PRSize size)
{
#if defined(XP_MAC)
#pragma unused (pool)
#endif

    return PR_MALLOC(size);
}

static void PR_CALLBACK
DefaultFreeTable(void *pool, void *item)
{
#if defined(XP_MAC)
#pragma unused (pool)
#endif

    PR_Free(item);
}

static PLHashEntry * PR_CALLBACK
DefaultAllocEntry(void *pool, const void *key)
{
#if defined(XP_MAC)
#pragma unused (pool,key)
#endif

    return PR_NEW(PLHashEntry);
}

static void PR_CALLBACK
SerialNumberFreeEntry(void *pool, PLHashEntry *he, PRUintn flag)
{
#if defined(XP_MAC)
#pragma unused (pool)
#endif

    if (flag == HT_FREE_ENTRY) {
        PR_Free(NS_REINTERPRET_CAST(serialNumberRecord*,he->value));
        PR_Free(he);
    }
}

static void PR_CALLBACK
TypesToLogFreeEntry(void *pool, PLHashEntry *he, PRUintn flag)
{
#if defined(XP_MAC)
#pragma unused (pool)
#endif

    if (flag == HT_FREE_ENTRY) {
        nsCRT::free(NS_CONST_CAST(char*,
                     NS_REINTERPRET_CAST(const char*, he->key)));
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
    PRInt32 cnt = (mNewStats.mAddRefs - mNewStats.mReleases);
    mNewStats.mRefsOutstandingTotal += cnt;
    mNewStats.mRefsOutstandingSquared += cnt * cnt;
  }

  void AccountObjs() {
    PRInt32 cnt = (mNewStats.mCreates - mNewStats.mDestroys);
    mNewStats.mObjsOutstandingTotal += cnt;
    mNewStats.mObjsOutstandingSquared += cnt * cnt;
  }

  static PRIntn PR_CALLBACK DumpEntry(PLHashEntry *he, PRIntn i, void *arg) {
    BloatEntry* entry = (BloatEntry*)he->value;
    if (entry) {
      entry->Accumulate();
      NS_STATIC_CAST(nsVoidArray*, arg)->AppendElement(entry);
    }
    return HT_ENUMERATE_NEXT;
  }

  static PRIntn PR_CALLBACK TotalEntries(PLHashEntry *he, PRIntn i, void *arg) {
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
    PRInt32 count = (mNewStats.mCreates + mAllStats.mCreates);
    total->mClassSize += mClassSize * count;    // adjust for average in DumpTotal
    total->mTotalLeaked += (PRInt32)(mClassSize *
                                     ((mNewStats.mCreates + mAllStats.mCreates)
                                      -(mNewStats.mDestroys + mAllStats.mDestroys)));
  }

  nsresult DumpTotal(PRUint32 nClasses, FILE* out) {
    mClassSize /= mAllStats.mCreates;
    return Dump(-1, out, nsTraceRefcntImpl::ALL_STATS);
  }

  static PRBool HaveLeaks(nsTraceRefcntStats* stats) {
    return ((stats->mAddRefs != stats->mReleases) ||
            (stats->mCreates != stats->mDestroys));
  }

  static nsresult PrintDumpHeader(FILE* out, const char* msg) {
    fprintf(out, "\n== BloatView: %s\n\n", msg);
    fprintf(out, 
        "     |<----------------Class--------------->|<-----Bytes------>|<----------------Objects---------------->|<--------------References-------------->|\n");
    fprintf(out,
        "                                              Per-Inst   Leaked    Total      Rem      Mean       StdDev     Total      Rem      Mean       StdDev\n");
    return NS_OK;
  }

  nsresult Dump(PRIntn i, FILE* out, nsTraceRefcntImpl::StatisticsType type) {
    nsTraceRefcntStats* stats = (type == nsTraceRefcntImpl::NEW_STATS) ? &mNewStats : &mAllStats;
    if (gLogLeaksOnly && !HaveLeaks(stats)) {
      return NS_OK;
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
      fprintf(out, "%4d %-40.40s %8d %8d %8d %8d (%8.2f +/- %8.2f) %8d %8d (%8.2f +/- %8.2f)\n",
              i+1, mClassName,
              (PRInt32)mClassSize,
              (nsCRT::strcmp(mClassName, "TOTAL"))
                  ?(PRInt32)((stats->mCreates - stats->mDestroys) * mClassSize)
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
    return NS_OK;
  }

protected:
  char*         mClassName;
  double        mClassSize;     // this is stored as a double because of the way we compute the avg class size for total bloat
  PRInt32       mTotalLeaked; // used only for TOTAL entry
  nsTraceRefcntStats mNewStats;
  nsTraceRefcntStats mAllStats;
};

static void PR_CALLBACK
BloatViewFreeEntry(void *pool, PLHashEntry *he, PRUintn flag)
{
#if defined(XP_MAC)
#pragma unused (pool)
#endif

    if (flag == HT_FREE_ENTRY) {
        BloatEntry* entry = NS_REINTERPRET_CAST(BloatEntry*,he->value);
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

static PRIntn PR_CALLBACK DumpSerialNumbers(PLHashEntry* aHashEntry, PRIntn aIndex, void* aClosure)
{
  serialNumberRecord* record = NS_REINTERPRET_CAST(serialNumberRecord *,aHashEntry->value);
#ifdef HAVE_CPP_DYNAMIC_CAST_TO_VOID_PTR
  fprintf((FILE*) aClosure, "%d (%d references; %d from COMPtrs)\n",
                            record->serialNumber,
                            record->refCount,
                            record->COMPtrCount);
#else
  fprintf((FILE*) aClosure, "%d (%d references)\n",
                            record->serialNumber,
                            record->refCount);
#endif
  return HT_ENUMERATE_NEXT;
}


#endif /* NS_BUILD_REFCNT_LOGGING */

nsresult
nsTraceRefcntImpl::DumpStatistics(StatisticsType type, FILE* out)
{
  nsresult rv = NS_OK;
#ifdef NS_BUILD_REFCNT_LOGGING
  if (gBloatLog == nsnull || gBloatView == nsnull) {
    return NS_ERROR_FAILURE;
  }
  if (out == nsnull) {
    out = gBloatLog;
  }

  LOCK_TRACELOG();

  PRBool wasLogging = gLogging;
  gLogging = PR_FALSE;  // turn off logging for this method
  
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
  rv = BloatEntry::PrintDumpHeader(out, msg);
  if (NS_FAILED(rv)) goto done;

  {
    BloatEntry total("TOTAL", 0);
    PL_HashTableEnumerateEntries(gBloatView, BloatEntry::TotalEntries, &total);
    total.DumpTotal(gBloatView->nentries, out);

    nsVoidArray entries;
    PL_HashTableEnumerateEntries(gBloatView, BloatEntry::DumpEntry, &entries);

    fprintf(stdout, "nsTraceRefcntImpl::DumpStatistics: %d entries\n",
           entries.Count());

    // Sort the entries alphabetically by classname.
    PRInt32 i, j;
    for (i = entries.Count() - 1; i >= 1; --i) {
      for (j = i - 1; j >= 0; --j) {
        BloatEntry* left  = NS_STATIC_CAST(BloatEntry*, entries[i]);
        BloatEntry* right = NS_STATIC_CAST(BloatEntry*, entries[j]);

        if (PL_strcmp(left->GetClassName(), right->GetClassName()) < 0) {
          entries.ReplaceElementAt(right, i);
          entries.ReplaceElementAt(left, j);
        }
      }
    }

    // Enumerate from back-to-front, so things come out in alpha order
    for (i = 0; i < entries.Count(); ++i) {
      BloatEntry* entry = NS_STATIC_CAST(BloatEntry*, entries[i]);
      entry->Dump(i, out, type);
    }
  }

  if (gSerialNumbers) {
    fprintf(out, "\n\nSerial Numbers of Leaked Objects:\n");
    PL_HashTableEnumerateEntries(gSerialNumbers, DumpSerialNumbers, out);
  }

done:
  gLogging = wasLogging;
  UNLOCK_TRACELOG();
#endif
  return rv;
}

void
nsTraceRefcntImpl::ResetStatistics()
{
#ifdef NS_BUILD_REFCNT_LOGGING
  LOCK_TRACELOG();
  if (gBloatView) {
    PL_HashTableDestroy(gBloatView);
    gBloatView = nsnull;
  }
  UNLOCK_TRACELOG();
#endif
}

#ifdef NS_BUILD_REFCNT_LOGGING
static PRBool LogThisType(const char* aTypeName)
{
  void* he = PL_HashTableLookup(gTypesToLog, aTypeName);
  return nsnull != he;
}

static PRInt32 GetSerialNumber(void* aPtr, PRBool aCreate)
{
#ifdef GC_LEAK_DETECTOR
  // need to disguise this pointer, so the table won't keep the object alive.
  aPtr = (void*) ~PLHashNumber(aPtr);
#endif
  PLHashEntry** hep = PL_HashTableRawLookup(gSerialNumbers, PLHashNumber(NS_PTR_TO_INT32(aPtr)), aPtr);
  if (hep && *hep) {
    return PRInt32((NS_REINTERPRET_CAST(serialNumberRecord*,(*hep)->value))->serialNumber);
  }
  else if (aCreate) {
    serialNumberRecord *record = PR_NEW(serialNumberRecord);
    record->serialNumber = ++gNextSerialNumber;
    record->refCount = 0;
    record->COMPtrCount = 0;
    PL_HashTableRawAdd(gSerialNumbers, hep, PLHashNumber(NS_PTR_TO_INT32(aPtr)), aPtr, NS_REINTERPRET_CAST(void*,record));
    return gNextSerialNumber;
  }
  else {
    return 0;
  }
}

static PRInt32* GetRefCount(void* aPtr)
{
#ifdef GC_LEAK_DETECTOR
  // need to disguise this pointer, so the table won't keep the object alive.
  aPtr = (void*) ~PLHashNumber(aPtr);
#endif
  PLHashEntry** hep = PL_HashTableRawLookup(gSerialNumbers, PLHashNumber(NS_PTR_TO_INT32(aPtr)), aPtr);
  if (hep && *hep) {
    return &((NS_REINTERPRET_CAST(serialNumberRecord*,(*hep)->value))->refCount);
  } else {
    return nsnull;
  }
}

static PRInt32* GetCOMPtrCount(void* aPtr)
{
#ifdef GC_LEAK_DETECTOR
  // need to disguise this pointer, so the table won't keep the object alive.
  aPtr = (void*) ~PLHashNumber(aPtr);
#endif
  PLHashEntry** hep = PL_HashTableRawLookup(gSerialNumbers, PLHashNumber(NS_PTR_TO_INT32(aPtr)), aPtr);
  if (hep && *hep) {
    return &((NS_REINTERPRET_CAST(serialNumberRecord*,(*hep)->value))->COMPtrCount);
  } else {
    return nsnull;
  }
}

static void RecycleSerialNumberPtr(void* aPtr)
{
#ifdef GC_LEAK_DETECTOR
  // need to disguise this pointer, so the table won't keep the object alive.
  aPtr = (void*) ~PLHashNumber(aPtr);
#endif
  PL_HashTableRemove(gSerialNumbers, aPtr);
}

static PRBool LogThisObj(PRInt32 aSerialNumber)
{
  return nsnull != PL_HashTableLookup(gObjectsToLog, (const void*)(aSerialNumber));
}

static PRBool InitLog(const char* envVar, const char* msg, FILE* *result)
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
      FILE *stream = ::fopen(value, "w");
      if (stream != NULL) {
        *result = stream;
        fprintf(stdout, "### %s defined -- logging %s to %s\n", 
                envVar, msg, value);
        return PR_TRUE;
      }
      else {
        fprintf(stdout, "### %s defined -- unable to log %s to %s\n", 
                envVar, msg, value);
        return PR_FALSE;
      }
    }
  }
  return PR_FALSE;
}


static PLHashNumber PR_CALLBACK HashNumber(const void* aKey)
{
  return PLHashNumber(NS_PTR_TO_INT32(aKey));
}

static void InitTraceLog(void)
{
  if (gInitialized) return;
  gInitialized = PR_TRUE;

#if defined(XP_MAC) && !TARGET_CARBON
  // this can get called before Toolbox has been initialized.
  InitializeMacToolbox();
#endif

  PRBool defined;
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
    void* p = nsnull;
    void* q = nsnull;
#ifdef HAVE_LIBDL
    p = dlsym(0, "__log_addref");
    q = dlsym(0, "__log_release");
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

#if defined(_WIN32) && defined(_M_IX86) // WIN32 x86 stack walking code
#include "nsStackFrameWin.h"
void
nsTraceRefcntImpl::WalkTheStack(FILE* aStream)
{
  DumpStackToFile(aStream);
}

// WIN32 x86 stack walking code
// i386 or PPC Linux stackwalking code or Solaris
#elif (defined(linux) && defined(__GLIBC__) && (defined(__i386) || defined(PPC))) || (defined(__sun) && (defined(__sparc) || defined(sparc) || defined(__i386) || defined(i386)))
#include "nsStackFrameUnix.h"
void
nsTraceRefcntImpl::WalkTheStack(FILE* aStream)
{
  DumpStackToFile(aStream);
}

#elif defined(XP_MAC)

/**
 * Stack walking code for the Mac OS.
 */

#include "gc_fragments.h"

#include <typeinfo>

extern "C" {
void MWUnmangle(const char *mangled_name, char *unmangled_name, size_t buffersize);
}

struct traceback_table {
	long zero;
	long magic;
	long reserved;
	long codeSize;
	short nameLength;
	char name[2];
};

static char* pc2name(long* pc, char name[], long size)
{
	name[0] = '\0';
	
	// make sure pc is instruction aligned (at least).
	if (UInt32(pc) == (UInt32(pc) & 0xFFFFFFFC)) {
		long instructionsToLook = 4096;
		long* instruction = (long*)pc;
		
		// look for the traceback table.
		while (instructionsToLook--) {
			if (instruction[0] == 0x4E800020 && instruction[1] == 0x00000000) {
				traceback_table* tb = (traceback_table*)&instruction[1];
				memcpy(name, tb->name + 1, --nameLength);
				name[nameLength] = '\0';
				break;
			}
			++instruction;
		}
	}
	
	return name;
}

struct stack_frame {
	stack_frame*	next;				// savedSP
	void*			savedCR;
	void*			savedLR;
	void*			reserved0;
	void*			reserved1;
	void*			savedTOC;
};

static asm stack_frame* getStackFrame() 
{
	mr		r3, sp
	blr
}

NS_COM void
nsTraceRefcntImpl::WalkTheStack(FILE* aStream)
{
  stack_frame* currentFrame = getStackFrame();    // WalkTheStack's frame.
	currentFrame = currentFrame->next;	            // WalkTheStack's caller's frame.
	currentFrame = currentFrame->next;              // WalkTheStack's caller's caller's frame.
    
	while (true) {
		// LR saved at 8(SP) in each frame. subtract 4 to get address of calling instruction.
		void* pc = currentFrame->savedLR;

		// convert PC to name, unmangle it, and generate source location, if possible.
		static char symbol_name[1024], unmangled_name[1024], file_name[256]; UInt32 file_offset;
		
     	if (GC_address_to_source((char*)pc, symbol_name, file_name, &file_offset)) {
			MWUnmangle(symbol_name, unmangled_name, sizeof(unmangled_name));
     		fprintf(aStream, "%s[%s,%ld]\n", unmangled_name, file_name, file_offset);
     	} else {
 		    pc2name((long*)pc, symbol_name, sizeof(symbol_name));
			MWUnmangle(symbol_name, unmangled_name, sizeof(unmangled_name));
    		fprintf(aStream, "%s(0x%08X)\n", unmangled_name, pc);
     	}

		currentFrame = currentFrame->next;
		// the bottom-most frame is marked as pointing to NULL, or is ODD if a 68K transition frame.
		if (currentFrame == NULL || UInt32(currentFrame) & 0x1)
			break;
	}
}

#else // unsupported platform.

void
nsTraceRefcntImpl::WalkTheStack(FILE* aStream)
{
	fprintf(aStream, "write me, dammit!\n");
}

#endif

//----------------------------------------------------------------------

// This thing is exported by libstdc++
// Yes, this is a gcc only hack
#if defined(MOZ_DEMANGLE_SYMBOLS)
#include <cxxabi.h>
#include <stdlib.h> // for free()
#endif // MOZ_DEMANGLE_SYMBOLS

NS_COM void 
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

NS_COM void
nsTraceRefcntImpl::LoadLibrarySymbols(const char* aLibraryName,
                                  void* aLibrayHandle)
{
#ifdef NS_BUILD_REFCNT_LOGGING
#if defined(_WIN32) && defined(_M_IX86) /* Win32 x86 only */
  if (!gInitialized)
    InitTraceLog();

  if (gAllocLog || gRefcntsLog) {
    fprintf(stdout, "### Loading symbols for %s\n", aLibraryName);
    fflush(stdout);

    HANDLE myProcess = ::GetCurrentProcess();    
    BOOL ok = EnsureSymInitialized();
    if (ok) {
      const char* baseName = aLibraryName;
      // just get the base name of the library if a full path was given:
      PRInt32 len = strlen(aLibraryName);
      for (PRInt32 i = len - 1; i >= 0; i--) {
        if (aLibraryName[i] == '\\') {
          baseName = &aLibraryName[i + 1];
          break;
        }
      }
      DWORD baseAddr = _SymLoadModule(myProcess,
                                     NULL,
                                     (char*)baseName,
                                     (char*)baseName,
                                     0,
                                     0);
      ok = (baseAddr != nsnull);
    }
    if (!ok) {
      LPVOID lpMsgBuf;
      FormatMessage( 
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM | 
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
        (LPTSTR) &lpMsgBuf,
        0,
        NULL 
        );
      fprintf(stdout, "### ERROR: LoadLibrarySymbols for %s: %s\n",
              aLibraryName, lpMsgBuf);
      fflush(stdout);
      LocalFree( lpMsgBuf );
    }
  }
#endif
#endif
}

//----------------------------------------------------------------------






// don't use the logging ones. :-)
NS_IMETHODIMP_(nsrefcnt) nsTraceRefcntImpl::AddRef(void)
{
  NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt");
  ++mRefCnt;
  return mRefCnt;
}

NS_IMETHODIMP_(nsrefcnt) nsTraceRefcntImpl::Release(void)                                
{                                                                             
  NS_PRECONDITION(0 != mRefCnt, "dup release");                               
  --mRefCnt;                                                                  
  if (mRefCnt == 0) {                                                         
    mRefCnt = 1; /* stabilize */                                              
    delete this;                                                              
    return 0;                                                                 
  }
  return mRefCnt;                                                             
}

NS_IMPL_QUERY_INTERFACE1(nsTraceRefcntImpl, nsITraceRefcnt)

nsTraceRefcntImpl::nsTraceRefcntImpl()
{
  /* member initializers and constructor code */
}

NS_IMETHODIMP 
nsTraceRefcntImpl::LogAddRef(void* aPtr,
                             nsrefcnt aRefcnt,
                             const char* aClazz,
                             PRUint32 classSize)
{
#ifdef NS_BUILD_REFCNT_LOGGING
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

    // Here's the case where neither NS_NEWXPCOM nor MOZ_COUNT_CTOR were used,
    // yet we still want to see creation information:

    PRBool loggingThisType = (!gTypesToLog || LogThisType(aClazz));
    PRInt32 serialno = 0;
    if (gSerialNumbers && loggingThisType) {
      serialno = GetSerialNumber(aPtr, aRefcnt == 1);
      PRInt32* count = GetRefCount(aPtr);
      if(count)
        (*count)++;

    }

    PRBool loggingThisObject = (!gObjectsToLog || LogThisObj(serialno));
    if (aRefcnt == 1 && gAllocLog && loggingThisType && loggingThisObject) {
      fprintf(gAllocLog, "\n<%s> 0x%08X %d Create\n",
              aClazz, NS_PTR_TO_INT32(aPtr), serialno);
      WalkTheStack(gAllocLog);
    }

    if (gRefcntsLog && loggingThisType && loggingThisObject) {
      if (gLogToLeaky) {
        (*leakyLogAddRef)(aPtr, aRefcnt - 1, aRefcnt);
      }
      else {        
          // Can't use PR_LOG(), b/c it truncates the line
          fprintf(gRefcntsLog,
                  "\n<%s> 0x%08X %d AddRef %d\n", aClazz, NS_PTR_TO_INT32(aPtr), serialno, aRefcnt);       
          WalkTheStack(gRefcntsLog);
          fflush(gRefcntsLog);
      }
    }
    UNLOCK_TRACELOG();
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP 
nsTraceRefcntImpl::LogRelease(void* aPtr,
                          nsrefcnt aRefcnt,
                          const char* aClazz)
{
#ifdef NS_BUILD_REFCNT_LOGGING
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

    PRBool loggingThisType = (!gTypesToLog || LogThisType(aClazz));
    PRInt32 serialno = 0;
    if (gSerialNumbers && loggingThisType) {
      serialno = GetSerialNumber(aPtr, PR_FALSE);
      PRInt32* count = GetRefCount(aPtr);
      if(count)
        (*count)--;

    }

    PRBool loggingThisObject = (!gObjectsToLog || LogThisObj(serialno));
    if (gRefcntsLog && loggingThisType && loggingThisObject) {
      if (gLogToLeaky) {
        (*leakyLogRelease)(aPtr, aRefcnt + 1, aRefcnt);
      }
      else {
          // Can't use PR_LOG(), b/c it truncates the line
          fprintf(gRefcntsLog,
                  "\n<%s> 0x%08X %d Release %d\n", aClazz, NS_PTR_TO_INT32(aPtr), serialno, aRefcnt);
          WalkTheStack(gRefcntsLog);
          fflush(gRefcntsLog);
      }
    }

    // Here's the case where neither NS_DELETEXPCOM nor MOZ_COUNT_DTOR were used,
    // yet we still want to see deletion information:

    if (aRefcnt == 0 && gAllocLog && loggingThisType && loggingThisObject) {
      fprintf(gAllocLog,
              "\n<%s> 0x%08X %d Destroy\n",
              aClazz, NS_PTR_TO_INT32(aPtr), serialno);
      WalkTheStack(gAllocLog);
    }

    if (aRefcnt == 0 && gSerialNumbers && loggingThisType) {
      RecycleSerialNumberPtr(aPtr);
    }

    UNLOCK_TRACELOG();
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP 
nsTraceRefcntImpl::LogCtor(void* aPtr,
                           const char* aType,
                           PRUint32 aInstanceSize)
{
#ifdef NS_BUILD_REFCNT_LOGGING
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

    PRBool loggingThisType = (!gTypesToLog || LogThisType(aType));
    PRInt32 serialno = 0;
    if (gSerialNumbers && loggingThisType) {
      serialno = GetSerialNumber(aPtr, PR_TRUE);
    }

    PRBool loggingThisObject = (!gObjectsToLog || LogThisObj(serialno));
    if (gAllocLog && loggingThisType && loggingThisObject) {
      fprintf(gAllocLog, "\n<%s> 0x%08X %d Ctor (%d)\n",
             aType, NS_PTR_TO_INT32(aPtr), serialno, aInstanceSize);
      WalkTheStack(gAllocLog);
    }

    UNLOCK_TRACELOG();
  }
#endif
  return NS_OK;
}


NS_IMETHODIMP 
nsTraceRefcntImpl::LogDtor(void* aPtr, 
                           const char* aType,
                           PRUint32 aInstanceSize)
{
#ifdef NS_BUILD_REFCNT_LOGGING
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

    PRBool loggingThisType = (!gTypesToLog || LogThisType(aType));
    PRInt32 serialno = 0;
    if (gSerialNumbers && loggingThisType) {
      serialno = GetSerialNumber(aPtr, PR_FALSE);
      RecycleSerialNumberPtr(aPtr);
    }

    PRBool loggingThisObject = (!gObjectsToLog || LogThisObj(serialno));

    // (If we're on a losing architecture, don't do this because we'll be
    // using LogDeleteXPCOM instead to get file and line numbers.)
    if (gAllocLog && loggingThisType && loggingThisObject) {
      fprintf(gAllocLog, "\n<%s> 0x%08X %d Dtor (%d)\n",
             aType, NS_PTR_TO_INT32(aPtr), serialno, aInstanceSize);
      WalkTheStack(gAllocLog);
    }

    UNLOCK_TRACELOG();
  }
#endif
  return NS_OK;
}


NS_IMETHODIMP 
nsTraceRefcntImpl::LogAddCOMPtr(void* aCOMPtr,
                            nsISupports* aObject)
{
#if defined(NS_BUILD_REFCNT_LOGGING) && defined(HAVE_CPP_DYNAMIC_CAST_TO_VOID_PTR)
  // Get the most-derived object.
  void *object = dynamic_cast<void *>(aObject);

  // This is a very indirect way of finding out what the class is
  // of the object being logged.  If we're logging a specific type,
  // then 
  if (!gTypesToLog || !gSerialNumbers) {
    return NS_OK;
  }
  PRInt32 serialno = GetSerialNumber(object, PR_FALSE);
  if (serialno == 0) {
    return NS_OK;
  }

  if (!gInitialized)
    InitTraceLog();
  if (gLogging) {
    LOCK_TRACELOG();

    PRInt32* count = GetCOMPtrCount(object);
    if(count)
      (*count)++;

    PRBool loggingThisObject = (!gObjectsToLog || LogThisObj(serialno));

    if (gCOMPtrLog && loggingThisObject) {
      fprintf(gCOMPtrLog, "\n<?> 0x%08X %d nsCOMPtrAddRef %d 0x%08X\n",
              NS_PTR_TO_INT32(object), serialno, count?(*count):-1, NS_PTR_TO_INT32(aCOMPtr));
      WalkTheStack(gCOMPtrLog);
    }

    UNLOCK_TRACELOG();
  }
#endif
  return NS_OK;
}


NS_IMETHODIMP 
nsTraceRefcntImpl::LogReleaseCOMPtr(void* aCOMPtr,
                                    nsISupports* aObject)
{
#if defined(NS_BUILD_REFCNT_LOGGING) && defined(HAVE_CPP_DYNAMIC_CAST_TO_VOID_PTR)
  // Get the most-derived object.
  void *object = dynamic_cast<void *>(aObject);

  // This is a very indirect way of finding out what the class is
  // of the object being logged.  If we're logging a specific type,
  // then 
  if (!gTypesToLog || !gSerialNumbers) {
    return NS_OK;
  }
  PRInt32 serialno = GetSerialNumber(object, PR_FALSE);
  if (serialno == 0) {
    return NS_OK;
  }

  if (!gInitialized)
    InitTraceLog();
  if (gLogging) {
    LOCK_TRACELOG();

    PRInt32* count = GetCOMPtrCount(object);
    if(count)
      (*count)--;

    PRBool loggingThisObject = (!gObjectsToLog || LogThisObj(serialno));

    if (gCOMPtrLog && loggingThisObject) {
      fprintf(gCOMPtrLog, "\n<?> 0x%08X %d nsCOMPtrRelease %d 0x%08X\n",
              NS_PTR_TO_INT32(object), serialno, count?(*count):-1, NS_PTR_TO_INT32(aCOMPtr));
      WalkTheStack(gCOMPtrLog);
    }

    UNLOCK_TRACELOG();
  }
#endif
  return NS_OK;
}

NS_COM void
nsTraceRefcntImpl::Startup()
{
#ifdef NS_BUILD_REFCNT_LOGGING
  SetActivityIsLegal(PR_TRUE);
#endif
}

NS_COM void
nsTraceRefcntImpl::Shutdown()
{
#ifdef NS_BUILD_REFCNT_LOGGING

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

  SetActivityIsLegal(PR_FALSE);

#endif
}

NS_COM void
nsTraceRefcntImpl::SetActivityIsLegal(PRBool aLegal)
{
#ifdef NS_BUILD_REFCNT_LOGGING
  gActivityIsLegal = aLegal;
#endif
}


NS_METHOD
nsTraceRefcntImpl::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
  *aInstancePtr = nsnull;
  nsITraceRefcnt* tracer = new nsTraceRefcntImpl();
  if (!tracer)
    return NS_ERROR_OUT_OF_MEMORY;
  
  nsresult rv = tracer->QueryInterface(aIID, aInstancePtr);
  if (NS_FAILED(rv)) {
    delete tracer;
  }
  
  return rv;
}
