/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Win32 x86/x64 code for stack walking, symbol resolution, and function hooking */

#ifndef __nsDebugHelpWin32_h__
#define __nsDebugHelpWin32_h__

#if defined(_WIN32) && (defined(_M_IX86) || defined(_M_X64))
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <imagehlp.h>
  #include <crtdbg.h>
#else
  #error "nsDebugHelpWin32.h should only be included in Win32 x86/x64 builds"
#endif

// XXX temporary hack...
//#include "hacky_defines.h"


/***************************************************************************/
// useful macros...

#ifdef DHW_IMPLEMENT_GLOBALS
#define DHW_DECLARE_FUN_GLOBAL(name_) decltype(name_)* dhw##name_
#else
#define DHW_DECLARE_FUN_GLOBAL(name_) extern decltype(name_)* dhw##name_
#endif


/**********************************************************/
// This is used to get 'original' function addresses from DHWImportHooker.

#define DHW_ORIGINAL(name_, hooker_) \
    ((decltype(name_)*) hooker_ . GetOriginalFunction())

/***************************************************************************/
// Global declarations of entry points into ImgHelp functions

#ifndef _WIN64
DHW_DECLARE_FUN_GLOBAL(EnumerateLoadedModules);
#else
DHW_DECLARE_FUN_GLOBAL(EnumerateLoadedModules64);
#endif

DHW_DECLARE_FUN_GLOBAL(ImageDirectoryEntryToData);

/***************************************************************************/

extern bool
dhwEnsureImageHlpInitialized();

/***************************************************************************/

class DHWImportHooker
{
public: 

    DHWImportHooker(const char* aModuleName,
                    const char* aFunctionName,
                    PROC aHook,
                    bool aExcludeOurModule = false);
                    
    ~DHWImportHooker();

    PROC GetOriginalFunction()  {return mOriginal;}

    bool PatchAllModules();
    bool PatchOneModule(HMODULE aModule, const char* name);
    static bool ModuleLoaded(HMODULE aModule, DWORD flags);


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
    bool             mHooking;

private:
    static PRLock* gLock;
    static DHWImportHooker* gHooks;
    static decltype(GetProcAddress)* gRealGetProcAddress;
    
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
    virtual bool AllocHook(size_t size) = 0;
    virtual bool ReallocHook(size_t size, size_t sizeOld) = 0;
    virtual bool FreeHook(size_t size) = 0;
};

extern bool dhwSetAllocationSizeDebugHook(DHWAllocationSizeDebugHook* hook);
extern bool dhwClearAllocationSizeDebugHook();

/***************************************************************************/
#endif //0

#endif /* __nsDebugHelpWin32_h__ */
