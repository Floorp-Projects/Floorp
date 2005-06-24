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
 * December 20, 2000.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
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
#ifndef nsStackFrameWin_h___
#define nsStackFrameWin_h___


#if defined(_WIN32) && (defined(_M_IX86) || defined(_M_AMD64) || defined(_M_IA64))
// WIN32 x86 stack walking code
#include "nspr.h"
#include <windows.h>
#ifdef _M_IX86
#include <imagehlp.h>
#endif

// Define these as static pointers so that we can load the DLL on the
// fly (and not introduce a link-time dependency on it). Tip o' the
// hat to Matt Pietrick for this idea. See:
//
//   http://msdn.microsoft.com/library/periodic/period97/F1/D3/S245C6.htm
//
PR_BEGIN_EXTERN_C

typedef DWORD (__stdcall *SYMSETOPTIONSPROC)(DWORD);
extern SYMSETOPTIONSPROC _SymSetOptions;

typedef BOOL (__stdcall *SYMINITIALIZEPROC)(HANDLE, LPSTR, BOOL);
extern SYMINITIALIZEPROC _SymInitialize;

typedef BOOL (__stdcall *SYMCLEANUPPROC)(HANDLE);
extern SYMCLEANUPPROC _SymCleanup;

typedef BOOL (__stdcall *STACKWALKPROC)(DWORD,
                                        HANDLE,
                                        HANDLE,
                                        LPSTACKFRAME,
                                        LPVOID,
                                        PREAD_PROCESS_MEMORY_ROUTINE,
                                        PFUNCTION_TABLE_ACCESS_ROUTINE,
                                        PGET_MODULE_BASE_ROUTINE,
                                        PTRANSLATE_ADDRESS_ROUTINE);
extern  STACKWALKPROC _StackWalk;

typedef BOOL (__stdcall *STACKWALKPROC64)(DWORD,
                                          HANDLE,
                                          HANDLE,
                                          LPSTACKFRAME64,
                                          PVOID,
                                          PREAD_PROCESS_MEMORY_ROUTINE64,
                                          PFUNCTION_TABLE_ACCESS_ROUTINE64,
                                          PGET_MODULE_BASE_ROUTINE64,
                                          PTRANSLATE_ADDRESS_ROUTINE64);
extern  STACKWALKPROC64 _StackWalk64;

typedef LPVOID (__stdcall *SYMFUNCTIONTABLEACCESSPROC)(HANDLE, DWORD);
extern  SYMFUNCTIONTABLEACCESSPROC _SymFunctionTableAccess;

typedef LPVOID (__stdcall *SYMFUNCTIONTABLEACCESSPROC64)(HANDLE, DWORD64);
extern  SYMFUNCTIONTABLEACCESSPROC64 _SymFunctionTableAccess64;

typedef DWORD (__stdcall *SYMGETMODULEBASEPROC)(HANDLE, DWORD);
extern  SYMGETMODULEBASEPROC _SymGetModuleBase;

typedef DWORD64 (__stdcall *SYMGETMODULEBASEPROC64)(HANDLE, DWORD64);
extern  SYMGETMODULEBASEPROC64 _SymGetModuleBase64;

typedef BOOL (__stdcall *SYMFROMADDRPROC)(HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO);
extern  SYMFROMADDRPROC _SymFromAddr;

typedef DWORD ( __stdcall *SYMLOADMODULE)(HANDLE, HANDLE, PSTR, PSTR, DWORD, DWORD);
extern  SYMLOADMODULE _SymLoadModule;

typedef DWORD ( __stdcall *SYMLOADMODULE64)(HANDLE, HANDLE, PCSTR, PCSTR, DWORD64, DWORD);
extern  SYMLOADMODULE64 _SymLoadModule64;

typedef DWORD ( __stdcall *SYMUNDNAME)(PIMAGEHLP_SYMBOL, PSTR, DWORD);
extern  SYMUNDNAME _SymUnDName;

typedef DWORD ( __stdcall *SYMGETMODULEINFO)( HANDLE, DWORD, PIMAGEHLP_MODULE);
extern  SYMGETMODULEINFO _SymGetModuleInfo;

typedef BOOL ( __stdcall *SYMGETMODULEINFO64)( HANDLE, DWORD64, PIMAGEHLP_MODULE64);
extern  SYMGETMODULEINFO64 _SymGetModuleInfo64;

typedef BOOL ( __stdcall *ENUMLOADEDMODULES)( HANDLE, PENUMLOADED_MODULES_CALLBACK, PVOID);
extern  ENUMLOADEDMODULES _EnumerateLoadedModules;

typedef BOOL ( __stdcall *ENUMLOADEDMODULES64)( HANDLE, PENUMLOADED_MODULES_CALLBACK64, PVOID);
extern  ENUMLOADEDMODULES64 _EnumerateLoadedModules64;

typedef BOOL (__stdcall *SYMGETLINEFROMADDRPROC)(HANDLE, DWORD, PDWORD, PIMAGEHLP_LINE);
extern  SYMGETLINEFROMADDRPROC _SymGetLineFromAddr;

typedef BOOL (__stdcall *SYMGETLINEFROMADDRPROC64)(HANDLE, DWORD64, PDWORD, PIMAGEHLP_LINE64);
extern  SYMGETLINEFROMADDRPROC64 _SymGetLineFromAddr64;

extern HANDLE hStackWalkMutex; 

HANDLE GetCurrentPIDorHandle();

PRBool EnsureSymInitialized();

PRBool EnsureImageHlpInitialized();

/*
 * SymGetModuleInfoEspecial
 *
 * Attempt to determine the module information.
 * Bug 112196 says this DLL may not have been loaded at the time
 *  SymInitialize was called, and thus the module information
 *  and symbol information is not available.
 * This code rectifies that problem.
 * Line information is optional.
 */
BOOL SymGetModuleInfoEspecial(HANDLE aProcess, DWORD aAddr, PIMAGEHLP_MODULE aModuleInfo, PIMAGEHLP_LINE aLineInfo);

struct DumpStackToFileData {
  FILE *stream;
  HANDLE thread;
  HANDLE process;
};

void PrintError(char *prefix, FILE* out);
void DumpStackToFile(FILE* out);
DWORD WINAPI  DumpStackToFileThread(LPVOID data);
void DumpStackToFileMain64(struct DumpStackToFileData* data);
void DumpStackToFileMain(struct DumpStackToFileData* data);


PR_END_EXTERN_C

#else
#pragma message( "You probably need to fix this file to handle your target platform" )
#endif //WIN32

#endif //nsStackFrameWin_h___


