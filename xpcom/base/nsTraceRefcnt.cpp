/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsISupports.h"
#include "nsVoidArray.h"
#include "prprf.h"
#include "prlog.h"
#include "plstr.h"
#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>
#elif defined(linux) && defined(__GLIBC__) && defined(__i386)
#include <setjmp.h>

//
// On glibc 2.1, the Dl_info api defined in <dlfcn.h> is only exposed
// if __USE_GNU is defined.  I suppose its some kind of standards
// adherence thing.
//
#if (__GLIBC_MINOR__ >= 1)
#define __USE_GNU
#endif

#include <dlfcn.h>

#endif

#ifdef HAVE_LIBDL
#include <dlfcn.h>
#endif

#ifdef NS_BUILD_REFCNT_LOGGING
#include "plhash.h"
#include <math.h>

#if defined(NS_MT_SUPPORTED)
#include "prlock.h"

static PRLock* gTraceLock;

#define LOCK_TRACELOG()   PR_Lock(gTraceLock)
#define UNLOCK_TRACELOG() PR_Unlock(gTraceLock)
#else /* ! NT_MT_SUPPORTED */
#define LOCK_TRACELOG()
#define UNLOCK_TRACELOG()
#endif /* ! NS_MT_SUPPORTED */

static PRLogModuleInfo* gTraceRefcntLog;

static PLHashTable* gBloatView;

static PLHashTable* gTypesToLog;

static PRBool gLogging;
static PRBool gLogAllRefcnts;
static PRBool gLogSomeRefcnts;
static PRBool gLogToLeaky;
static PRBool gTrackBloat;
static PRBool gLogCalls;
static PRBool gLogNewAndDelete;
static PRBool gDumpLeaksOnly;

static void (*leakyLogAddRef)(void* p, int oldrc, int newrc);
static void (*leakyLogRelease)(void* p, int oldrc, int newrc);

#define XPCOM_REFCNT_TRACK_BLOAT  0x1
#define XPCOM_REFCNT_LOG_ALL      0x2
#define XPCOM_REFCNT_LOG_SOME     0x4
#define XPCOM_REFCNT_LOG_TO_LEAKY 0x8

// Should only use this on NS_LOSING_ARCHITECTURE...
#define XPCOM_REFCNT_LOG_CALLS    0x10
#define XPCOM_REFCNT_LOG_NEW      0x20

struct GatherArgs {
  nsTraceRefcntStatFunc func;
  void* closure;
};

class BloatEntry {
public:
  BloatEntry(const char* className, PRUint32 classSize)
    : mClassName(className), mClassSize(classSize) { 
    Clear();
  }

  ~BloatEntry() {}

  void Clear() {
    mCurrentStats.mAddRefs = 0;
    mCurrentStats.mReleases = 0;
    mCurrentStats.mCreates = 0;
    mCurrentStats.mDestroys = 0;
    mPrevStats.mAddRefs = 0;
    mPrevStats.mReleases = 0;
    mPrevStats.mCreates = 0;
    mPrevStats.mDestroys = 0;
    mRefsOutstandingTotal = 0;
    mRefsOutstandingVariance = 0;
    mObjsOutstandingTotal = 0;
    mObjsOutstandingVariance = 0;
  }

  void AddRef(nsrefcnt refcnt) {
    mCurrentStats.mAddRefs++;
    if (refcnt == 1) {
      mCurrentStats.mCreates++;
      AccountObjs();
    }
    AccountRefs();
  }

  void Release(nsrefcnt refcnt) {
    mCurrentStats.mReleases++;
    if (refcnt == 0) {
      mCurrentStats.mDestroys++;
      AccountObjs();
    }
    AccountRefs();
  }

  void Ctor() {
    mCurrentStats.mCreates++;
  }

  void Dtor() {
    mCurrentStats.mDestroys++;
  }

  void Snapshot() {
    mPrevStats = mCurrentStats;
  }

  void AccountRefs() {
    PRInt32 cnt = (mCurrentStats.mAddRefs - mCurrentStats.mReleases);
    //    NS_ASSERTION(cnt >= 0, "too many releases");
    mRefsOutstandingTotal += cnt;
    mRefsOutstandingVariance += cnt * cnt;
  }

  void AccountObjs() {
    PRInt32 cnt = (mCurrentStats.mCreates - mCurrentStats.mDestroys);
    //    NS_ASSERTION(cnt >= 0, "too many releases");
    mObjsOutstandingTotal += cnt;
    mObjsOutstandingVariance += cnt * cnt;
  }

  static PRIntn DumpEntry(PLHashEntry *he, PRIntn i, void *arg) {
    BloatEntry* entry = (BloatEntry*)he->value;
    if (entry) {
      entry->Dump(i, (FILE*)arg);
    }
    return HT_ENUMERATE_NEXT;
  }

  static PRIntn DestroyEntry(PLHashEntry *he, PRIntn i, void *arg) {
    BloatEntry* entry = (BloatEntry*)he->value;
    if (entry) {
      delete entry;
    }
    return HT_ENUMERATE_REMOVE | HT_ENUMERATE_NEXT;
  }

  static PRIntn SnapshotEntry(PLHashEntry *he, PRIntn i, void *arg) {
    BloatEntry* entry = (BloatEntry*)he->value;
    if (entry) {
      entry->Snapshot();
    }
    return HT_ENUMERATE_REMOVE | HT_ENUMERATE_NEXT;
  }

  static PRIntn GatherEntry(PLHashEntry *he, PRIntn i, void *arg) {
    BloatEntry* entry = (BloatEntry*)he->value;
    GatherArgs* ga = (GatherArgs*) arg;
    if (arg && entry && ga->func) {
      PRBool stop = (*ga->func)(entry->mClassName, entry->mClassSize,
                                &entry->mCurrentStats, &entry->mPrevStats,
                                ga->closure);
      if (stop) {
        return HT_ENUMERATE_STOP;
      }
    }
    return HT_ENUMERATE_NEXT;
  }

  PRBool HaveLeaks() const {
    return ((mCurrentStats.mAddRefs != mCurrentStats.mReleases) ||
            (mCurrentStats.mCreates != mCurrentStats.mDestroys));
  }

  void Dump(PRIntn i, FILE* fp) {
    if (gDumpLeaksOnly && !HaveLeaks()) {
      return;
    }
#if 1
    double meanRefCnts = mRefsOutstandingTotal /
      (mCurrentStats.mAddRefs + mCurrentStats.mReleases);
    double varRefCnts = fabs(mRefsOutstandingVariance /
                             mRefsOutstandingTotal - meanRefCnts * meanRefCnts);
    double meanObjs = mObjsOutstandingTotal /
      (mCurrentStats.mCreates + mCurrentStats.mDestroys);
    double varObjs = fabs(mObjsOutstandingVariance /
                             mObjsOutstandingTotal - meanObjs * meanObjs);
    fprintf(fp, "%4d %-20.20s %8d %8d (%8.2f +/- %8.2f) %8d %8d (%8.2f +/- %8.2f) %8d %8d\n",
            i, mClassName, 
            (mCurrentStats.mAddRefs - mCurrentStats.mReleases),
            mCurrentStats.mAddRefs,
            meanRefCnts,
            sqrt(varRefCnts),
            (mCurrentStats.mCreates - mCurrentStats.mDestroys),
            mCurrentStats.mCreates,
            meanObjs,
            sqrt(varObjs),
            mClassSize,
            (mCurrentStats.mCreates - mCurrentStats.mDestroys) * mClassSize);
#else
    fprintf(fp, "%4d %-20.20s %8d %8d %8d %8d %8d %8d\n",
            i, mClassName, 
            mCurrentStats.mCreates,
            (mCurrentStats.mCreates - mCurrentStats.mDestroys),
            mCurrentStats.mAddRefs,
            (mCurrentStats.mAddRefs - mCurrentStats.mReleases),
            mClassSize,
            (mCurrentStats.mCreates - mCurrentStats.mDestroys) * mClassSize);
#endif
  }

protected:
  const char*   mClassName;
  PRUint32      mClassSize;
  double        mRefsOutstandingTotal;
  double        mRefsOutstandingVariance;
  double        mObjsOutstandingTotal;
  double        mObjsOutstandingVariance;
  nsTraceRefcntStats mCurrentStats;
  nsTraceRefcntStats mPrevStats;
};

static void
RecreateBloatView()
{
  gBloatView = PL_NewHashTable(256, 
                               PL_HashString,
                               PL_CompareStrings,
                               PL_CompareValues,
                               NULL, NULL);
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
    if (entry == NULL) {
      entry = new BloatEntry(aTypeName, aInstanceSize);
      PLHashEntry* e = PL_HashTableAdd(gBloatView, aTypeName, entry);
      if (e == NULL) {
        delete entry;
        entry = NULL;
      }
    }
  }
  return entry;
}
#endif /* NS_BUILD_REFCNT_LOGGING */
 
void
nsTraceRefcnt::DumpStatistics()
{
#ifdef NS_BUILD_REFCNT_LOGGING
  if (!gTrackBloat) {
    return;
  }

  LOCK_TRACELOG();
  if (gDumpLeaksOnly) {
    printf("Bloaty: Only dumping data about objects that leaked\n");
  }

//  fprintf(stdout, "     Name                  AddRefs    [mean / stddev] Objects    [mean / stddev]    Size TotalSize\n");
//  fprintf(stdout, "     Name                 Tot-Objs Rem-Objs Tot-Adds Rem-Adds Obj-Size  Mem-Use\n");
  fprintf(stdout, "                                           Bloaty: Refcounting and Memory Bloat Statistics\n");
  fprintf(stdout, "     |<-------Name------>|<--------------References-------------->|<----------------Objects---------------->|<------Size----->|\n");
  fprintf(stdout, "                               Rem    Total      Mean       StdDev       Rem    Total      Mean       StdDev Per-Class      Rem\n");
  PL_HashTableDump(gBloatView, BloatEntry::DumpEntry, stdout);

  UNLOCK_TRACELOG();
#endif
}

void
nsTraceRefcnt::ResetStatistics()
{
#ifdef NS_BUILD_REFCNT_LOGGING
  LOCK_TRACELOG();
  if (gBloatView) {
    PL_HashTableEnumerateEntries(gBloatView, BloatEntry::DestroyEntry, 0);
    PL_HashTableDestroy(gBloatView);
    gBloatView = nsnull;
  }
  UNLOCK_TRACELOG();
#endif
}

void
nsTraceRefcnt::GatherStatistics(nsTraceRefcntStatFunc aFunc,
                                void* aClosure)
{
#ifdef NS_BUILD_REFCNT_LOGGING
  LOCK_TRACELOG();

  if (gBloatView) {
    GatherArgs ga;
    ga.func = aFunc;
    ga.closure = aClosure;
    PL_HashTableEnumerateEntries(gBloatView, BloatEntry::GatherEntry,
                                 (void*) &ga);
  }

  UNLOCK_TRACELOG();
#endif
}

void
nsTraceRefcnt::SnapshotStatistics()
{
#ifdef NS_BUILD_REFCNT_LOGGING
  LOCK_TRACELOG();

  if (gBloatView) {
    PL_HashTableEnumerateEntries(gBloatView, BloatEntry::SnapshotEntry, 0);
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

static void InitTraceLog(void)
{
  if (0 == gTraceRefcntLog) {
    gTraceRefcntLog = PR_NewLogModule("xpcomrefcnt");

#if defined(XP_UNIX) || defined(_WIN32)
    if (getenv("MOZ_DUMP_LEAKS")) {
      gDumpLeaksOnly = PR_TRUE;
    }
#endif

    // See if bloaty is enabled
    if (XPCOM_REFCNT_TRACK_BLOAT & gTraceRefcntLog->level) {
      gTrackBloat = PR_TRUE;
      RecreateBloatView();
      if (NS_WARN_IF_FALSE(gBloatView, "out of memory")) {
        gTrackBloat = PR_FALSE;
      }
      else {
        printf("XPCOM: using turbo mega bloatvision\n");
      }
    }

    // See if raw nspr logging is enabled
    if (XPCOM_REFCNT_LOG_ALL & gTraceRefcntLog->level) {
      gLogAllRefcnts = PR_TRUE;
      printf("XPCOM: logging all refcnt calls\n");
    }
    else if (XPCOM_REFCNT_LOG_SOME & gTraceRefcntLog->level) {
      gLogSomeRefcnts = PR_TRUE;
      gTypesToLog = PL_NewHashTable(256,
                                    PL_HashString,
                                    PL_CompareStrings,
                                    PL_CompareValues,
                                    NULL, NULL);
      if (NS_WARN_IF_FALSE(gTypesToLog, "out of memory")) {
        gLogSomeRefcnts = PR_FALSE;
      }
      else {
#if defined(XP_UNIX) || defined (_WIN32)
        char* types = getenv("MOZ_TRACE_REFCNTS_TYPES");
        if (types) {
          printf("XPCOM: logging some refcnt calls: ");
          char* cp = types;
          for (;;) {
            char* cm = strchr(cp, ',');
            if (cm) {
              *cm = '\0';
            }
            PL_HashTableAdd(gTypesToLog, strdup(cp), (void*)1);
            printf("%s ", cp);
            if (!cm) break;
            *cm = ',';
            cp = cm + 1;
          }
          printf("\n");
        }
        else {
          printf("XPCOM: MOZ_TRACE_REFCNTS_TYPES wasn't set; can't log some refcnts\n");
          gLogSomeRefcnts = PR_FALSE;
        }
#endif
      }
    }
    if (XPCOM_REFCNT_LOG_CALLS & gTraceRefcntLog->level) {
      gLogCalls = PR_TRUE;
    }
    if (XPCOM_REFCNT_LOG_NEW & gTraceRefcntLog->level) {
      gLogNewAndDelete = PR_TRUE;
    }

    // See if we should log to leaky instead of to nspr
    if (XPCOM_REFCNT_LOG_TO_LEAKY & gTraceRefcntLog->level) {
      gLogToLeaky = PR_TRUE;
#ifdef HAVE_LIBDL
      void* p = dlsym(0, "__log_addref");
      if (p) {
        leakyLogAddRef = (void (*)(void*,int,int)) p;
        p = dlsym(0, "__log_release");
        if (p) {
          leakyLogRelease = (void (*)(void*,int,int)) p;
          printf("XPCOM: logging addref/release calls to leaky\n");
        }
        else {
          gLogToLeaky = PR_FALSE;
        }
      }
      else {
        gLogToLeaky = PR_FALSE;
      }
#endif
    }

    if (gTrackBloat || gLogAllRefcnts || gLogSomeRefcnts ||
        gLogCalls || gLogNewAndDelete) {
      gLogging = PR_TRUE;
    }

#if defined(NS_MT_SUPPORTED)
    gTraceLock = PR_NewLock();
#endif /* NS_MT_SUPPORTED */

  }
}
#endif


static int nsIToA16(PRUint32 aNumber, char* aBuffer)
{
  static char kHex[] = "0123456789abcdef";

  if (aNumber == 0) {
    *aBuffer = '0';
    return 1;
  }

  char buf[8];
  PRInt32 count = 0;
  while (aNumber != 0) {
    PRUint32 nibble = aNumber & 0xf;
    buf[count++] = kHex[nibble];
    aNumber >>= 4;
  }

  for (PRInt32 i = count - 1; i >= 0; --i)
    *aBuffer++ = buf[i];

  return count;
}

#if defined(_WIN32) && defined(_M_IX86) // WIN32 x86 stack walking code
#include "imagehlp.h"
#include <stdio.h>

// Define these as static pointers so that we can load the DLL on the
// fly (and not introduce a link-time dependency on it). Tip o' the
// hat to Matt Pietrick for this idea. See:
//
//   http://msdn.microsoft.com/library/periodic/period97/F1/D3/S245C6.htm
//
typedef BOOL (__stdcall *SYMINITIALIZEPROC)(HANDLE, LPSTR, BOOL);
static SYMINITIALIZEPROC _SymInitialize;

typedef BOOL (__stdcall *SYMCLEANUPPROC)(HANDLE);
static SYMCLEANUPPROC _SymCleanup;

typedef BOOL (__stdcall *STACKWALKPROC)(DWORD,
                                        HANDLE,
                                        HANDLE,
                                        LPSTACKFRAME,
                                        LPVOID,
                                        PREAD_PROCESS_MEMORY_ROUTINE,
                                        PFUNCTION_TABLE_ACCESS_ROUTINE,
                                        PGET_MODULE_BASE_ROUTINE,
                                        PTRANSLATE_ADDRESS_ROUTINE);
static STACKWALKPROC _StackWalk;

typedef LPVOID (__stdcall *SYMFUNCTIONTABLEACCESSPROC)(HANDLE, DWORD);
static SYMFUNCTIONTABLEACCESSPROC _SymFunctionTableAccess;

typedef DWORD (__stdcall *SYMGETMODULEBASEPROC)(HANDLE, DWORD);
static SYMGETMODULEBASEPROC _SymGetModuleBase;

typedef BOOL (__stdcall *SYMGETSYMFROMADDRPROC)(HANDLE, DWORD, PDWORD, PIMAGEHLP_SYMBOL);
static SYMGETSYMFROMADDRPROC _SymGetSymFromAddr;


static PRBool
EnsureSymInitialized()
{
  PRBool gInitialized = PR_FALSE;

  if (! gInitialized) {
    HMODULE module = ::LoadLibrary("IMAGEHLP.DLL");
    if (!module) return PR_FALSE;

    _SymInitialize = (SYMINITIALIZEPROC) ::GetProcAddress(module, "SymInitialize");
    if (!_SymInitialize) return PR_FALSE;

    _SymCleanup = (SYMCLEANUPPROC)GetProcAddress(module, "SymCleanup");
    if (!_SymCleanup) return PR_FALSE;

    _StackWalk = (STACKWALKPROC)GetProcAddress(module, "StackWalk");
    if (!_StackWalk) return PR_FALSE;

    _SymFunctionTableAccess = (SYMFUNCTIONTABLEACCESSPROC) GetProcAddress(module, "SymFunctionTableAccess");
    if (!_SymFunctionTableAccess) return PR_FALSE;

    _SymGetModuleBase = (SYMGETMODULEBASEPROC)GetProcAddress(module, "SymGetModuleBase");
    if (!_SymGetModuleBase) return PR_FALSE;

    _SymGetSymFromAddr = (SYMGETSYMFROMADDRPROC)GetProcAddress(module, "SymGetSymFromAddr");
    if (!_SymGetSymFromAddr) return PR_FALSE;

    gInitialized = _SymInitialize(GetCurrentProcess(), 0, TRUE);
  }

  return gInitialized;
}

/**
 * Walk the stack, translating PC's found into strings and recording the
 * chain in aBuffer. For this to work properly, the dll's must be rebased
 * so that the address in the file agrees with the address in memory.
 * Otherwise StackWalk will return FALSE when it hits a frame in a dll's
 * whose in memory address doesn't match it's in-file address.
 *
 * Fortunately, there is a handy dandy routine in IMAGEHLP.DLL that does
 * the rebasing and accordingly I've made a tool to use it to rebase the
 * DLL's in one fell swoop (see xpcom/tools/windows/rebasedlls.cpp).
 */
void
nsTraceRefcnt::WalkTheStack(char* aBuffer, int aBufLen)
{
  aBuffer[0] = '\0';
  aBufLen--;                    // leave room for nul

  HANDLE myProcess = ::GetCurrentProcess();
  HANDLE myThread = ::GetCurrentThread();

  BOOL ok;

  ok = EnsureSymInitialized();
  if (! ok)
    return;

  // Get the context information for this thread. That way we will
  // know where our sp, fp, pc, etc. are and can fill in the
  // STACKFRAME with the initial values.
  CONTEXT context;
  context.ContextFlags = CONTEXT_FULL;
  ok = GetThreadContext(myThread, &context);
  if (! ok)
    return;

  // Setup initial stack frame to walk from
  STACKFRAME frame;
  memset(&frame, 0, sizeof(frame));
  frame.AddrPC.Offset    = context.Eip;
  frame.AddrPC.Mode      = AddrModeFlat;
  frame.AddrStack.Offset = context.Esp;
  frame.AddrStack.Mode   = AddrModeFlat;
  frame.AddrFrame.Offset = context.Ebp;
  frame.AddrFrame.Mode   = AddrModeFlat;

  // Now walk the stack and map the pc's to symbol names that we stuff
  // append to *cp.
  char* cp = aBuffer;

  int skip = 2;
  while (aBufLen > 0) {
    ok = _StackWalk(IMAGE_FILE_MACHINE_I386,
                   myProcess,
                   myThread,
                   &frame,
                   &context,
                   0,                        // read process memory routine
                   _SymFunctionTableAccess,  // function table access routine
                   _SymGetModuleBase,        // module base routine
                   0);                       // translate address routine

    if (!ok || frame.AddrPC.Offset == 0)
      break;

    if (skip-- > 0)
      continue;

    char buf[sizeof(IMAGEHLP_SYMBOL) + 512];
    PIMAGEHLP_SYMBOL symbol = (PIMAGEHLP_SYMBOL) buf;
    symbol->SizeOfStruct = sizeof(buf);
    symbol->MaxNameLength = 512;

    DWORD displacement;
    ok = _SymGetSymFromAddr(myProcess,
                            frame.AddrPC.Offset,
                            &displacement,
                            symbol);

    if (ok) {
      int nameLen = strlen(symbol->Name);
      if (nameLen + 12 > aBufLen) { // 12 == strlen("+0x12345678 ")
        break;
      }
      char* cp2 = symbol->Name;
      while (*cp2) {
        if (*cp2 == ' ') *cp2 = '_'; // replace spaces with underscores
        *cp++ = *cp2++;
      }
      aBufLen -= nameLen;
      *cp++ = '+';
      *cp++ = '0';
      *cp++ = 'x';
      PRInt32 len = nsIToA16(displacement, cp);
      cp += len;
      *cp++ = ' ';

      aBufLen -= nameLen + len + 4;
    }
    else {
      if (11 > aBufLen) { // 11 == strlen("0x12345678 ")
        break;
      }
      *cp++ = '0';
      *cp++ = 'x';
      PRInt32 len = nsIToA16(frame.AddrPC.Offset, cp);
      cp += len;
      *cp++ = ' ';
      aBufLen -= len + 3;
    }
  }
  *cp = 0;
}
/* _WIN32 */


#elif defined(linux) && defined(__GLIBC__) && defined(__i386) // i386 Linux stackwalking code

void
nsTraceRefcnt::WalkTheStack(char* aBuffer, int aBufLen)
{
  aBuffer[0] = '\0';
  aBufLen--; // leave room for nul

  char* cp = aBuffer;

  jmp_buf jb;
  setjmp(jb);

  // Stack walking code courtesy Kipp's "leaky". 
  u_long* bp = (u_long*) (jb[0].__jmpbuf[JB_BP]);
  int skip = 2;
  for (;;) {
    u_long* nextbp = (u_long*) *bp++;
    u_long pc = *bp;
    if ((pc < 0x08000000) || (pc > 0x7fffffff) || (nextbp < bp)) {
      break;
    }
    if (--skip <= 0) {
      Dl_info info;
      int ok = dladdr((void*) pc, &info);
      if (ok < 0)
        break;

      const char * symbol = info.dli_sname;

      int len = strlen(symbol);
      if (! len)
        break; // XXX Lazy. We could look at the filename or something.

      char demangled[4096] = "\0";

      DemangleSymbol(symbol,demangled,sizeof(demangled));

      if (demangled && strlen(demangled))
      {
        symbol = demangled;
        len = strlen(symbol);
      }
        
      if (len + 12 >= aBufLen) // 12 == strlen("+0x12345678 ")
        break;

      strcpy(cp, symbol);
      cp += len;

      *cp++ = '+';
      *cp++ = '0';
      *cp++ = 'x';

      PRUint32 off = (char*)pc - (char*)info.dli_saddr;
      PRInt32 addrStrLen = nsIToA16(off, cp);
      cp += addrStrLen;

      *cp++ = '\t';

      aBufLen -= addrStrLen + 4;
    }
    bp = nextbp;
  }
  *cp = '\0';
}

#else // unsupported platform.

NS_COM void
nsTraceRefcnt::WalkTheStack(char* aBuffer, int aBufLen)
{
  // Write me!!!
  *aBuffer = '\0';
}

#endif

//----------------------------------------------------------------------

// This thing is exported by libiberty.a (-liberty)
// Yes, this is a gcc only hack
#if defined(MOZ_DEMANGLE_SYMBOLS)
extern "C" char * cplus_demangle(const char *,int);
#include <stdlib.h> // for free()
#endif // MOZ_DEMANGLE_SYMBOLS

#ifdef __linux__
NS_COM void 
nsTraceRefcnt::DemangleSymbol(const char * aSymbol, 
                              char * aBuffer,
                              int aBufLen)
{
  NS_ASSERTION(nsnull != aSymbol,"null symbol");
  NS_ASSERTION(nsnull != aBuffer,"null buffer");
  NS_ASSERTION(aBufLen >= 32 ,"pulled 32 out of you know where");

  aBuffer[0] = '\0';

#if defined(MOZ_DEMANGLE_SYMBOLS)
  /* See demangle.h in the gcc source for the voodoo */
  char * demangled = cplus_demangle(aSymbol,3);

  if (demangled)
  {
    strncpy(aBuffer,demangled,aBufLen);

    free(demangled);
  }
#endif // MOZ_DEMANGLE_SYMBOLS
}

#else // __linux__

NS_COM void 
nsTraceRefcnt::DemangleSymbol(const char * aSymbol, 
                              char * aBuffer,
                              int aBufLen)
{
  NS_ASSERTION(nsnull != aSymbol,"null symbol");
  NS_ASSERTION(nsnull != aBuffer,"null buffer");

  // lose
  aBuffer[0] = '\0';
}
#endif // __linux__

//----------------------------------------------------------------------

NS_COM void
nsTraceRefcnt::LoadLibrarySymbols(const char* aLibraryName,
                                  void* aLibrayHandle)
{
#ifdef NS_BUILD_REFCNT_LOGGING
#if defined(_WIN32) && defined(_M_IX86) /* Win32 x86 only */
  InitTraceLog();
  if (PR_LOG_TEST(gTraceRefcntLog,PR_LOG_DEBUG)) {
    HANDLE myProcess = ::GetCurrentProcess();

    if (!SymInitialize(myProcess, ".;..\\lib", TRUE)) {
      return;
    }

    BOOL b = ::SymLoadModule(myProcess,
                             NULL,
                             (char*)aLibraryName,
                             (char*)aLibraryName,
                             0,
                             0);
//  DWORD lastError = 0;
//  if (!b) lastError = ::GetLastError();
//  printf("loading symbols for library %s => %s [%d]\n", aLibraryName,
//         b ? "true" : "false", lastError);
  }
#endif
#endif
}

//----------------------------------------------------------------------

NS_COM void
nsTraceRefcnt::LogAddRef(void* aPtr,
                         nsrefcnt aRefCnt,
                         const char* aClazz,
                         PRUint32 classSize)
{
#ifdef NS_BUILD_REFCNT_LOGGING
  InitTraceLog();
  if (gLogging) {
    LOCK_TRACELOG();

    if (gTrackBloat) {
      BloatEntry* entry = GetBloatEntry(aClazz, classSize);
      if (entry) {
        entry->AddRef(aRefCnt);
      }
    }

    if (gLogAllRefcnts || (gTypesToLog && LogThisType(aClazz))) {
      if (gLogToLeaky) {
        (*leakyLogAddRef)(aPtr, aRefCnt - 1, aRefCnt);
      }
      else {
        char sb[16384];
        WalkTheStack(sb, sizeof(sb));
        // Can't use PR_LOG(), b/c it truncates the line
        printf("%s\t%p\tAddRef\t%d\t%s\n", aClazz, aPtr, aRefCnt, sb);
      }
    }

    UNLOCK_TRACELOG();
  }
#endif
}


NS_COM void
nsTraceRefcnt::LogRelease(void* aPtr,
                          nsrefcnt aRefCnt,
                          const char* aClazz)
{
#ifdef NS_BUILD_REFCNT_LOGGING
  InitTraceLog();
  if (gLogging) {
    LOCK_TRACELOG();

    if (gTrackBloat) {
      BloatEntry* entry = GetBloatEntry(aClazz, (PRUint32)-1);
      if (entry) {
        entry->Release(aRefCnt);
      }
    }
    if (gLogAllRefcnts || (gTypesToLog && LogThisType(aClazz))) {
      if (gLogToLeaky) {
        (*leakyLogRelease)(aPtr, aRefCnt + 1, aRefCnt);
      }
      else {
        char sb[16384];
        WalkTheStack(sb, sizeof(sb));
        // Can't use PR_LOG(), b/c it truncates the line
        printf("%s\t%p\tRelease\t%d\t%s\n", aClazz, aPtr, aRefCnt, sb);
      }
    }

    UNLOCK_TRACELOG();
  }
#endif
}

NS_COM nsrefcnt
nsTraceRefcnt::LogAddRefCall(void* aPtr,
                             nsrefcnt aNewRefcnt,
                             const char* aFile,
                             int aLine)
{
#ifdef NS_BUILD_REFCNT_LOGGING
  InitTraceLog();
  if (gLogCalls) {
    LOCK_TRACELOG();

    char sb[1000];
    WalkTheStack(sb, sizeof(sb));
    PR_LOG(gTraceRefcntLog, PR_LOG_DEBUG,
           ("AddRef: %p: %d=>%d [%s] in %s (line %d)",
            aPtr, aNewRefcnt-1, aNewRefcnt, sb, aFile, aLine));

    UNLOCK_TRACELOG();
  }
#endif
  return aNewRefcnt;
}

NS_COM nsrefcnt
nsTraceRefcnt::LogReleaseCall(void* aPtr,
                              nsrefcnt aNewRefcnt,
                              const char* aFile,
                              int aLine)
{
#ifdef NS_BUILD_REFCNT_LOGGING
  InitTraceLog();

  if (gLogCalls) {
    LOCK_TRACELOG();

    char sb[1000];
    WalkTheStack(sb, sizeof(sb));
    PR_LOG(gTraceRefcntLog, PR_LOG_DEBUG,
           ("Release: %p: %d=>%d [%s] in %s (line %d)",
            aPtr, aNewRefcnt+1, aNewRefcnt, sb, aFile, aLine));

    UNLOCK_TRACELOG();
  }
#endif
  return aNewRefcnt;
}

NS_COM void
nsTraceRefcnt::LogNewXPCOM(void* aPtr,
                           const char* aType,
                           PRUint32 aInstanceSize,
                           const char* aFile,
                           int aLine)
{
#ifdef NS_BUILD_REFCNT_LOGGING
  InitTraceLog();

  if (gLogNewAndDelete) {
    LOCK_TRACELOG();

    char sb[1000];
    WalkTheStack(sb, sizeof(sb));
    PR_LOG(gTraceRefcntLog, PR_LOG_DEBUG,
           ("Create: %p[%s]: [%s] in %s (line %d)",
            aPtr, aType, sb, aFile, aLine));

    UNLOCK_TRACELOG();
  }
#endif
}

NS_COM void
nsTraceRefcnt::LogDeleteXPCOM(void* aPtr,
                              const char* aFile,
                              int aLine)
{
#ifdef NS_BUILD_REFCNT_LOGGING
  InitTraceLog();

  if (gLogNewAndDelete) {
    LOCK_TRACELOG();

    char sb[1000];
    WalkTheStack(sb, sizeof(sb));
    PR_LOG(gTraceRefcntLog, PR_LOG_DEBUG,
           ("Destroy: %p: [%s] in %s (line %d)",
            aPtr, sb, aFile, aLine));

    UNLOCK_TRACELOG();
  }
#endif
}

NS_COM void
nsTraceRefcnt::LogCtor(void* aPtr,
                       const char* aTypeName,
                       PRUint32 aInstanceSize)
{
#ifdef NS_BUILD_REFCNT_LOGGING
  InitTraceLog();
  if (gLogging) {
    LOCK_TRACELOG();

    if (gTrackBloat) {
      BloatEntry* entry = GetBloatEntry(aTypeName, aInstanceSize);
      if (entry) {
        entry->Ctor();
      }
    }

    UNLOCK_TRACELOG();
  }
#endif
}

NS_COM void
nsTraceRefcnt::LogDtor(void* aPtr, const char* aTypeName,
                       PRUint32 aInstanceSize)
{
#ifdef NS_BUILD_REFCNT_LOGGING
  InitTraceLog();
  if (gLogging) {
    LOCK_TRACELOG();

    if (gTrackBloat) {
      BloatEntry* entry = GetBloatEntry(aTypeName, aInstanceSize);
      if (entry) {
        entry->Dtor();
      }
    }

    UNLOCK_TRACELOG();
  }
#endif
}
