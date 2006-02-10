/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is nsStackFrameWin.h code, released
 * December 20, 2003.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Judge, 20-December-2000
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

#include "nscore.h"
#include "windows.h"
#include "stdio.h"
#include "nsStackFrameWin.h"

// Define these as static pointers so that we can load the DLL on the
// fly (and not introduce a link-time dependency on it). Tip o' the
// hat to Matt Pietrick for this idea. See:
//
//   http://msdn.microsoft.com/library/periodic/period97/F1/D3/S245C6.htm
//


PR_BEGIN_EXTERN_C

SYMSETOPTIONSPROC _SymSetOptions;

SYMINITIALIZEPROC _SymInitialize;

SYMCLEANUPPROC _SymCleanup;

STACKWALKPROC _StackWalk;
#ifdef USING_WXP_VERSION
STACKWALKPROC64 _StackWalk64;
#else
#define _StackWalk64 0
#endif

SYMFUNCTIONTABLEACCESSPROC _SymFunctionTableAccess;
#ifdef USING_WXP_VERSION
SYMFUNCTIONTABLEACCESSPROC64 _SymFunctionTableAccess64;
#else
#define _SymFunctionTableAccess64 0
#endif

SYMGETMODULEBASEPROC _SymGetModuleBase;
#ifdef USING_WXP_VERSION
SYMGETMODULEBASEPROC64 _SymGetModuleBase64;
#else
#define _SymGetModuleBase64 0
#endif

SYMGETSYMFROMADDRPROC _SymGetSymFromAddr;
#ifdef USING_WXP_VERSION
SYMFROMADDRPROC _SymFromAddr;
#else
#define _SymFromAddr 0
#endif

SYMLOADMODULE _SymLoadModule;
#ifdef USING_WXP_VERSION
SYMLOADMODULE64 _SymLoadModule64;
#else
#define _SymLoadModule64 0
#endif

SYMUNDNAME _SymUnDName;

SYMGETMODULEINFO _SymGetModuleInfo;
#ifdef USING_WXP_VERSION
SYMGETMODULEINFO64 _SymGetModuleInfo64;
#else
#define _SymGetModuleInfo64 0
#endif

ENUMLOADEDMODULES _EnumerateLoadedModules;
#ifdef USING_WXP_VERSION
ENUMLOADEDMODULES64 _EnumerateLoadedModules64;
#else
#define _EnumerateLoadedModules64 0
#endif

SYMGETLINEFROMADDRPROC _SymGetLineFromAddr;
#ifdef USING_WXP_VERSION
SYMGETLINEFROMADDRPROC64 _SymGetLineFromAddr64;
#else
#define _SymGetLineFromAddr64 0
#endif

HANDLE hStackWalkMutex;

PR_END_EXTERN_C

// Routine to print an error message to standard error.
// Will also print to an additional stream if one is supplied.
void PrintError(char *prefix, FILE *out)
{
    LPVOID lpMsgBuf;
    DWORD lastErr = GetLastError();
    FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,
      lastErr,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
      (LPTSTR) &lpMsgBuf,
      0,
      NULL
    );
    fprintf(stderr, "### ERROR: %s: %s", prefix, lpMsgBuf);
    if (out)
        fprintf(out, "### ERROR: %s: %s\n", prefix, lpMsgBuf);
    LocalFree( lpMsgBuf );
}

PRBool
EnsureImageHlpInitialized()
{
    static PRBool gInitialized = PR_FALSE;

    if (gInitialized)
        return gInitialized;

    // Create a mutex with no initial owner.
    hStackWalkMutex = CreateMutex(
      NULL,                       // default security attributes
      FALSE,                      // initially not owned
      NULL);                      // unnamed mutex

    if (hStackWalkMutex == NULL) {
        PrintError("CreateMutex", NULL);
        return PR_FALSE;
    }

    HMODULE module = ::LoadLibrary("DBGHELP.DLL");
    if (!module) {
        module = ::LoadLibrary("IMAGEHLP.DLL");
        if (!module) return PR_FALSE;
    }

    _SymSetOptions = (SYMSETOPTIONSPROC) ::GetProcAddress(module, "SymSetOptions");
    if (!_SymSetOptions) return PR_FALSE;

    _SymInitialize = (SYMINITIALIZEPROC) ::GetProcAddress(module, "SymInitialize");
    if (!_SymInitialize) return PR_FALSE;

    _SymCleanup = (SYMCLEANUPPROC)GetProcAddress(module, "SymCleanup");
    if (!_SymCleanup) return PR_FALSE;

#ifdef USING_WXP_VERSION
    _StackWalk64 = (STACKWALKPROC64)GetProcAddress(module, "StackWalk64");
#endif
    _StackWalk = (STACKWALKPROC)GetProcAddress(module, "StackWalk");
    if (!_StackWalk64  && !_StackWalk) return PR_FALSE;

#ifdef USING_WXP_VERSION
    _SymFunctionTableAccess64 = (SYMFUNCTIONTABLEACCESSPROC64) GetProcAddress(module, "SymFunctionTableAccess64");
#endif
    _SymFunctionTableAccess = (SYMFUNCTIONTABLEACCESSPROC) GetProcAddress(module, "SymFunctionTableAccess");
    if (!_SymFunctionTableAccess64 && !_SymFunctionTableAccess) return PR_FALSE;

#ifdef USING_WXP_VERSION
    _SymGetModuleBase64 = (SYMGETMODULEBASEPROC64)GetProcAddress(module, "SymGetModuleBase64");
#endif
    _SymGetModuleBase = (SYMGETMODULEBASEPROC)GetProcAddress(module, "SymGetModuleBase");
    if (!_SymGetModuleBase64 && !_SymGetModuleBase) return PR_FALSE;

    _SymGetSymFromAddr = (SYMGETSYMFROMADDRPROC)GetProcAddress(module, "SymGetSymFromAddr");
#ifdef USING_WXP_VERSION
    _SymFromAddr = (SYMFROMADDRPROC)GetProcAddress(module, "SymFromAddr");
#endif
    if (!_SymFromAddr && !_SymGetSymFromAddr) return PR_FALSE;

#ifdef USING_WXP_VERSION
    _SymLoadModule64 = (SYMLOADMODULE64)GetProcAddress(module, "SymLoadModule64");
#endif
    _SymLoadModule = (SYMLOADMODULE)GetProcAddress(module, "SymLoadModule");
    if (!_SymLoadModule64 && !_SymLoadModule) return PR_FALSE;

    _SymUnDName = (SYMUNDNAME)GetProcAddress(module, "SymUnDName");
    if (!_SymUnDName) return PR_FALSE;

#ifdef USING_WXP_VERSION
    _SymGetModuleInfo64 = (SYMGETMODULEINFO64)GetProcAddress(module, "SymGetModuleInfo64");
#endif
    _SymGetModuleInfo = (SYMGETMODULEINFO)GetProcAddress(module, "SymGetModuleInfo");
    if (!_SymGetModuleInfo64 && !_SymGetModuleInfo) return PR_FALSE;

#ifdef USING_WXP_VERSION
    _EnumerateLoadedModules64 = (ENUMLOADEDMODULES64)GetProcAddress(module, "EnumerateLoadedModules64");
#endif
    _EnumerateLoadedModules = (ENUMLOADEDMODULES)GetProcAddress(module, "EnumerateLoadedModules");
    if (!_EnumerateLoadedModules64 && !_EnumerateLoadedModules) return PR_FALSE;

#ifdef USING_WXP_VERSION
    _SymGetLineFromAddr64 = (SYMGETLINEFROMADDRPROC64)GetProcAddress(module, "SymGetLineFromAddr64");
#endif
    _SymGetLineFromAddr = (SYMGETLINEFROMADDRPROC)GetProcAddress(module, "SymGetLineFromAddr");
    if (!_SymGetLineFromAddr64 && !_SymGetLineFromAddr) return PR_FALSE;

    return gInitialized = PR_TRUE;
}


static BOOL CALLBACK callbackEspecial(
  LPSTR aModuleName,
  DWORD aModuleBase,
  ULONG aModuleSize,
  PVOID aUserContext)
{
    BOOL retval = TRUE;
    DWORD addr = *(DWORD*)aUserContext;

    /*
     * You'll want to control this if we are running on an
     *  architecture where the addresses go the other direction.
     * Not sure this is even a realistic consideration.
     */
    const BOOL addressIncreases = TRUE;

    /*
     * If it falls inside the known range, load the symbols.
     */
    if (addressIncreases
       ? (addr >= aModuleBase && addr <= (aModuleBase + aModuleSize))
       : (addr <= aModuleBase && addr >= (aModuleBase - aModuleSize))
        ) {
        retval = _SymLoadModule(GetCurrentProcess(), NULL, aModuleName, NULL, aModuleBase, aModuleSize);
    }

    return retval;
}

static BOOL CALLBACK callbackEspecial64(
  PTSTR aModuleName,
  DWORD64 aModuleBase,
  ULONG aModuleSize,
  PVOID aUserContext)
{
#ifdef USING_WXP_VERSION
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
        retval = _SymLoadModule64(GetCurrentProcess(), NULL, aModuleName, NULL, aModuleBase, aModuleSize);
    }

    return retval;
#else
    return FALSE;
#endif
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
BOOL SymGetModuleInfoEspecial(HANDLE aProcess, DWORD aAddr, PIMAGEHLP_MODULE aModuleInfo, PIMAGEHLP_LINE aLineInfo)
{
    BOOL retval = FALSE;

    /*
     * Init the vars if we have em.
     */
    aModuleInfo->SizeOfStruct = sizeof(IMAGEHLP_MODULE);
    if (nsnull != aLineInfo) {
      aLineInfo->SizeOfStruct = sizeof(IMAGEHLP_LINE);
    }

    /*
     * Give it a go.
     * It may already be loaded.
     */
    retval = _SymGetModuleInfo(aProcess, aAddr, aModuleInfo);

    if (FALSE == retval) {
        BOOL enumRes = FALSE;

        /*
         * Not loaded, here's the magic.
         * Go through all the modules.
         */
        enumRes = _EnumerateLoadedModules(aProcess, callbackEspecial, (PVOID)&aAddr);
        if (FALSE != enumRes)
        {
            /*
             * One final go.
             * If it fails, then well, we have other problems.
             */
            retval = _SymGetModuleInfo(aProcess, aAddr, aModuleInfo);
        }
    }

    /*
     * If we got module info, we may attempt line info as well.
     * We will not report failure if this does not work.
     */
    if (FALSE != retval && nsnull != aLineInfo && nsnull != _SymGetLineFromAddr) {
        DWORD displacement = 0;
        BOOL lineRes = FALSE;
        lineRes = _SymGetLineFromAddr(aProcess, aAddr, &displacement, aLineInfo);
    }

    return retval;
}

#ifdef USING_WXP_VERSION
BOOL SymGetModuleInfoEspecial64(HANDLE aProcess, DWORD64 aAddr, PIMAGEHLP_MODULE64 aModuleInfo, PIMAGEHLP_LINE64 aLineInfo)
{
    BOOL retval = FALSE;

    /*
     * Init the vars if we have em.
     */
    aModuleInfo->SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
    if (nsnull != aLineInfo) {
        aLineInfo->SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    }

    /*
     * Give it a go.
     * It may already be loaded.
     */
    retval = _SymGetModuleInfo64(aProcess, aAddr, aModuleInfo);

    if (FALSE == retval) {
        BOOL enumRes = FALSE;

        /*
         * Not loaded, here's the magic.
         * Go through all the modules.
         */
        enumRes = _EnumerateLoadedModules64(aProcess, callbackEspecial64, (PVOID)&aAddr);
        if (FALSE != enumRes)
        {
            /*
             * One final go.
             * If it fails, then well, we have other problems.
             */
            retval = _SymGetModuleInfo64(aProcess, aAddr, aModuleInfo);
        }
    }

    /*
     * If we got module info, we may attempt line info as well.
     * We will not report failure if this does not work.
     */
    if (FALSE != retval && nsnull != aLineInfo && nsnull != _SymGetLineFromAddr64) {
        DWORD displacement = 0;
        BOOL lineRes = FALSE;
        lineRes = _SymGetLineFromAddr64(aProcess, aAddr, &displacement, aLineInfo);
    }

    return retval;
}
#endif

HANDLE
GetCurrentPIDorHandle()
{
    if (_SymGetModuleBase64)
        return GetCurrentProcess();  // winxp and friends use process handle

    return (HANDLE) GetCurrentProcessId(); // winme win98 win95 etc use process identifier
}

PRBool
EnsureSymInitialized()
{
    static PRBool gInitialized = PR_FALSE;
    PRBool retStat;

    if (gInitialized)
        return gInitialized;

    if (!EnsureImageHlpInitialized())
        return PR_FALSE;

    _SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    retStat = _SymInitialize(GetCurrentPIDorHandle(), NULL, TRUE);
    if (!retStat)
        PrintError("SymInitialize", NULL);

    gInitialized = retStat;
    /* XXX At some point we need to arrange to call _SymCleanup */

    return retStat;
}


/**
 * Walk the stack, translating PC's found into strings and recording the
 * chain in aBuffer. For this to work properly, the DLLs must be rebased
 * so that the address in the file agrees with the address in memory.
 * Otherwise StackWalk will return FALSE when it hits a frame in a DLL
 * whose in memory address doesn't match its in-file address.
 */

void
DumpStackToFile(FILE* aStream)
{
    HANDLE myProcess = ::GetCurrentProcess();
    HANDLE myThread, walkerThread;
    DWORD walkerReturn;
    struct DumpStackToFileData data;

    if (!EnsureSymInitialized())
        return;

    // Have to duplicate handle to get a real handle.
    ::DuplicateHandle(
      ::GetCurrentProcess(),
      ::GetCurrentThread(),
      ::GetCurrentProcess(),
      &myThread,
      THREAD_ALL_ACCESS, FALSE, 0
    );

    data.stream = aStream;
    data.thread = myThread;
    data.process = myProcess;
    walkerThread = ::CreateThread( NULL, 0, DumpStackToFileThread, (LPVOID) &data, 0, NULL ) ;
    if (walkerThread) {
        walkerReturn = ::WaitForSingleObject(walkerThread, 2000); // no timeout is never a good idea
        if (walkerReturn != WAIT_OBJECT_0) {
            PrintError("ThreadWait", aStream);
        }
        CloseHandle(myThread);
    }
    else {
        PrintError("ThreadCreate", aStream);
    }
    return;
}

DWORD WINAPI
DumpStackToFileThread(LPVOID lpdata)
{
    struct DumpStackToFileData *data = (DumpStackToFileData *)lpdata;
    DWORD ret ;

    // Suspend the calling thread, dump his stack, and then resume him.
    // He's currently waiting for us to finish so now should be a good time.
    ret = ::SuspendThread( data->thread );
    if (ret == -1) {
        PrintError("ThreadSuspend", data->stream);
    }
    else {
        if (_StackWalk64)
            DumpStackToFileMain64(data);
        else
            DumpStackToFileMain(data);
        ret = ::ResumeThread(data->thread);
        if (ret == -1) {
            PrintError("ThreadResume", data->stream);
        }
    }

    return 0;
}

void
DumpStackToFileMain64(struct DumpStackToFileData* data)
{
#ifdef USING_WXP_VERSION
    // Get the context information for the thread. That way we will
    // know where our sp, fp, pc, etc. are and can fill in the
    // STACKFRAME64 with the initial values.
    CONTEXT context;
    HANDLE myProcess = data->process;
    HANDLE myThread = data->thread;
    FILE* aStream = data->stream;
    DWORD64 addr;
    STACKFRAME64 frame64;
    int skip = 6; // skip our own stack walking frames
    BOOL ok;

    // Get a context for the specified thread.
    memset(&context, 0, sizeof(CONTEXT));
    context.ContextFlags = CONTEXT_FULL;
    if (!GetThreadContext(myThread, &context)) {
        PrintError("GetThreadContext", aStream);
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
    fprintf(aStream, "Unknown platform. No stack walking.");
    return;
#endif
    frame64.AddrPC.Mode      = AddrModeFlat;
    frame64.AddrStack.Mode   = AddrModeFlat;
    frame64.AddrFrame.Mode   = AddrModeFlat;
    frame64.AddrReturn.Mode  = AddrModeFlat;

    // Now walk the stack and map the pc's to symbol names
    while (1) {

        ok = 0;

        // stackwalk is not threadsafe, so grab the lock.
        DWORD dwWaitResult;
        dwWaitResult = WaitForSingleObject(hStackWalkMutex, INFINITE);
        if (dwWaitResult == WAIT_OBJECT_0) {

            ok = _StackWalk64(
#ifdef _M_AMD64
              IMAGE_FILE_MACHINE_AMD64,
#elif defined _M_IA64
              IMAGE_FILE_MACHINE_IA64,
#elif defined _M_IX86
              IMAGE_FILE_MACHINE_I386,
#else
              0,
#endif
              myProcess,
              myThread,
              &frame64,
              &context,
              NULL,
              _SymFunctionTableAccess64, // function table access routine
              _SymGetModuleBase64,       // module base routine
              0
            );

            if (ok)
                addr = frame64.AddrPC.Offset;
            else
                PrintError("WalkStack64", aStream);

            if (!ok || (addr == 0)) {
                ReleaseMutex(hStackWalkMutex);  // release our lock
                break;
            }

            if (skip-- > 0) {
                ReleaseMutex(hStackWalkMutex);  // release our lock
                continue;
            }

            //
            // Attempt to load module info before we attempt to reolve the symbol.
            // This just makes sure we get good info if available.
            //

            IMAGEHLP_MODULE64 modInfo;
            modInfo.SizeOfStruct = sizeof(modInfo);
            BOOL modInfoRes;
            modInfoRes = SymGetModuleInfoEspecial64(myProcess, addr, &modInfo, nsnull);

            ULONG64 buffer[(sizeof(SYMBOL_INFO) +
              MAX_SYM_NAME*sizeof(TCHAR) + sizeof(ULONG64) - 1) / sizeof(ULONG64)];
            PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;
            pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            pSymbol->MaxNameLen = MAX_SYM_NAME;

            DWORD64 displacement;
            ok = _SymFromAddr && _SymFromAddr(myProcess, addr, &displacement, pSymbol);

            // All done with debug calls so release our lock.
            ReleaseMutex(hStackWalkMutex);

            if (ok)
                fprintf(aStream, "%s!%s+0x%016X\n", modInfo.ModuleName, pSymbol->Name, displacement);
            else
                fprintf(aStream, "0x%016X\n", addr);

            // Stop walking when we get to kernel32.
            if (strcmp(modInfo.ModuleName, "kernel32") == 0)
                break;
        }
        else {
            PrintError("LockError64", aStream);
        } 
    }
    return;
#endif
}


void
DumpStackToFileMain(struct DumpStackToFileData* data)
{
    // Get the context information for the thread. That way we will
    // know where our sp, fp, pc, etc. are and can fill in the
    // STACKFRAME with the initial values.
    CONTEXT context;
    HANDLE myProcess = data->process;
    HANDLE myThread = data->thread;
    FILE* aStream = data->stream;
    DWORD addr;
    STACKFRAME frame;
    int skip = 2;  // skip our own stack walking frames
    BOOL ok;

    // Get a context for the specified thread.
    memset(&context, 0, sizeof(CONTEXT));
    context.ContextFlags = CONTEXT_FULL;
    if (!GetThreadContext(myThread, &context)) {
        PrintError("GetThreadContext", aStream);
        return;
    }

    // Setup initial stack frame to walk from
#if defined _M_IX86
    memset(&frame, 0, sizeof(frame));
    frame.AddrPC.Offset    = context.Eip;
    frame.AddrPC.Mode      = AddrModeFlat;
    frame.AddrStack.Offset = context.Esp;
    frame.AddrStack.Mode   = AddrModeFlat;
    frame.AddrFrame.Offset = context.Ebp;
    frame.AddrFrame.Mode   = AddrModeFlat;
#else
    fprintf(aStream, "Unknown platform. No stack walking.");
    return;
#endif

    // Now walk the stack and map the pc's to symbol names
    while (1) {

        ok = 0;

        // debug routines are not threadsafe, so grab the lock.
        DWORD dwWaitResult;
        dwWaitResult = WaitForSingleObject(hStackWalkMutex, INFINITE);
        if (dwWaitResult == WAIT_OBJECT_0) {

            ok = _StackWalk(
                IMAGE_FILE_MACHINE_I386,
                myProcess,
                myThread,
                &frame,
                &context,
                0,                        // read process memory routine
                _SymFunctionTableAccess,  // function table access routine
                _SymGetModuleBase,        // module base routine
                0                         // translate address routine
              );

            if (ok)
                addr = frame.AddrPC.Offset;
            else
                PrintError("WalkStack", aStream);

            if (!ok || (addr == 0)) {
                ReleaseMutex(hStackWalkMutex);  // release our lock
                break;
            }

            if (skip-- > 0) {
                ReleaseMutex(hStackWalkMutex);  // release the lock
                continue;
            }

            //
            // Attempt to load module info before we attempt to resolve the symbol.
            // This just makes sure we get good info if available.
            //

            IMAGEHLP_MODULE modInfo;
            modInfo.SizeOfStruct = sizeof(modInfo);
            BOOL modInfoRes;
            modInfoRes = SymGetModuleInfoEspecial(myProcess, addr, &modInfo, nsnull);

#ifdef USING_WXP_VERSION
            ULONG64 buffer[(sizeof(SYMBOL_INFO) +
              MAX_SYM_NAME*sizeof(TCHAR) + sizeof(ULONG64) - 1) / sizeof(ULONG64)];
            PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;
            pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            pSymbol->MaxNameLen = MAX_SYM_NAME;

            DWORD64 displacement;

            ok = _SymFromAddr && _SymFromAddr(myProcess, addr, &displacement, pSymbol);
#else
            char buf[sizeof(IMAGEHLP_SYMBOL) + 512];
            PIMAGEHLP_SYMBOL pSymbol = (PIMAGEHLP_SYMBOL) buf;
            pSymbol->SizeOfStruct = sizeof(buf);
            pSymbol->MaxNameLength = 512;

            DWORD displacement;

            ok = _SymGetSymFromAddr(myProcess,
                        frame.AddrPC.Offset,
                        &displacement,
                        pSymbol);
#endif

            // All done with debug calls so release our lock.
            ReleaseMutex(hStackWalkMutex);

            if (ok)
                fprintf(aStream, "%s!%s+0x%08X\n", modInfo.ImageName, pSymbol->Name, displacement);
            else
                fprintf(aStream, "0x%08X\n", (DWORD) addr);

            // Stop walking when we get to kernel32.dll.
            if (strcmp(modInfo.ImageName, "kernel32.dll") == 0)
                break;

        }
        else {
            PrintError("LockError", aStream);
        }
        
    }

    return;

}
