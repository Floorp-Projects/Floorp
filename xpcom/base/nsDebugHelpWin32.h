/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Win32 x86 code for stack walking, symbol resolution, and function hooking */

#ifndef __nsDebugHelpWin32_h__
#define __nsDebugHelpWin32_h__

#if defined(_WIN32) && defined(_M_IX86)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <imagehlp.h>
  #include <crtdbg.h>
#else
  #error "nsDebugHelpWin32.h should only be included in Win32 x86 builds"
#endif

// XXX temporary hack...
//#include "hacky_defines.h"


/***************************************************************************/
// useful macros...

#define DHW_DECLARE_FUN_TYPE(retval_, conv_, typename_, args_) \
    typedef retval_ ( conv_ * typename_ ) args_ ;

#ifdef DHW_IMPLEMENT_GLOBALS
#define DHW_DECLARE_FUN_GLOBAL(typename_, name_) typename_ dhw##name_
#else
#define DHW_DECLARE_FUN_GLOBAL(typename_, name_) extern typename_ dhw##name_
#endif

#define DHW_DECLARE_FUN_PROTO(retval_, conv_, name_, args_) \
    retval_ conv_ name_ args_

#define DHW_DECLARE_FUN_STATIC_PROTO(retval_, name_, args_) \
    static retval_ conv_ name_ args_

#define DHW_DECLARE_FUN_TYPE_AND_PROTO(name_, retval_, conv_, typename_, args_) \
    DHW_DECLARE_FUN_TYPE(retval_, conv_, typename_, args_); \
    DHW_DECLARE_FUN_PROTO(retval_, conv_, name_, args_)

#define DHW_DECLARE_FUN_TYPE_AND_STATIC_PROTO(name_, retval_, conv_, typename_, args_) \
    DHW_DECLARE_FUN_TYPE(retval_, conv_, typename_, args_); \
    DHW_DECLARE_FUN_STATIC_PROTO(retval_, conv_, name_, args_)

#define DHW_DECLARE_FUN_TYPE_AND_GLOBAL(typename_, name_, retval_, conv_, args_) \
    DHW_DECLARE_FUN_TYPE(retval_, conv_, typename_, args_); \
    DHW_DECLARE_FUN_GLOBAL(typename_, name_)


/**********************************************************/
// These are used to get 'original' function addresses from DHWImportHooker.

#define DHW_DECLARE_ORIGINAL(type_, name_, hooker_) \
    type_ name_ = (type_) hooker_ . GetOriginalFunction()

#define DHW_DECLARE_ORIGINAL_PTR(type_, name_, hooker_) \
    type_ name_ = (type_) hooker_ -> GetOriginalFunction()

#define DHW_ORIGINAL(type_, hooker_) \
    ((type_) hooker_ . GetOriginalFunction())

#define DHW_ORIGINAL_PTR(type_, hooker_) \
    ((type_) hooker_ -> GetOriginalFunction())

/***************************************************************************/
// Global declarations of entry points into ImgHelp functions

DHW_DECLARE_FUN_TYPE_AND_GLOBAL(SYMINITIALIZEPROC, SymInitialize, \
                                BOOL, __stdcall, (HANDLE, LPSTR, BOOL));

DHW_DECLARE_FUN_TYPE_AND_GLOBAL(SYMSETOPTIONS, SymSetOptions, \
                                DWORD, __stdcall, (DWORD));

DHW_DECLARE_FUN_TYPE_AND_GLOBAL(SYMGETOPTIONS, SymGetOptions, \
                                DWORD, __stdcall, ());

DHW_DECLARE_FUN_TYPE_AND_GLOBAL(SYMGETMODULEINFO, SymGetModuleInfo, \
                                BOOL, __stdcall, (HANDLE, DWORD, PIMAGEHLP_MODULE));

DHW_DECLARE_FUN_TYPE_AND_GLOBAL(SYMGETSYMFROMADDRPROC, SymGetSymFromAddr, \
                                BOOL, __stdcall, (HANDLE, DWORD, PDWORD, PIMAGEHLP_SYMBOL));

DHW_DECLARE_FUN_TYPE_AND_GLOBAL(ENUMERATELOADEDMODULES, EnumerateLoadedModules, \
                                BOOL, __stdcall, (HANDLE, PENUMLOADED_MODULES_CALLBACK, PVOID));

DHW_DECLARE_FUN_TYPE_AND_GLOBAL(IMAGEDIRECTORYENTRYTODATA, ImageDirectoryEntryToData, \
                                PVOID, __stdcall, (PVOID, BOOL, USHORT, PULONG));


// We aren't using any of the below yet...

/*
DHW_DECLARE_FUN_TYPE_AND_GLOBAL(SYMCLEANUPPROC, SymCleanup, \
                                BOOL, __stdcall, (HANDLE));

DHW_DECLARE_FUN_TYPE_AND_GLOBAL(STACKWALKPROC, StackWalk, \
                                BOOL, 
                                __stdcall, 
                                (DWORD, HANDLE, HANDLE, LPSTACKFRAME, LPVOID, \
                                       PREAD_PROCESS_MEMORY_ROUTINE, \
                                       PFUNCTION_TABLE_ACCESS_ROUTINE, \
                                       PGET_MODULE_BASE_ROUTINE, \
                                       PTRANSLATE_ADDRESS_ROUTINE));

DHW_DECLARE_FUN_TYPE_AND_GLOBAL(SYMFUNCTIONTABLEACCESSPROC, SymFunctionTableAccess, \
                                LPVOID, __stdcall, (HANDLE, DWORD));

DHW_DECLARE_FUN_TYPE_AND_GLOBAL(SYMGETMODULEBASEPROC, SymGetModuleBase, \
                                DWORD, __stdcall, (HANDLE, DWORD));

DHW_DECLARE_FUN_TYPE_AND_GLOBAL(SYMLOADMODULE, SymLoadModule, \
                                DWORD, __stdcall, (HANDLE, HANDLE, PSTR, PSTR, DWORD, DWORD));

DHW_DECLARE_FUN_TYPE_AND_GLOBAL(UNDECORATESYMBOLNAME, _UnDecorateSymbolName, \
                                DWORD, __stdcall, (LPCSTR, LPSTR, DWORD, DWORD));

DHW_DECLARE_FUN_TYPE_AND_GLOBAL(SYMUNDNAME, SymUnDName, \
                                BOOL, __stdcall, (PIMAGEHLP_SYMBOL, LPSTR, DWORD));

DHW_DECLARE_FUN_TYPE_AND_GLOBAL(SYMGETLINEFROMADDR, SymGetLineFromAddr, \
                                BOOL, __stdcall, (HANDLE, DWORD, PDWORD, PIMAGEHLP_LINE));

*/

/***************************************************************************/

extern PRBool
dhwEnsureImageHlpInitialized();

extern PRBool
dhwEnsureSymInitialized();

/***************************************************************************/

DHW_DECLARE_FUN_TYPE(FARPROC, __stdcall, GETPROCADDRESS, (HMODULE, PCSTR));

class DHWImportHooker
{
public: 

    DHWImportHooker(const char* aModuleName,
                    const char* aFunctionName,
                    PROC aHook,
                    PRBool aExcludeOurModule = PR_FALSE);
                    
    ~DHWImportHooker();

    PROC GetOriginalFunction()  {return mOriginal;}

    PRBool PatchAllModules();
    PRBool PatchOneModule(HMODULE aModule, const char* name);
    static PRBool ModuleLoaded(HMODULE aModule, DWORD flags);


    // I think that these should be made not static members, but allocated
    // things created in an explicit static 'init' method and cleaned up in
    // an explicit static 'finish' method. This would allow the application
    // to have proper lifetime control over all the hooks.

    static DHWImportHooker &getLoadLibraryWHooker();
    static DHWImportHooker &getLoadLibraryExWHooker();
    static DHWImportHooker &getLoadLibraryAHooker();
    static DHWImportHooker &getLoadLibraryExAHooker();
    static DHWImportHooker &getGetProcAddressHooker();

    static HMODULE WINAPI LoadLibraryA(PCSTR path);

private:
    DHWImportHooker* mNext;
    const char*      mModuleName;
    const char*      mFunctionName;
    PROC             mOriginal;
    PROC             mHook;
    HMODULE          mIgnoreModule;
    PRBool           mHooking;

private:
    static PRLock* gLock;
    static DHWImportHooker* gHooks;
    static GETPROCADDRESS gRealGetProcAddress;
    
    static HMODULE WINAPI LoadLibraryW(PCWSTR path);
    static HMODULE WINAPI LoadLibraryExW(PCWSTR path, HANDLE file, DWORD flags);
    static HMODULE WINAPI LoadLibraryExA(PCSTR path, HANDLE file, DWORD flags);

    static FARPROC WINAPI GetProcAddress(HMODULE aModule, PCSTR aFunctionName);
};

/***************************************************************************/
// This supports the _CrtSetAllocHook based hooking.
// This system sucks because you don't get to see the allocated pointer. I
// don't think it appropriate for nsTraceMalloc, but is useful as a means to make
// malloc fail for testing purposes.
#if 0 //comment out this stuff. not necessary

class DHWAllocationSizeDebugHook
{
public:
    virtual PRBool AllocHook(size_t size) = 0;
    virtual PRBool ReallocHook(size_t size, size_t sizeOld) = 0;
    virtual PRBool FreeHook(size_t size) = 0;
};

extern PRBool dhwSetAllocationSizeDebugHook(DHWAllocationSizeDebugHook* hook);
extern PRBool dhwClearAllocationSizeDebugHook();

/***************************************************************************/
#endif //0

#endif /* __nsDebugHelpWin32_h__ */
