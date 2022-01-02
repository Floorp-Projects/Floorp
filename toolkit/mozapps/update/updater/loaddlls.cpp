/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>

// Delayed load libraries are loaded when the first symbol is used.
// The following ensures that we load the delayed loaded libraries from the
// system directory.
struct AutoLoadSystemDependencies {
  AutoLoadSystemDependencies() {
    // Remove the current directory from the search path for dynamically loaded
    // DLLs as a precaution.  This call has no effect for delay load DLLs.
    SetDllDirectory(L"");

    HMODULE module = ::GetModuleHandleW(L"kernel32.dll");
    if (module) {
      // SetDefaultDllDirectories is always available on Windows 8 and above. It
      // is also available on Windows Vista, Windows Server 2008, and
      // Windows 7 when MS KB2533623 has been applied.
      decltype(SetDefaultDllDirectories)* setDefaultDllDirectories =
          (decltype(SetDefaultDllDirectories)*)GetProcAddress(
              module, "SetDefaultDllDirectories");
      if (setDefaultDllDirectories) {
        setDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
        return;
      }
    }

    // When SetDefaultDllDirectories is not available, fallback to preloading
    // dlls. The order that these are loaded does not matter since they are
    // loaded using the LOAD_WITH_ALTERED_SEARCH_PATH flag.
#ifdef HAVE_64BIT_BUILD
    // DLLs for Firefox x64 on Windows 7 (x64).
    // Note: dwmapi.dll is preloaded since a crash will try to load it from the
    // application's directory.
    static LPCWSTR delayDLLs[] = {
        L"apphelp.dll", L"cryptbase.dll", L"cryptsp.dll", L"dwmapi.dll",
        L"mpr.dll",     L"ntmarta.dll",   L"profapi.dll", L"propsys.dll",
        L"sspicli.dll", L"wsock32.dll"};

#else
    // DLLs for Firefox x86 on Windows XP through Windows 7 (x86 and x64).
    // Note: dwmapi.dll is preloaded since a crash will try to load it from the
    // application's directory.
    static LPCWSTR delayDLLs[] = {
        L"apphelp.dll", L"crypt32.dll", L"cryptbase.dll", L"cryptsp.dll",
        L"dwmapi.dll",  L"mpr.dll",     L"msasn1.dll",    L"ntmarta.dll",
        L"profapi.dll", L"propsys.dll", L"psapi.dll",     L"secur32.dll",
        L"sspicli.dll", L"userenv.dll", L"uxtheme.dll",   L"ws2_32.dll",
        L"ws2help.dll", L"wsock32.dll"};
#endif

    WCHAR systemDirectory[MAX_PATH + 1] = {L'\0'};
    // If GetSystemDirectory fails we accept that we'll load the DLLs from the
    // normal search path.
    GetSystemDirectoryW(systemDirectory, MAX_PATH + 1);
    size_t systemDirLen = wcslen(systemDirectory);

    // Make the system directory path terminate with a slash
    if (systemDirectory[systemDirLen - 1] != L'\\' && systemDirLen) {
      systemDirectory[systemDirLen] = L'\\';
      ++systemDirLen;
      // No need to re-null terminate
    }

    // For each known DLL ensure it is loaded from the system32 directory
    for (size_t i = 0; i < sizeof(delayDLLs) / sizeof(delayDLLs[0]); ++i) {
      size_t fileLen = wcslen(delayDLLs[i]);
      wcsncpy(systemDirectory + systemDirLen, delayDLLs[i],
              MAX_PATH - systemDirLen);
      if (systemDirLen + fileLen <= MAX_PATH) {
        systemDirectory[systemDirLen + fileLen] = L'\0';
      } else {
        systemDirectory[MAX_PATH] = L'\0';
      }
      LPCWSTR fullModulePath = systemDirectory;  // just for code readability
      // LOAD_WITH_ALTERED_SEARCH_PATH makes a dll look in its own directory for
      // dependencies and is only available on Win 7 and below.
      LoadLibraryExW(fullModulePath, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    }
  }
} loadDLLs;
