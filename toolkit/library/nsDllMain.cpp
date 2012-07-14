/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
#include <windows.h>
#include <delayimp.h>
#include "nsToolkit.h"

#if defined(__GNUC__)
// If DllMain gets name mangled, it won't be seen.
extern "C" {
#endif

BOOL APIENTRY DllMain(  
                      HINSTANCE hModule, 
                      DWORD reason, 
                      LPVOID lpReserved )
{
    switch( reason ) {
        case DLL_PROCESS_ATTACH:
            nsToolkit::Startup((HINSTANCE)hModule);
            break;

        case DLL_THREAD_ATTACH:
            break;
    
        case DLL_THREAD_DETACH:
            break;
    
        case DLL_PROCESS_DETACH:
            nsToolkit::Shutdown();
            break;

    }

    return TRUE;
}

#if defined(MOZ_METRO)
/*
 * DelayDllLoadHook - the crt calls here anytime a delay load dll is about to
 * load. There are a number of events, we listen for dliNotePreLoadLibrary.
 * 
 * On Win8, we enable Windows Runtime Component Extension support. When enabled
 * the compiler bakes auto-generated code into our binary, including a c init-
 * ializer which inits the winrt library through a call into the winrt standard
 * lib 'vccorlib'. Vccorlib in turn has system dll dependencies which are only
 * available on Win8 (currently API-MS-WIN-CORE-WINRT and
 * API-MS-WIN-CORE-WINRT-STRING), which prevent xul.dll from loading on os <=
 * Win7. To get around this we generate a dummy vccore lib with the three entry
 * points the initializer needs and load it in place of the real vccorlib. We
 * also have to add vccorlib and the system dlls to the delay load list.
 */
static bool IsWin8OrHigher()
{
  static PRInt32 version = 0;

  if (version) {
    return (version >= 0x602);
  }

  // Match Win8 or Win8 Server or higher
  OSVERSIONINFOEX osInfo;
  osInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  ::GetVersionEx((OSVERSIONINFO*)&osInfo);
  version =
    (osInfo.dwMajorVersion & 0xff) << 8 | (osInfo.dwMinorVersion & 0xff);
  return (version >= 0x602);
}

const char* kvccorlib = "vccorlib";
const char* kwinrtprelim = "api-ms-win-core-winrt";

static bool IsWinRTDLLPresent(PDelayLoadInfo pdli, const char* aLibToken)
{
  return (!IsWin8OrHigher() && pdli->szDll &&
          !strnicmp(pdli->szDll, aLibToken, strlen(aLibToken)));
}

FARPROC WINAPI DelayDllLoadHook(unsigned dliNotify, PDelayLoadInfo pdli)
{
  if (dliNotify == dliNotePreLoadLibrary) {
    if (IsWinRTDLLPresent(pdli, kvccorlib)) {
      return (FARPROC)LoadLibraryA("dummyvccorlib.dll");
    }
    NS_ASSERTION(!IsWinRTDLLPresent(pdli, kwinrtprelim),
      "Attempting to load winrt libs in non-metro environment. "
      "(Winrt variable type placed in global scope?)");
  }
  if (dliNotify == dliFailGetProc && IsWinRTDLLPresent(pdli, kvccorlib)) {
    NS_WARNING("Attempting to access winrt vccorlib entry point in non-metro environment.");
    NS_WARNING(pdli->szDll);
    NS_WARNING(pdli->dlp.szProcName);
    NS_ABORT();
  }
  return NULL;
}

ExternC PfnDliHook __pfnDliNotifyHook2 = DelayDllLoadHook;
ExternC PfnDliHook __pfnDliFailureHook2 = DelayDllLoadHook;

#endif // MOZ_METRO

#if defined(__GNUC__)
} // extern "C"
#endif
