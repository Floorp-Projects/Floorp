/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "imagehlp.h"
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

SYMFUNCTIONTABLEACCESSPROC _SymFunctionTableAccess;

SYMGETMODULEBASEPROC _SymGetModuleBase;

SYMGETSYMFROMADDRPROC _SymGetSymFromAddr;

SYMLOADMODULE _SymLoadModule;

SYMUNDNAME _SymUnDName;

SYMGETMODULEINFO _SymGetModuleInfo;

ENUMLOADEDMODULES _EnumerateLoadedModules;

SYMGETLINEFROMADDRPROC _SymGetLineFromAddr;

PR_END_EXTERN_C




PRBool
EnsureImageHlpInitialized()
{
  static PRBool gInitialized = PR_FALSE;

  if (! gInitialized) {
    HMODULE module = ::LoadLibrary("IMAGEHLP.DLL");
    if (!module) return PR_FALSE;

    _SymSetOptions = (SYMSETOPTIONSPROC) ::GetProcAddress(module, "SymSetOptions");
    if (!_SymSetOptions) return PR_FALSE;

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

    _SymLoadModule = (SYMLOADMODULE)GetProcAddress(module, "SymLoadModule");
    if (!_SymLoadModule) return PR_FALSE;

    _SymUnDName = (SYMUNDNAME)GetProcAddress(module, "SymUnDName");
    if (!_SymUnDName) return PR_FALSE;

    _SymGetModuleInfo = (SYMGETMODULEINFO)GetProcAddress(module, "SymGetModuleInfo");
    if (!_SymGetModuleInfo) return PR_FALSE;

    _EnumerateLoadedModules = (ENUMLOADEDMODULES)GetProcAddress(module, "EnumerateLoadedModules");
    if (!_EnumerateLoadedModules) return PR_FALSE;

    _SymGetLineFromAddr = (SYMGETLINEFROMADDRPROC)GetProcAddress(module, "SymGetLineFromAddr");
    if (!_SymGetLineFromAddr) return PR_FALSE;

    gInitialized = PR_TRUE;
  }

  return gInitialized;
} 

/*
 * Callback used by SymGetModuleInfoEspecial
 */
static BOOL CALLBACK callbackEspecial(LPSTR aModuleName, ULONG aModuleBase, ULONG aModuleSize, PVOID aUserContext)
{
    BOOL retval = TRUE;
    DWORD addr = (DWORD)aUserContext;

    /*
     * You'll want to control this if we are running on an
     *  architecture where the addresses go the other direction.
     * Not sure this is even a realistic consideration.
     */
    const BOOL addressIncreases = TRUE;
    
    /*
     * If it falls in side the known range, load the symbols.
     */
    if(addressIncreases
       ? (addr >= aModuleBase && addr <= (aModuleBase + aModuleSize))
       : (addr <= aModuleBase && addr >= (aModuleBase - aModuleSize))
        )
    {
        BOOL loadRes = FALSE;
        HANDLE process = GetCurrentProcess();
                
        loadRes = _SymLoadModule(process, NULL, aModuleName, NULL, aModuleBase, aModuleSize);
        PR_ASSERT(FALSE != loadRes);
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
        enumRes = _EnumerateLoadedModules(aProcess, callbackEspecial, (PVOID)aAddr);
        if(FALSE != enumRes)
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

PRBool
EnsureSymInitialized()
{  
  static PRBool gInitialized = PR_FALSE;

  if (! gInitialized) {
    if (! EnsureImageHlpInitialized())
      return PR_FALSE;
    _SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
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
DumpStackToFile(FILE* aStream) 
{
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

  // Now walk the stack and map the pc's to symbol names
  int skip = 2;
  while (1) {
    ok = _StackWalk(IMAGE_FILE_MACHINE_I386,
                   myProcess,
                   myThread,
                   &frame,
                   &context,
                   0,                        // read process memory routine
                   _SymFunctionTableAccess,  // function table access routine
                   _SymGetModuleBase,        // module base routine
                   0);                       // translate address routine

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
      fprintf(aStream, "### ERROR: WalkStack: %s", lpMsgBuf);
      fflush(aStream);
      LocalFree( lpMsgBuf );
    }
    if (!ok || frame.AddrPC.Offset == 0)
      break;

    if (skip-- > 0)
      continue;

    //
    // Attempt to load module info before we attempt to reolve the symbol.
    // This just makes sure we get good info if available.
    //
    IMAGEHLP_MODULE modInfo;
    modInfo.SizeOfStruct = sizeof(modInfo);
    BOOL modInfoRes = TRUE;
    modInfoRes = SymGetModuleInfoEspecial(myProcess, frame.AddrPC.Offset, &modInfo, nsnull);

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
      fprintf(aStream, "%s+0x%08X\n", symbol->Name, displacement);
    }
    else {
      fprintf(aStream, "0x%08X\n", frame.AddrPC.Offset);
    }
  }
}

