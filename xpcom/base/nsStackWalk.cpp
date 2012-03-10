/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set shiftwidth=4 tabstop=8 autoindent cindent expandtab: */
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
 * The Original Code is Mozilla stack walking code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Judge, 20-December-2000
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation
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

/* API for getting a stack trace of the C/C++ stack on the current thread */

#include "mozilla/Util.h"
#include "mozilla/StackWalk.h"
#include "nsDebug.h"
#include "nsStackWalkPrivate.h"

#include "nsStackWalk.h"

using namespace mozilla;

// The presence of this address is the stack must stop the stack walk. If
// there is no such address, the structure will be {NULL, true}.
struct CriticalAddress {
  void* mAddr;
  bool mInit;
};
static CriticalAddress gCriticalAddress;

#if defined(HAVE_DLOPEN) || defined(XP_MACOSX)
#include <dlfcn.h>
#endif

#define NSSTACKWALK_SUPPORTS_MACOSX \
    (defined(XP_MACOSX) && \
     (defined(__i386) || defined(__ppc__) || defined(HAVE__UNWIND_BACKTRACE)))

#define NSSTACKWALK_SUPPORTS_LINUX \
    (defined(linux) && \
     ((defined(__GNUC__) && (defined(__i386) || defined(PPC))) || \
      defined(HAVE__UNWIND_BACKTRACE)))

#define NSSTACKWALK_SUPPORTS_SOLARIS \
    (defined(__sun) && \
     (defined(__sparc) || defined(sparc) || defined(__i386) || defined(i386)))

#if NSSTACKWALK_SUPPORTS_MACOSX
#include <pthread.h>
#include <errno.h>
#include <CoreServices/CoreServices.h>

typedef void
malloc_logger_t(uint32_t type, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3,
                uintptr_t result, uint32_t num_hot_frames_to_skip);
extern malloc_logger_t *malloc_logger;

static void
stack_callback(void *pc, void *closure)
{
  const char *name = reinterpret_cast<char *>(closure);
  Dl_info info;

  // On Leopard dladdr returns the wrong value for "new_sem_from_pool". The
  // stack shows up as having two pthread_cond_wait$UNIX2003 frames. The
  // correct one is the first that we find on our way up, so the
  // following check for gCriticalAddress.mAddr is critical.
  if (gCriticalAddress.mAddr || dladdr(pc, &info) == 0  ||
      info.dli_sname == NULL || strcmp(info.dli_sname, name) != 0)
    return;
  gCriticalAddress.mAddr = pc;
}

#define MAC_OS_X_VERSION_10_7_HEX 0x00001070
#define MAC_OS_X_VERSION_10_6_HEX 0x00001060

static PRInt32 OSXVersion()
{
  static PRInt32 gOSXVersion = 0x0;
  if (gOSXVersion == 0x0) {
    OSErr err = ::Gestalt(gestaltSystemVersion, (SInt32*)&gOSXVersion);
    MOZ_ASSERT(err == noErr);
  }
  return gOSXVersion;
}

static bool OnLionOrLater()
{
  return (OSXVersion() >= MAC_OS_X_VERSION_10_7_HEX);
}

static bool OnSnowLeopardOrLater()
{
  return (OSXVersion() >= MAC_OS_X_VERSION_10_6_HEX);
}

static void
my_malloc_logger(uint32_t type, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3,
                 uintptr_t result, uint32_t num_hot_frames_to_skip)
{
  static bool once = false;
  if (once)
    return;
  once = true;

  // On Leopard dladdr returns the wrong value for "new_sem_from_pool". The
  // stack shows up as having two pthread_cond_wait$UNIX2003 frames.
  const char *name = OnSnowLeopardOrLater() ? "new_sem_from_pool" :
    "pthread_cond_wait$UNIX2003";
  NS_StackWalk(stack_callback, 0, const_cast<char*>(name), 0);
}

void
StackWalkInitCriticalAddress()
{
  if(gCriticalAddress.mInit)
    return;
  gCriticalAddress.mInit = true;
  // We must not do work when 'new_sem_from_pool' calls realloc, since
  // it holds a non-reentrant spin-lock and we will quickly deadlock.
  // new_sem_from_pool is not directly accessible using dlsym, so
  // we force a situation where new_sem_from_pool is on the stack and
  // use dladdr to check the addresses.

  MOZ_ASSERT(malloc_logger == NULL);
  malloc_logger = my_malloc_logger;

  pthread_cond_t cond;
  int r = pthread_cond_init(&cond, 0);
  MOZ_ASSERT(r == 0);
  pthread_mutex_t mutex;
  r = pthread_mutex_init(&mutex,0);
  MOZ_ASSERT(r == 0);
  r = pthread_mutex_lock(&mutex);
  MOZ_ASSERT(r == 0);
  struct timespec abstime = {0, 1};
  r = pthread_cond_timedwait_relative_np(&cond, &mutex, &abstime);
  malloc_logger = NULL;

  // On Lion, malloc is no longer called from pthread_cond_*wait*. This prevents
  // us from finding the address, but that is fine, since with no call to malloc
  // there is no critical address.
  MOZ_ASSERT(OnLionOrLater() || gCriticalAddress.mAddr != NULL);
  MOZ_ASSERT(r == ETIMEDOUT);
  r = pthread_mutex_unlock(&mutex);
  MOZ_ASSERT(r == 0);
  r = pthread_mutex_destroy(&mutex);
  MOZ_ASSERT(r == 0);
  r = pthread_cond_destroy(&cond);
  MOZ_ASSERT(r == 0);
}

static bool IsCriticalAddress(void* aPC)
{
  return gCriticalAddress.mAddr == aPC;
}
#else
static bool IsCriticalAddress(void* aPC)
{
  return false;
}
// We still initialize gCriticalAddress.mInit so that this code behaves
// the same on all platforms. Otherwise a failure to init would be visible
// only on OS X.
void
StackWalkInitCriticalAddress()
{
  gCriticalAddress.mInit = true;
}
#endif

#if defined(_WIN32) && (defined(_M_IX86) || defined(_M_AMD64) || defined(_M_IA64)) // WIN32 x86 stack walking code

#include "nscore.h"
#include <windows.h>
#include <process.h>
#include <stdio.h>
#include "plstr.h"
#include "mozilla/FunctionTimer.h"

#include "nspr.h"
#include <imagehlp.h>
// We need a way to know if we are building for WXP (or later), as if we are, we
// need to use the newer 64-bit APIs. API_VERSION_NUMBER seems to fit the bill.
// A value of 9 indicates we want to use the new APIs.
#if API_VERSION_NUMBER < 9
#error Too old imagehlp.h
#endif

// Define these as static pointers so that we can load the DLL on the
// fly (and not introduce a link-time dependency on it). Tip o' the
// hat to Matt Pietrick for this idea. See:
//
//   http://msdn.microsoft.com/library/periodic/period97/F1/D3/S245C6.htm
//
PR_BEGIN_EXTERN_C

extern HANDLE hStackWalkMutex; 

bool EnsureSymInitialized();

bool EnsureImageHlpInitialized();

struct WalkStackData {
  PRUint32 skipFrames;
  HANDLE thread;
  bool walkCallingThread;
  HANDLE process;
  HANDLE eventStart;
  HANDLE eventEnd;
  void **pcs;
  PRUint32 pc_size;
  PRUint32 pc_count;
};

void PrintError(char *prefix, WalkStackData* data);
unsigned int WINAPI WalkStackThread(void* data);
void WalkStackMain64(struct WalkStackData* data);


DWORD gStackWalkThread;
CRITICAL_SECTION gDbgHelpCS;

PR_END_EXTERN_C

// Routine to print an error message to standard error.
// Will also call callback with error, if data supplied.
void PrintError(char *prefix)
{
    LPVOID lpMsgBuf;
    DWORD lastErr = GetLastError();
    FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,
      lastErr,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
      (LPSTR) &lpMsgBuf,
      0,
      NULL
    );
    fprintf(stderr, "### ERROR: %s: %s",
                    prefix, lpMsgBuf ? lpMsgBuf : "(null)\n");
    fflush(stderr);
    LocalFree(lpMsgBuf);
}

bool
EnsureImageHlpInitialized()
{
    static bool gInitialized = false;

    if (gInitialized)
        return gInitialized;

    // Hope that our first call doesn't happen during static
    // initialization.  If it does, this CreateThread call won't
    // actually start the thread until after the static initialization
    // is done, which means we'll deadlock while waiting for it to
    // process a stack.
    HANDLE readyEvent = ::CreateEvent(NULL, FALSE /* auto-reset*/,
                            FALSE /* initially non-signaled */, NULL);
    unsigned int threadID;
    HANDLE hStackWalkThread = (HANDLE)
      _beginthreadex(NULL, 0, WalkStackThread, (void*)readyEvent,
                     0, &threadID);
    gStackWalkThread = threadID;
    if (hStackWalkThread == NULL) {
        PrintError("CreateThread");
        return false;
    }
    ::CloseHandle(hStackWalkThread);

    // Wait for the thread's event loop to start before posting events to it.
    ::WaitForSingleObject(readyEvent, INFINITE);
    ::CloseHandle(readyEvent);

    ::InitializeCriticalSection(&gDbgHelpCS);

    return gInitialized = true;
}

void
WalkStackMain64(struct WalkStackData* data)
{
    // Get the context information for the thread. That way we will
    // know where our sp, fp, pc, etc. are and can fill in the
    // STACKFRAME64 with the initial values.
    CONTEXT context;
    HANDLE myProcess = data->process;
    HANDLE myThread = data->thread;
    DWORD64 addr;
    STACKFRAME64 frame64;
    // skip our own stack walking frames
    int skip = (data->walkCallingThread ? 3 : 0) + data->skipFrames;
    BOOL ok;

    // Get a context for the specified thread.
    memset(&context, 0, sizeof(CONTEXT));
    context.ContextFlags = CONTEXT_FULL;
    if (!GetThreadContext(myThread, &context)) {
        PrintError("GetThreadContext");
        return;
    }

    // Setup initial stack frame to walk from
    memset(&frame64, 0, sizeof(frame64));
#ifdef _M_IX86
    frame64.AddrPC.Offset    = context.Eip;
    frame64.AddrStack.Offset = context.Esp;
    frame64.AddrFrame.Offset = context.Ebp;
#elif defined _M_AMD64
    frame64.AddrPC.Offset    = context.Rip;
    frame64.AddrStack.Offset = context.Rsp;
    frame64.AddrFrame.Offset = context.Rbp;
#elif defined _M_IA64
    frame64.AddrPC.Offset    = context.StIIP;
    frame64.AddrStack.Offset = context.SP;
    frame64.AddrFrame.Offset = context.RsBSP;
#else
#error "Should not have compiled this code"
#endif
    frame64.AddrPC.Mode      = AddrModeFlat;
    frame64.AddrStack.Mode   = AddrModeFlat;
    frame64.AddrFrame.Mode   = AddrModeFlat;
    frame64.AddrReturn.Mode  = AddrModeFlat;

    // Now walk the stack
    while (1) {

        // debug routines are not threadsafe, so grab the lock.
        EnterCriticalSection(&gDbgHelpCS);
        ok = StackWalk64(
#ifdef _M_AMD64
          IMAGE_FILE_MACHINE_AMD64,
#elif defined _M_IA64
          IMAGE_FILE_MACHINE_IA64,
#elif defined _M_IX86
          IMAGE_FILE_MACHINE_I386,
#else
#error "Should not have compiled this code"
#endif
          myProcess,
          myThread,
          &frame64,
          &context,
          NULL,
          SymFunctionTableAccess64, // function table access routine
          SymGetModuleBase64,       // module base routine
          0
        );
        LeaveCriticalSection(&gDbgHelpCS);

        if (ok)
            addr = frame64.AddrPC.Offset;
        else {
            addr = 0;
            PrintError("WalkStack64");
        }

        if (!ok || (addr == 0)) {
            break;
        }

        if (skip-- > 0) {
            continue;
        }

        if (data->pc_count < data->pc_size)
            data->pcs[data->pc_count] = (void*)addr;
        ++data->pc_count;

        if (frame64.AddrReturn.Offset == 0)
            break;
    }
    return;
}


unsigned int WINAPI
WalkStackThread(void* aData)
{
    BOOL msgRet;
    MSG msg;

    // Call PeekMessage to force creation of a message queue so that
    // other threads can safely post events to us.
    ::PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

    // and tell the thread that created us that we're ready.
    HANDLE readyEvent = (HANDLE)aData;
    ::SetEvent(readyEvent);

    while ((msgRet = ::GetMessage(&msg, (HWND)-1, 0, 0)) != 0) {
        if (msgRet == -1) {
            PrintError("GetMessage");
        } else {
            DWORD ret;

            struct WalkStackData *data = (WalkStackData *)msg.lParam;
            if (!data)
              continue;

            // Don't suspend the calling thread until it's waiting for
            // us; otherwise the number of frames on the stack could vary.
            ret = ::WaitForSingleObject(data->eventStart, INFINITE);
            if (ret != WAIT_OBJECT_0)
                PrintError("WaitForSingleObject");

            // Suspend the calling thread, dump his stack, and then resume him.
            // He's currently waiting for us to finish so now should be a good time.
            ret = ::SuspendThread( data->thread );
            if (ret == -1) {
                PrintError("ThreadSuspend");
            }
            else {
                WalkStackMain64(data);

                ret = ::ResumeThread(data->thread);
                if (ret == -1) {
                    PrintError("ThreadResume");
                }
            }

            ::SetEvent(data->eventEnd);
        }
    }

    return 0;
}

/**
 * Walk the stack, translating PC's found into strings and recording the
 * chain in aBuffer. For this to work properly, the DLLs must be rebased
 * so that the address in the file agrees with the address in memory.
 * Otherwise StackWalk will return FALSE when it hits a frame in a DLL
 * whose in memory address doesn't match its in-file address.
 */

EXPORT_XPCOM_API(nsresult)
NS_StackWalk(NS_WalkStackCallback aCallback, PRUint32 aSkipFrames,
             void *aClosure, uintptr_t aThread)
{
    MOZ_ASSERT(gCriticalAddress.mInit);
    HANDLE myProcess, myThread;
    DWORD walkerReturn;
    struct WalkStackData data;

    if (!EnsureImageHlpInitialized())
        return false;

    HANDLE targetThread = ::GetCurrentThread();
    data.walkCallingThread = true;
    if (aThread) {
        HANDLE threadToWalk = reinterpret_cast<HANDLE> (aThread);
        // walkCallingThread indicates whether we are walking the caller's stack
        data.walkCallingThread = (threadToWalk == targetThread);
        targetThread = threadToWalk;
    }

    // Have to duplicate handle to get a real handle.
    if (!::DuplicateHandle(::GetCurrentProcess(),
                           ::GetCurrentProcess(),
                           ::GetCurrentProcess(),
                           &myProcess,
                           PROCESS_ALL_ACCESS, FALSE, 0)) {
        PrintError("DuplicateHandle (process)");
        return NS_ERROR_FAILURE;
    }
    if (!::DuplicateHandle(::GetCurrentProcess(),
                           targetThread,
                           ::GetCurrentProcess(),
                           &myThread,
                           THREAD_ALL_ACCESS, FALSE, 0)) {
        PrintError("DuplicateHandle (thread)");
        ::CloseHandle(myProcess);
        return NS_ERROR_FAILURE;
    }

    data.skipFrames = aSkipFrames;
    data.thread = myThread;
    data.process = myProcess;
    void *local_pcs[1024];
    data.pcs = local_pcs;
    data.pc_count = 0;
    data.pc_size = ArrayLength(local_pcs);

    if (aThread) {
        // If we're walking the stack of another thread, we don't need to
        // use a separate walker thread.
        WalkStackMain64(&data);
    } else {
        data.eventStart = ::CreateEvent(NULL, FALSE /* auto-reset*/,
                              FALSE /* initially non-signaled */, NULL);
        data.eventEnd = ::CreateEvent(NULL, FALSE /* auto-reset*/,
                            FALSE /* initially non-signaled */, NULL);

        ::PostThreadMessage(gStackWalkThread, WM_USER, 0, (LPARAM)&data);

        walkerReturn = ::SignalObjectAndWait(data.eventStart,
                           data.eventEnd, INFINITE, FALSE);
        if (walkerReturn != WAIT_OBJECT_0)
            PrintError("SignalObjectAndWait (1)");
        if (data.pc_count > data.pc_size) {
            data.pcs = (void**) malloc(data.pc_count * sizeof(void*));
            data.pc_size = data.pc_count;
            data.pc_count = 0;
            ::PostThreadMessage(gStackWalkThread, WM_USER, 0, (LPARAM)&data);
            walkerReturn = ::SignalObjectAndWait(data.eventStart,
                               data.eventEnd, INFINITE, FALSE);
            if (walkerReturn != WAIT_OBJECT_0)
                PrintError("SignalObjectAndWait (2)");
        }

        ::CloseHandle(data.eventStart);
        ::CloseHandle(data.eventEnd);
    }

    ::CloseHandle(myThread);
    ::CloseHandle(myProcess);

    for (PRUint32 i = 0; i < data.pc_count; ++i)
        (*aCallback)(data.pcs[i], aClosure);

    if (data.pc_size > ArrayLength(local_pcs))
        free(data.pcs);

    return NS_OK;
}


static BOOL CALLBACK callbackEspecial64(
  PCSTR aModuleName,
  DWORD64 aModuleBase,
  ULONG aModuleSize,
  PVOID aUserContext)
{
    BOOL retval = TRUE;
    DWORD64 addr = *(DWORD64*)aUserContext;

    /*
     * You'll want to control this if we are running on an
     *  architecture where the addresses go the other direction.
     * Not sure this is even a realistic consideration.
     */
    const BOOL addressIncreases = TRUE;

    /*
     * If it falls in side the known range, load the symbols.
     */
    if (addressIncreases
       ? (addr >= aModuleBase && addr <= (aModuleBase + aModuleSize))
       : (addr <= aModuleBase && addr >= (aModuleBase - aModuleSize))
        ) {
        retval = SymLoadModule64(GetCurrentProcess(), NULL, (PSTR)aModuleName, NULL, aModuleBase, aModuleSize);
        if (!retval)
            PrintError("SymLoadModule64");
    }

    return retval;
}

/*
 * SymGetModuleInfoEspecial
 *
 * Attempt to determine the module information.
 * Bug 112196 says this DLL may not have been loaded at the time
 *  SymInitialize was called, and thus the module information
 *  and symbol information is not available.
 * This code rectifies that problem.
 */

// New members were added to IMAGEHLP_MODULE64 (that show up in the
// Platform SDK that ships with VC8, but not the Platform SDK that ships
// with VC7.1, i.e., between DbgHelp 6.0 and 6.1), but we don't need to
// use them, and it's useful to be able to function correctly with the
// older library.  (Stock Windows XP SP2 seems to ship with dbghelp.dll
// version 5.1.)  Since Platform SDK version need not correspond to
// compiler version, and the version number in debughlp.h was NOT bumped
// when these changes were made, ifdef based on a constant that was
// added between these versions.
#ifdef SSRVOPT_SETCONTEXT
#define NS_IMAGEHLP_MODULE64_SIZE (((offsetof(IMAGEHLP_MODULE64, LoadedPdbName) + sizeof(DWORD64) - 1) / sizeof(DWORD64)) * sizeof(DWORD64))
#else
#define NS_IMAGEHLP_MODULE64_SIZE sizeof(IMAGEHLP_MODULE64)
#endif

BOOL SymGetModuleInfoEspecial64(HANDLE aProcess, DWORD64 aAddr, PIMAGEHLP_MODULE64 aModuleInfo, PIMAGEHLP_LINE64 aLineInfo)
{
    BOOL retval = FALSE;

    /*
     * Init the vars if we have em.
     */
    aModuleInfo->SizeOfStruct = NS_IMAGEHLP_MODULE64_SIZE;
    if (nsnull != aLineInfo) {
        aLineInfo->SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    }

    /*
     * Give it a go.
     * It may already be loaded.
     */
    retval = SymGetModuleInfo64(aProcess, aAddr, aModuleInfo);

    if (FALSE == retval) {
        BOOL enumRes = FALSE;

        /*
         * Not loaded, here's the magic.
         * Go through all the modules.
         */
        // Need to cast to PENUMLOADED_MODULES_CALLBACK64 because the
        // constness of the first parameter of
        // PENUMLOADED_MODULES_CALLBACK64 varies over SDK versions (from
        // non-const to const over time).  See bug 391848 and bug
        // 415426.
        enumRes = EnumerateLoadedModules64(aProcess, (PENUMLOADED_MODULES_CALLBACK64)callbackEspecial64, (PVOID)&aAddr);
        if (FALSE != enumRes)
        {
            /*
             * One final go.
             * If it fails, then well, we have other problems.
             */
            retval = SymGetModuleInfo64(aProcess, aAddr, aModuleInfo);
            if (!retval)
                PrintError("SymGetModuleInfo64");
        }
    }

    /*
     * If we got module info, we may attempt line info as well.
     * We will not report failure if this does not work.
     */
    if (FALSE != retval && nsnull != aLineInfo) {
        DWORD displacement = 0;
        BOOL lineRes = FALSE;
        lineRes = SymGetLineFromAddr64(aProcess, aAddr, &displacement, aLineInfo);
        if (!lineRes) {
            // Clear out aLineInfo to indicate that it's not valid
            memset(aLineInfo, 0, sizeof(*aLineInfo));
        }
    }

    return retval;
}

bool
EnsureSymInitialized()
{
    static bool gInitialized = false;
    bool retStat;

    if (gInitialized)
        return gInitialized;

    NS_TIME_FUNCTION;

    if (!EnsureImageHlpInitialized())
        return false;

    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    retStat = SymInitialize(GetCurrentProcess(), NULL, TRUE);
    if (!retStat)
        PrintError("SymInitialize");

    gInitialized = retStat;
    /* XXX At some point we need to arrange to call SymCleanup */

    return retStat;
}


EXPORT_XPCOM_API(nsresult)
NS_DescribeCodeAddress(void *aPC, nsCodeAddressDetails *aDetails)
{
    aDetails->library[0] = '\0';
    aDetails->loffset = 0;
    aDetails->filename[0] = '\0';
    aDetails->lineno = 0;
    aDetails->function[0] = '\0';
    aDetails->foffset = 0;

    if (!EnsureSymInitialized())
        return NS_ERROR_FAILURE;

    HANDLE myProcess = ::GetCurrentProcess();
    BOOL ok;

    // debug routines are not threadsafe, so grab the lock.
    EnterCriticalSection(&gDbgHelpCS);

    //
    // Attempt to load module info before we attempt to resolve the symbol.
    // This just makes sure we get good info if available.
    //

    DWORD64 addr = (DWORD64)aPC;
    IMAGEHLP_MODULE64 modInfo;
    IMAGEHLP_LINE64 lineInfo;
    BOOL modInfoRes;
    modInfoRes = SymGetModuleInfoEspecial64(myProcess, addr, &modInfo, &lineInfo);

    if (modInfoRes) {
        PL_strncpyz(aDetails->library, modInfo.ModuleName,
                    sizeof(aDetails->library));
        aDetails->loffset = (char*) aPC - (char*) modInfo.BaseOfImage;
        
        if (lineInfo.FileName) {
            PL_strncpyz(aDetails->filename, lineInfo.FileName,
                        sizeof(aDetails->filename));
            aDetails->lineno = lineInfo.LineNumber;
        }
    }

    ULONG64 buffer[(sizeof(SYMBOL_INFO) +
      MAX_SYM_NAME*sizeof(TCHAR) + sizeof(ULONG64) - 1) / sizeof(ULONG64)];
    PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;
    pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    pSymbol->MaxNameLen = MAX_SYM_NAME;

    DWORD64 displacement;
    ok = SymFromAddr(myProcess, addr, &displacement, pSymbol);

    if (ok) {
        PL_strncpyz(aDetails->function, pSymbol->Name,
                    sizeof(aDetails->function));
        aDetails->foffset = displacement;
    }

    LeaveCriticalSection(&gDbgHelpCS); // release our lock
    return NS_OK;
}

EXPORT_XPCOM_API(nsresult)
NS_FormatCodeAddressDetails(void *aPC, const nsCodeAddressDetails *aDetails,
                            char *aBuffer, PRUint32 aBufferSize)
{
    if (aDetails->function[0])
        _snprintf(aBuffer, aBufferSize, "%s!%s+0x%016lX",
                  aDetails->library, aDetails->function, aDetails->foffset);
    else
        _snprintf(aBuffer, aBufferSize, "0x%016lX", aPC);

    aBuffer[aBufferSize - 1] = '\0';

    PRUint32 len = strlen(aBuffer);
    if (aDetails->filename[0]) {
        _snprintf(aBuffer + len, aBufferSize - len, " (%s, line %d)\n",
                  aDetails->filename, aDetails->lineno);
    } else {
        aBuffer[len] = '\n';
        if (++len != aBufferSize)
            aBuffer[len] = '\0';
    }
    aBuffer[aBufferSize - 2] = '\n';
    aBuffer[aBufferSize - 1] = '\0';
    return NS_OK;
}

// WIN32 x86 stack walking code
// i386 or PPC Linux stackwalking code or Solaris
#elif HAVE_DLADDR && (HAVE__UNWIND_BACKTRACE || NSSTACKWALK_SUPPORTS_LINUX || NSSTACKWALK_SUPPORTS_SOLARIS || NSSTACKWALK_SUPPORTS_MACOSX)

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "nscore.h"
#include <stdio.h>
#include "plstr.h"

// On glibc 2.1, the Dl_info api defined in <dlfcn.h> is only exposed
// if __USE_GNU is defined.  I suppose its some kind of standards
// adherence thing.
//
#if (__GLIBC_MINOR__ >= 1) && !defined(__USE_GNU)
#define __USE_GNU
#endif

// This thing is exported by libstdc++
// Yes, this is a gcc only hack
#if defined(MOZ_DEMANGLE_SYMBOLS)
#include <cxxabi.h>
#include <stdlib.h> // for free()
#endif // MOZ_DEMANGLE_SYMBOLS

void DemangleSymbol(const char * aSymbol, 
                    char * aBuffer,
                    int aBufLen)
{
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


#if NSSTACKWALK_SUPPORTS_SOLARIS

/*
 * Stack walking code for Solaris courtesy of Bart Smaalder's "memtrak".
 */

#include <synch.h>
#include <ucontext.h>
#include <sys/frame.h>
#include <sys/regset.h>
#include <sys/stack.h>

static int    load_address ( void * pc, void * arg );
static struct bucket * newbucket ( void * pc );
static struct frame * cs_getmyframeptr ( void );
static void   cs_walk_stack ( void * (*read_func)(char * address),
                              struct frame * fp,
                              int (*operate_func)(void *, void *),
                              void * usrarg );
static void   cs_operate ( void (*operate_func)(void *, void *),
                           void * usrarg );

#ifndef STACK_BIAS
#define STACK_BIAS 0
#endif /*STACK_BIAS*/

#define LOGSIZE 4096

/* type of demangling function */
typedef int demf_t(const char *, char *, size_t);

static demf_t *demf;

static int initialized = 0;

#if defined(sparc) || defined(__sparc)
#define FRAME_PTR_REGISTER REG_SP
#endif

#if defined(i386) || defined(__i386)
#define FRAME_PTR_REGISTER EBP
#endif

struct bucket {
    void * pc;
    int index;
    struct bucket * next;
};

struct my_user_args {
    NS_WalkStackCallback callback;
    PRUint32 skipFrames;
    void *closure;
};


static void myinit();

#pragma init (myinit)

static void
myinit()
{

    if (! initialized) {
#ifndef __GNUC__
        void *handle;
        const char *libdem = "libdemangle.so.1";

        /* load libdemangle if we can and need to (only try this once) */
        if ((handle = dlopen(libdem, RTLD_LAZY)) != NULL) {
            demf = (demf_t *)dlsym(handle,
                           "cplus_demangle"); /*lint !e611 */
                /*
                 * lint override above is to prevent lint from
                 * complaining about "suspicious cast".
                 */
        }
#endif /*__GNUC__*/
    }    
    initialized = 1;
}


static int
load_address(void * pc, void * arg )
{
    static struct bucket table[2048];
    static mutex_t lock;
    struct bucket * ptr;
    struct my_user_args * args = (struct my_user_args *) arg;

    unsigned int val = NS_PTR_TO_INT32(pc);

    ptr = table + ((val >> 2)&2047);

    mutex_lock(&lock);
    while (ptr->next) {
        if (ptr->next->pc == pc)
            break;
        ptr = ptr->next;
    }

    if (ptr->next) {
        mutex_unlock(&lock);
    } else {
        (args->callback)(pc, args->closure);

        ptr->next = newbucket(pc);
        mutex_unlock(&lock);
    }
    return 0;
}


static struct bucket *
newbucket(void * pc)
{
    struct bucket * ptr = (struct bucket *) malloc(sizeof (*ptr));
    static int index; /* protected by lock in caller */
                     
    ptr->index = index++;
    ptr->next = NULL;
    ptr->pc = pc;    
    return (ptr);    
}


static struct frame *
csgetframeptr()
{
    ucontext_t u;
    struct frame *fp;

    (void) getcontext(&u);

    fp = (struct frame *)
        ((char *)u.uc_mcontext.gregs[FRAME_PTR_REGISTER] +
        STACK_BIAS);

    /* make sure to return parents frame pointer.... */

    return ((struct frame *)((ulong_t)fp->fr_savfp + STACK_BIAS));
}


static void
cswalkstack(struct frame *fp, int (*operate_func)(void *, void *),
    void *usrarg)
{

    while (fp != 0 && fp->fr_savpc != 0) {

        if (operate_func((void *)fp->fr_savpc, usrarg) != 0)
            break;
        /*
         * watch out - libthread stacks look funny at the top
         * so they may not have their STACK_BIAS set
         */

        fp = (struct frame *)((ulong_t)fp->fr_savfp +
            (fp->fr_savfp?(ulong_t)STACK_BIAS:0));
    }
}


static void
cs_operate(int (*operate_func)(void *, void *), void * usrarg)
{
    cswalkstack(csgetframeptr(), operate_func, usrarg);
}

EXPORT_XPCOM_API(nsresult)
NS_StackWalk(NS_WalkStackCallback aCallback, PRUint32 aSkipFrames,
             void *aClosure, uintptr_t aThread)
{
    MOZ_ASSERT(gCriticalAddress.mInit);
    MOZ_ASSERT(!aThread);
    struct my_user_args args;

    if (!initialized)
        myinit();

    args.callback = aCallback;
    args.skipFrames = aSkipFrames; /* XXX Not handled! */
    args.closure = aClosure;
    cs_operate(load_address, &args);
    return NS_OK;
}

EXPORT_XPCOM_API(nsresult)
NS_DescribeCodeAddress(void *aPC, nsCodeAddressDetails *aDetails)
{
    aDetails->library[0] = '\0';
    aDetails->loffset = 0;
    aDetails->filename[0] = '\0';
    aDetails->lineno = 0;
    aDetails->function[0] = '\0';
    aDetails->foffset = 0;

    char dembuff[4096];
    Dl_info info;

    if (dladdr(aPC, & info)) {
        if (info.dli_fname) {
            PL_strncpyz(aDetails->library, info.dli_fname,
                        sizeof(aDetails->library));
            aDetails->loffset = (char*)aPC - (char*)info.dli_fbase;
        }
        if (info.dli_sname) {
            aDetails->foffset = (char*)aPC - (char*)info.dli_saddr;
#ifdef __GNUC__
            DemangleSymbol(info.dli_sname, dembuff, sizeof(dembuff));
#else
            if (!demf || demf(info.dli_sname, dembuff, sizeof (dembuff)))
                dembuff[0] = 0;
#endif /*__GNUC__*/
            PL_strncpyz(aDetails->function,
                        (dembuff[0] != '\0') ? dembuff : info.dli_sname,
                        sizeof(aDetails->function));
        }
    }

    return NS_OK;
}

EXPORT_XPCOM_API(nsresult)
NS_FormatCodeAddressDetails(void *aPC, const nsCodeAddressDetails *aDetails,
                            char *aBuffer, PRUint32 aBufferSize)
{
    snprintf(aBuffer, aBufferSize, "%p %s:%s+0x%lx\n",
             aPC,
             aDetails->library[0] ? aDetails->library : "??",
             aDetails->function[0] ? aDetails->function : "??",
             aDetails->foffset);
    return NS_OK;
}

#else // not __sun-specific

#if __GLIBC__ > 2 || __GLIBC_MINOR > 1
#define HAVE___LIBC_STACK_END 1
#else
#define HAVE___LIBC_STACK_END 0
#endif

#if HAVE___LIBC_STACK_END
extern void *__libc_stack_end; // from ld-linux.so
#endif
namespace mozilla {
nsresult
FramePointerStackWalk(NS_WalkStackCallback aCallback, PRUint32 aSkipFrames,
                      void *aClosure, void **bp, void *aStackEnd)
{
  // Stack walking code courtesy Kipp's "leaky".

  int skip = aSkipFrames;
  while (1) {
    void **next = (void**)*bp;
    // bp may not be a frame pointer on i386 if code was compiled with
    // -fomit-frame-pointer, so do some sanity checks.
    // (bp should be a frame pointer on ppc(64) but checking anyway may help
    // a little if the stack has been corrupted.)
    // We don't need to check against the begining of the stack because
    // we can assume that bp > sp
    if (next <= bp ||
        next > aStackEnd ||
        (long(next) & 3)) {
      break;
    }
#if (defined(__ppc__) && defined(XP_MACOSX)) || defined(__powerpc64__)
    // ppc mac or powerpc64 linux
    void *pc = *(bp+2);
#else // i386 or powerpc32 linux
    void *pc = *(bp+1);
#endif
    if (IsCriticalAddress(pc)) {
      printf("Aborting stack trace, PC is critical\n");
      return NS_ERROR_UNEXPECTED;
    }
    if (--skip < 0) {
      (*aCallback)(pc, aClosure);
    }
    bp = next;
  }
  return NS_OK;
}

}

#define X86_OR_PPC (defined(__i386) || defined(PPC) || defined(__ppc__))
#if X86_OR_PPC && (NSSTACKWALK_SUPPORTS_MACOSX || NSSTACKWALK_SUPPORTS_LINUX) // i386 or PPC Linux or Mac stackwalking code

EXPORT_XPCOM_API(nsresult)
NS_StackWalk(NS_WalkStackCallback aCallback, PRUint32 aSkipFrames,
             void *aClosure, uintptr_t aThread)
{
  MOZ_ASSERT(gCriticalAddress.mInit);
  MOZ_ASSERT(!aThread);

  // Get the frame pointer
  void **bp;
#if defined(__i386) 
  __asm__( "movl %%ebp, %0" : "=g"(bp));
#else
  // It would be nice if this worked uniformly, but at least on i386 and
  // x86_64, it stopped working with gcc 4.1, because it points to the
  // end of the saved registers instead of the start.
  bp = (void**) __builtin_frame_address(0);
#endif

  void *stackEnd;
#if HAVE___LIBC_STACK_END
  stackEnd = __libc_stack_end;
#else
  stackEnd = reinterpret_cast<void*>(-1);
#endif
  return FramePointerStackWalk(aCallback, aSkipFrames,
                               aClosure, bp, stackEnd);

}

#elif defined(HAVE__UNWIND_BACKTRACE)

// libgcc_s.so symbols _Unwind_Backtrace@@GCC_3.3 and _Unwind_GetIP@@GCC_3.0
#include <unwind.h>

struct unwind_info {
    NS_WalkStackCallback callback;
    int skip;
    void *closure;
};

static _Unwind_Reason_Code
unwind_callback (struct _Unwind_Context *context, void *closure)
{
    unwind_info *info = static_cast<unwind_info *>(closure);
    void *pc = reinterpret_cast<void *>(_Unwind_GetIP(context));
    if (IsCriticalAddress(pc)) {
        printf("Aborting stack trace, PC is critical\n");
        /* We just want to stop the walk, so any error code will do.
           Using _URC_NORMAL_STOP would probably be the most accurate,
           but it is not defined on Android for ARM. */
        return _URC_FOREIGN_EXCEPTION_CAUGHT;
    }
    if (--info->skip < 0)
        (*info->callback)(pc, info->closure);
    return _URC_NO_REASON;
}

EXPORT_XPCOM_API(nsresult)
NS_StackWalk(NS_WalkStackCallback aCallback, PRUint32 aSkipFrames,
             void *aClosure, uintptr_t aThread)
{
    MOZ_ASSERT(gCriticalAddress.mInit);
    MOZ_ASSERT(!aThread);
    unwind_info info;
    info.callback = aCallback;
    info.skip = aSkipFrames + 1;
    info.closure = aClosure;

    _Unwind_Reason_Code t = _Unwind_Backtrace(unwind_callback, &info);
    if (t != _URC_END_OF_STACK)
        return NS_ERROR_UNEXPECTED;
    return NS_OK;
}

#endif

EXPORT_XPCOM_API(nsresult)
NS_DescribeCodeAddress(void *aPC, nsCodeAddressDetails *aDetails)
{
  aDetails->library[0] = '\0';
  aDetails->loffset = 0;
  aDetails->filename[0] = '\0';
  aDetails->lineno = 0;
  aDetails->function[0] = '\0';
  aDetails->foffset = 0;

  Dl_info info;
  int ok = dladdr(aPC, &info);
  if (!ok) {
    return NS_OK;
  }

  PL_strncpyz(aDetails->library, info.dli_fname, sizeof(aDetails->library));
  aDetails->loffset = (char*)aPC - (char*)info.dli_fbase;

  const char * symbol = info.dli_sname;
  int len;
  if (!symbol || !(len = strlen(symbol))) {
    return NS_OK;
  }

  char demangled[4096] = "\0";

  DemangleSymbol(symbol, demangled, sizeof(demangled));

  if (strlen(demangled)) {
    symbol = demangled;
    len = strlen(symbol);
  }

  PL_strncpyz(aDetails->function, symbol, sizeof(aDetails->function));
  aDetails->foffset = (char*)aPC - (char*)info.dli_saddr;
  return NS_OK;
}

EXPORT_XPCOM_API(nsresult)
NS_FormatCodeAddressDetails(void *aPC, const nsCodeAddressDetails *aDetails,
                            char *aBuffer, PRUint32 aBufferSize)
{
  if (!aDetails->library[0]) {
    snprintf(aBuffer, aBufferSize, "UNKNOWN %p\n", aPC);
  } else if (!aDetails->function[0]) {
    snprintf(aBuffer, aBufferSize, "UNKNOWN [%s +0x%08lX]\n",
                                   aDetails->library, aDetails->loffset);
  } else {
    snprintf(aBuffer, aBufferSize, "%s+0x%08lX [%s +0x%08lX]\n",
                                   aDetails->function, aDetails->foffset,
                                   aDetails->library, aDetails->loffset);
  }
  return NS_OK;
}

#endif

#else // unsupported platform.

EXPORT_XPCOM_API(nsresult)
NS_StackWalk(NS_WalkStackCallback aCallback, PRUint32 aSkipFrames,
             void *aClosure, uintptr_t aThread)
{
    MOZ_ASSERT(gCriticalAddress.mInit);
    MOZ_ASSERT(!aThread);
    return NS_ERROR_NOT_IMPLEMENTED;
}

namespace mozilla {
nsresult
FramePointerStackWalk(NS_WalkStackCallback aCallback, PRUint32 aSkipFrames,
                      void *aClosure, void **bp)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
}

EXPORT_XPCOM_API(nsresult)
NS_DescribeCodeAddress(void *aPC, nsCodeAddressDetails *aDetails)
{
    aDetails->library[0] = '\0';
    aDetails->loffset = 0;
    aDetails->filename[0] = '\0';
    aDetails->lineno = 0;
    aDetails->function[0] = '\0';
    aDetails->foffset = 0;
    return NS_ERROR_NOT_IMPLEMENTED;
}

EXPORT_XPCOM_API(nsresult)
NS_FormatCodeAddressDetails(void *aPC, const nsCodeAddressDetails *aDetails,
                            char *aBuffer, PRUint32 aBufferSize)
{
    aBuffer[0] = '\0';
    return NS_ERROR_NOT_IMPLEMENTED;
}

#endif
