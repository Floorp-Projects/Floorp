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

#if defined(MOZ_TRACE_XPCOM_REFCNT)

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

#if defined(_WIN32)
#include "imagehlp.h"
#include <stdio.h>

#if 0
static BOOL __stdcall
EnumSymbolsCB(LPSTR aSymbolName,
              ULONG aSymbolAddress,
              ULONG aSymbolSize,
              PVOID aUserContext)
{
  int* countp = (int*) aUserContext;
  int count = *countp;
  if (count < 3) {
    printf("  %p[%4d]: %s\n", aSymbolAddress, aSymbolSize, aSymbolName);
    count++;
    *countp = count++;
  }
  return TRUE;
}

static BOOL __stdcall
EnumModulesCB(LPSTR aModuleName,
              ULONG aBaseOfDll,
              PVOID aUserContext)
{
  HANDLE myProcess = ::GetCurrentProcess();

  printf("module=%s dll=%x\n", aModuleName, aBaseOfDll);
//  int count = 0;
//  SymEnumerateSymbols(myProcess, aBaseOfDll, EnumSymbolsCB, (void*) &count);
  return TRUE;
}
#endif

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
  CONTEXT context;
  STACKFRAME frame;
  char* cp = aBuffer;

  aBuffer[0] = '\0';
  aBufLen--;                    // leave room for nul

  HANDLE myProcess = ::GetCurrentProcess();
  HANDLE myThread = ::GetCurrentThread();

  // Get the context information for this thread. That way we will
  // know where our sp, fp, pc, etc. are and can fill in the
  // STACKFRAME with the initial values.
  context.ContextFlags = CONTEXT_FULL;
  GetThreadContext(myThread, &context);

  if (!SymInitialize(myProcess, ".;..\\lib", TRUE)) {
    return;
  }

#if 0
  SymEnumerateModules(myProcess, EnumModulesCB, NULL);
#endif

  // Setup initial stack frame to walk from
  memset(&frame, 0, sizeof(frame));
  frame.AddrPC.Mode = AddrModeFlat;
  frame.AddrPC.Offset = context.Eip;
  frame.AddrReturn.Mode = AddrModeFlat;
  frame.AddrReturn.Offset = context.Ebp;
  frame.AddrFrame.Mode = AddrModeFlat;
  frame.AddrFrame.Offset = context.Ebp;
  frame.AddrStack.Mode = AddrModeFlat;
  frame.AddrStack.Offset = context.Esp;
  frame.Params[0] = context.Eax;
  frame.Params[1] = context.Ecx;
  frame.Params[2] = context.Edx;
  frame.Params[3] = context.Ebx;

  // Now walk the stack and map the pc's to symbol names that we stuff
  // append to *cp.

  int skip = 2;
  int syms = 0;
  while (aBufLen > 0) {
    char symbolBuffer[sizeof(IMAGEHLP_SYMBOL) + 512];
    PIMAGEHLP_SYMBOL pSymbol = (PIMAGEHLP_SYMBOL) symbolBuffer;
    pSymbol->SizeOfStruct = sizeof(symbolBuffer);
    pSymbol->MaxNameLength = 512;

    DWORD oldAddress = frame.AddrPC.Offset;
    BOOL b = ::StackWalk(IMAGE_FILE_MACHINE_I386, myProcess, myThread,
                         &frame, &context, NULL,
                         SymFunctionTableAccess,
                         SymGetModuleBase,
                         NULL);
    if (!b || (0 == frame.AddrPC.Offset)) {
      if (syms <= 1) {
        skip = 7;
      }
      break;
    }
    if (--skip >= 0) {
      continue;
    }
    DWORD disp;
    if (SymGetSymFromAddr(myProcess, frame.AddrPC.Offset, &disp, pSymbol)) {
      int nameLen = strlen(pSymbol->Name);
      if (nameLen + 2 > aBufLen) {
        break;
      }
      memcpy(cp, pSymbol->Name, nameLen);
      cp += nameLen;
      *cp++ = ' ';
      aBufLen -= nameLen + 1;
      syms++;
    }
    else {
      if (11 > aBufLen) {
        break;
      }
      char tmp[30];
      PR_snprintf(tmp, sizeof(tmp), "0x%08x ", frame.AddrPC.Offset);
      memcpy(cp, tmp, 11);
      cp += 11;
      aBufLen -= 11;
      syms++;
    }
  }
  *cp = 0;
}

#endif /* _WIN32 */

#else /* MOZ_TRACE_XPCOM_REFCNT */
void
nsTraceRefcnt::WalkTheStack(char* aBuffer, int aBufLen)
{
  aBuffer[0] = '\0';
}
#endif /* MOZ_TRACE_XPCOM_REFCNT */

NS_COM void
nsTraceRefcnt::LoadLibrarySymbols(const char* aLibraryName,
                                  void* aLibrayHandle)
{
#ifdef MOZ_TRACE_XPCOM_REFCNT
#if defined(_WIN32)
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
