/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>

// Delayed load libraries are loaded when the first symbol is used.
// The following ensures that we load the delayed loaded libraries from the
// system directory.
struct AutoLoadSystemDependencies
{
  AutoLoadSystemDependencies()
  {
    // Remove the current directory from the search path for dynamically loaded
    // DLLs as a precaution.  This call has no effect for delay load DLLs.
    SetDllDirectory(L"");

    // The order that these are loaded matter, for example if we load something
    // that tries to load profapi.dll first, then profapi.dll would be loaded
    // wrongly from the current directory.
    static LPCWSTR delayDLLs[] = { L"profapi.dll", L"wsock32.dll",
                                   L"crypt32.dll", L"cryptsp.dll",
                                   L"cryptbase.dll", L"msasn1.dll",
                                   L"userenv.dll", L"secur32.dll",
                                   L"ws2_32.dll", L"ws2help.dll",
                                   L"apphelp.dll", L"bcryptprimitives.dll" };

    WCHAR systemDirectory[MAX_PATH + 1] = { L'\0' };
    // If GetSystemDirectory fails we accept that we'll load the DLLs from the
    // normal search path.
    GetSystemDirectory(systemDirectory, MAX_PATH + 1);
    size_t systemDirLen = wcslen(systemDirectory);

    // Make the system directory path terminate with a slash
    if (systemDirectory[systemDirLen - 1] != L'\\' && systemDirLen) {
      systemDirectory[systemDirLen] = L'\\';
      ++systemDirLen;
      // No need to re-NULL terminate
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
      LPCWSTR fullModulePath = systemDirectory; // just for code readability
      LoadLibraryW(fullModulePath);
    }
  }
} loadDLLs;
