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
#include "prprf.h"
#include "prlog.h"

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

static void InitTraceLog(void)
{
  if (0 == gTraceRefcntLog) {
    gTraceRefcntLog = PR_NewLogModule("xpcomrefcnt");

#if defined(NS_MT_SUPPORTED)
    gTraceLock = PR_NewLock();
#endif /* NS_MT_SUPPORTED */
  }
}


int nsIToA16(PRUint32 aNumber, char* aBuffer)
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

      int len = strlen(info.dli_sname);
      if (! len)
        break; // XXX Lazy. We could look at the filename or something.

      if (len + 12 >= aBufLen) // 12 == strlen("+0x12345678 ")
        break;

      strcpy(cp, info.dli_sname);
      cp += len;

      *cp++ = '+';
      *cp++ = '0';
      *cp++ = 'x';

      PRUint32 off = (char*)pc - (char*)info.dli_saddr;
      PRInt32 addrStrLen = nsIToA16(off, cp);
      cp += addrStrLen;

      *cp++ = ' ';

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

NS_COM void
nsTraceRefcnt::LoadLibrarySymbols(const char* aLibraryName,
                                  void* aLibrayHandle)
{
#ifdef MOZ_TRACE_XPCOM_REFCNT
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

NS_COM unsigned long
nsTraceRefcnt::AddRef(void* aPtr,
                      unsigned long aNewRefcnt,
                      const char* aFile,
                      PRIntn aLine)
{
#ifdef MOZ_TRACE_XPCOM_REFCNT
  InitTraceLog();

  LOCK_TRACELOG();
  if (PR_LOG_TEST(gTraceRefcntLog,PR_LOG_DEBUG)) {
    char sb[1000];
    WalkTheStack(sb, sizeof(sb));
    PR_LOG(gTraceRefcntLog, PR_LOG_DEBUG,
           ("AddRef: %p: %d=>%d [%s] in %s (line %d)",
            aPtr, aNewRefcnt-1, aNewRefcnt, sb, aFile, aLine));
  }
  UNLOCK_TRACELOG();
#endif
  return aNewRefcnt;

}

NS_COM unsigned long
nsTraceRefcnt::Release(void* aPtr,
                       unsigned long aNewRefcnt,
                       const char* aFile,
                       PRIntn aLine)
{
#ifdef MOZ_TRACE_XPCOM_REFCNT
  InitTraceLog();

  LOCK_TRACELOG();
  if (PR_LOG_TEST(gTraceRefcntLog,PR_LOG_DEBUG)) {
    char sb[1000];
    WalkTheStack(sb, sizeof(sb));
    PR_LOG(gTraceRefcntLog, PR_LOG_DEBUG,
           ("Release: %p: %d=>%d [%s] in %s (line %d)",
            aPtr, aNewRefcnt+1, aNewRefcnt, sb, aFile, aLine));
  }
  UNLOCK_TRACELOG();
#endif
  return aNewRefcnt;
}

NS_COM void
nsTraceRefcnt::Create(void* aPtr,
                      const char* aType,
                      const char* aFile,
                      PRIntn aLine)
{
#ifdef MOZ_TRACE_XPCOM_REFCNT
  InitTraceLog();

  LOCK_TRACELOG();
  if (PR_LOG_TEST(gTraceRefcntLog,PR_LOG_DEBUG)) {
    char sb[1000];
    WalkTheStack(sb, sizeof(sb));
    PR_LOG(gTraceRefcntLog, PR_LOG_DEBUG,
           ("Create: %p[%s]: [%s] in %s (line %d)",
            aPtr, aType, sb, aFile, aLine));
  }
  UNLOCK_TRACELOG();
#endif
}

NS_COM void
nsTraceRefcnt::Destroy(void* aPtr,
                      const char* aFile,
                      PRIntn aLine)
{
#ifdef MOZ_TRACE_XPCOM_REFCNT
  InitTraceLog();

  LOCK_TRACELOG();
  if (PR_LOG_TEST(gTraceRefcntLog,PR_LOG_DEBUG)) {
    char sb[1000];
    WalkTheStack(sb, sizeof(sb));
    PR_LOG(gTraceRefcntLog, PR_LOG_DEBUG,
           ("Destroy: %p: [%s] in %s (line %d)",
            aPtr, sb, aFile, aLine));
  }
  UNLOCK_TRACELOG();
#endif
}


NS_COM void
nsTraceRefcnt::LogAddRef(void* aPtr,
                         nsrefcnt aRefCnt,
                         const char* aFile,
                         int aLine)
{
  InitTraceLog();
  if (PR_LOG_TEST(gTraceRefcntLog, PR_LOG_DEBUG)) {
    char sb[16384];
    WalkTheStack(sb, sizeof(sb));
    // Can't use PR_LOG(), b/c it truncates the line
    printf("%s(%d) %p AddRef %d %s\n", aFile, aLine, aPtr, aRefCnt, sb);
  }
}


NS_COM void
nsTraceRefcnt::LogRelease(void* aPtr,
                         nsrefcnt aRefCnt,
                         const char* aFile,
                         int aLine)
{
  InitTraceLog();
  if (PR_LOG_TEST(gTraceRefcntLog, PR_LOG_DEBUG)) {
    char sb[16384];
    WalkTheStack(sb, sizeof(sb));
    // Can't use PR_LOG(), b/c it truncates the line
    printf("%s(%d) %p Release %d %s\n", aFile, aLine, aPtr, aRefCnt, sb);
  }
}
