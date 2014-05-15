/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWindowsHelpers_h
#define nsWindowsHelpers_h

#include <windows.h>
#include "nsAutoRef.h"
#include "nscore.h"

// ----------------------------------------------------------------------------
// Critical Section helper class
// ----------------------------------------------------------------------------

class AutoCriticalSection
{
public:
  AutoCriticalSection(LPCRITICAL_SECTION aSection)
    : mSection(aSection)
  {
    ::EnterCriticalSection(mSection);
  }
  ~AutoCriticalSection()
  {
    ::LeaveCriticalSection(mSection);
  }
private:
  LPCRITICAL_SECTION mSection;
};

template<>
class nsAutoRefTraits<HKEY>
{
public:
  typedef HKEY RawRef;
  static HKEY Void()
  {
    return nullptr;
  }

  static void Release(RawRef aFD)
  {
    if (aFD != Void()) {
      RegCloseKey(aFD);
    }
  }
};

template<>
class nsAutoRefTraits<SC_HANDLE>
{
public:
  typedef SC_HANDLE RawRef;
  static SC_HANDLE Void()
  {
    return nullptr;
  }

  static void Release(RawRef aFD)
  {
    if (aFD != Void()) {
      CloseServiceHandle(aFD);
    }
  }
};

template<>
class nsSimpleRef<HANDLE>
{
protected:
  typedef HANDLE RawRef;

  nsSimpleRef() : mRawRef(nullptr)
  {
  }

  nsSimpleRef(RawRef aRawRef) : mRawRef(aRawRef)
  {
  }

  bool HaveResource() const
  {
    return mRawRef && mRawRef != INVALID_HANDLE_VALUE;
  }

public:
  RawRef get() const
  {
    return mRawRef;
  }

  static void Release(RawRef aRawRef)
  {
    if (aRawRef && aRawRef != INVALID_HANDLE_VALUE) {
      CloseHandle(aRawRef);
    }
  }
  RawRef mRawRef;
};


template<>
class nsAutoRefTraits<HMODULE>
{
public:
  typedef HMODULE RawRef;
  static RawRef Void()
  {
    return nullptr;
  }

  static void Release(RawRef aFD)
  {
    if (aFD != Void()) {
      FreeLibrary(aFD);
    }
  }
};

typedef nsAutoRef<HKEY> nsAutoRegKey;
typedef nsAutoRef<SC_HANDLE> nsAutoServiceHandle;
typedef nsAutoRef<HANDLE> nsAutoHandle;
typedef nsAutoRef<HMODULE> nsModuleHandle;

namespace {

bool
IsRunningInWindowsMetro()
{
  static bool alreadyChecked = false;
  static bool isMetro = false;
  if (alreadyChecked) {
    return isMetro;
  }

  HMODULE user32DLL = LoadLibraryW(L"user32.dll");
  if (!user32DLL) {
    return false;
  }

  typedef BOOL (WINAPI* IsImmersiveProcessFunc)(HANDLE aProcess);
  IsImmersiveProcessFunc IsImmersiveProcessPtr =
    (IsImmersiveProcessFunc)GetProcAddress(user32DLL,
                                           "IsImmersiveProcess");
  FreeLibrary(user32DLL);
  if (!IsImmersiveProcessPtr) {
    // isMetro is already set to false.
    alreadyChecked = true;
    return false;
  }

  isMetro = IsImmersiveProcessPtr(GetCurrentProcess());
  alreadyChecked = true;
  return isMetro;
}

HMODULE
LoadLibrarySystem32(LPCWSTR aModule)
{
  WCHAR systemPath[MAX_PATH + 1] = { L'\0' };

  // If GetSystemPath fails we accept that we'll load the DLLs from the
  // normal search path.
  GetSystemDirectoryW(systemPath, MAX_PATH + 1);
  size_t systemDirLen = wcslen(systemPath);

  // Make the system directory path terminate with a slash
  if (systemDirLen && systemPath[systemDirLen - 1] != L'\\') {
    systemPath[systemDirLen] = L'\\';
    ++systemDirLen;
    // No need to re-nullptr terminate
  }

  size_t fileLen = wcslen(aModule);
  wcsncpy(systemPath + systemDirLen, aModule,
          MAX_PATH - systemDirLen);
  if (systemDirLen + fileLen <= MAX_PATH) {
    systemPath[systemDirLen + fileLen] = L'\0';
  } else {
    systemPath[MAX_PATH] = L'\0';
  }
  return LoadLibraryW(systemPath);
}

}

#endif
