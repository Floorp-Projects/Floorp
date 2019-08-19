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
#include "mozilla/Assertions.h"
#include "mozilla/UniquePtr.h"

// ----------------------------------------------------------------------------
// Critical Section helper class
// ----------------------------------------------------------------------------

class AutoCriticalSection {
 public:
  explicit AutoCriticalSection(LPCRITICAL_SECTION aSection)
      : mSection(aSection) {
    ::EnterCriticalSection(mSection);
  }
  ~AutoCriticalSection() { ::LeaveCriticalSection(mSection); }

 private:
  LPCRITICAL_SECTION mSection;
};

template <>
class nsAutoRefTraits<HKEY> {
 public:
  typedef HKEY RawRef;
  static HKEY Void() { return nullptr; }

  static void Release(RawRef aFD) {
    if (aFD != Void()) {
      RegCloseKey(aFD);
    }
  }
};

template <>
class nsAutoRefTraits<HDC> {
 public:
  typedef HDC RawRef;
  static HDC Void() { return nullptr; }

  static void Release(RawRef aFD) {
    if (aFD != Void()) {
      ::DeleteDC(aFD);
    }
  }
};

template <>
class nsAutoRefTraits<HBRUSH> {
 public:
  typedef HBRUSH RawRef;
  static HBRUSH Void() { return nullptr; }

  static void Release(RawRef aFD) {
    if (aFD != Void()) {
      ::DeleteObject(aFD);
    }
  }
};

template <>
class nsAutoRefTraits<HRGN> {
 public:
  typedef HRGN RawRef;
  static HRGN Void() { return nullptr; }

  static void Release(RawRef aFD) {
    if (aFD != Void()) {
      ::DeleteObject(aFD);
    }
  }
};

template <>
class nsAutoRefTraits<HBITMAP> {
 public:
  typedef HBITMAP RawRef;
  static HBITMAP Void() { return nullptr; }

  static void Release(RawRef aFD) {
    if (aFD != Void()) {
      ::DeleteObject(aFD);
    }
  }
};

template <>
class nsAutoRefTraits<SC_HANDLE> {
 public:
  typedef SC_HANDLE RawRef;
  static SC_HANDLE Void() { return nullptr; }

  static void Release(RawRef aFD) {
    if (aFD != Void()) {
      CloseServiceHandle(aFD);
    }
  }
};

template <>
class nsSimpleRef<HANDLE> {
 protected:
  typedef HANDLE RawRef;

  nsSimpleRef() : mRawRef(nullptr) {}

  explicit nsSimpleRef(RawRef aRawRef) : mRawRef(aRawRef) {}

  bool HaveResource() const {
    return mRawRef && mRawRef != INVALID_HANDLE_VALUE;
  }

 public:
  RawRef get() const { return mRawRef; }

  static void Release(RawRef aRawRef) {
    if (aRawRef && aRawRef != INVALID_HANDLE_VALUE) {
      CloseHandle(aRawRef);
    }
  }
  RawRef mRawRef;
};

template <>
class nsAutoRefTraits<HMODULE> {
 public:
  typedef HMODULE RawRef;
  static RawRef Void() { return nullptr; }

  static void Release(RawRef aFD) {
    if (aFD != Void()) {
      FreeLibrary(aFD);
    }
  }
};

template <>
class nsAutoRefTraits<DEVMODEW*> {
 public:
  typedef DEVMODEW* RawRef;
  static RawRef Void() { return nullptr; }

  static void Release(RawRef aDevMode) {
    if (aDevMode != Void()) {
      ::HeapFree(::GetProcessHeap(), 0, aDevMode);
    }
  }
};

// HGLOBAL is just a typedef of HANDLE which nsSimpleRef has a specialization
// of, that means having a nsAutoRefTraits specialization for HGLOBAL is
// useless. Therefore we create a wrapper class for HGLOBAL to make
// nsAutoRefTraits and nsAutoRef work as intention.
class nsHGLOBAL {
 public:
  MOZ_IMPLICIT nsHGLOBAL(HGLOBAL hGlobal) : m_hGlobal(hGlobal) {}

  operator HGLOBAL() const { return m_hGlobal; }

 private:
  HGLOBAL m_hGlobal;
};

template <>
class nsAutoRefTraits<nsHGLOBAL> {
 public:
  typedef nsHGLOBAL RawRef;
  static RawRef Void() { return nullptr; }

  static void Release(RawRef hGlobal) { ::GlobalFree(hGlobal); }
};

// Because Printer's HANDLE uses ClosePrinter and we already have
// nsAutoRef<HANDLE> which uses CloseHandle so we need to create a wrapper class
// for HANDLE to have another specialization for nsAutoRefTraits.
class nsHPRINTER {
 public:
  MOZ_IMPLICIT nsHPRINTER(HANDLE hPrinter) : m_hPrinter(hPrinter) {}

  operator HANDLE() const { return m_hPrinter; }

  HANDLE* operator&() { return &m_hPrinter; }

 private:
  HANDLE m_hPrinter;
};

// winspool.h header has AddMonitor macro, it conflicts with AddMonitor member
// function in TaskbarPreview.cpp and TaskbarTabPreview.cpp. Beside, we only
// need ClosePrinter here for Release function, so having its prototype is
// enough.
extern "C" BOOL WINAPI ClosePrinter(HANDLE hPrinter);

template <>
class nsAutoRefTraits<nsHPRINTER> {
 public:
  typedef nsHPRINTER RawRef;
  static RawRef Void() { return nullptr; }

  static void Release(RawRef hPrinter) { ::ClosePrinter(hPrinter); }
};

typedef nsAutoRef<HKEY> nsAutoRegKey;
typedef nsAutoRef<HDC> nsAutoHDC;
typedef nsAutoRef<HBRUSH> nsAutoBrush;
typedef nsAutoRef<HRGN> nsAutoRegion;
typedef nsAutoRef<HBITMAP> nsAutoBitmap;
typedef nsAutoRef<SC_HANDLE> nsAutoServiceHandle;
typedef nsAutoRef<HANDLE> nsAutoHandle;
typedef nsAutoRef<HMODULE> nsModuleHandle;
typedef nsAutoRef<DEVMODEW*> nsAutoDevMode;
typedef nsAutoRef<nsHGLOBAL> nsAutoGlobalMem;
typedef nsAutoRef<nsHPRINTER> nsAutoPrinter;

namespace {

// Construct a path "<system32>\<aModule>". return false if the output buffer
// is too small.
// Note: If the system path cannot be found, or doesn't fit in the output buffer
// with the module name, we will just ignore the system path and output the
// module name alone;
// this may mean using a normal search path wherever the output is used.
bool inline ConstructSystem32Path(LPCWSTR aModule, WCHAR* aSystemPath,
                                  UINT aSize) {
  MOZ_ASSERT(aSystemPath);

  size_t fileLen = wcslen(aModule);
  if (fileLen >= aSize) {
    // The module name alone cannot even fit!
    return false;
  }

  size_t systemDirLen = GetSystemDirectoryW(aSystemPath, aSize);

  if (systemDirLen) {
    if (systemDirLen < aSize - fileLen) {
      // Make the system directory path terminate with a slash.
      if (aSystemPath[systemDirLen - 1] != L'\\') {
        if (systemDirLen + 1 < aSize - fileLen) {
          aSystemPath[systemDirLen] = L'\\';
          ++systemDirLen;
          // No need to re-nullptr terminate.
        } else {
          // Couldn't fit the system path with added slash.
          systemDirLen = 0;
        }
      }
    } else {
      // Couldn't fit the system path.
      systemDirLen = 0;
    }
  }

  MOZ_ASSERT(systemDirLen + fileLen < aSize);

  wcsncpy(aSystemPath + systemDirLen, aModule, fileLen);
  aSystemPath[systemDirLen + fileLen] = L'\0';
  return true;
}

HMODULE inline LoadLibrarySystem32(LPCWSTR aModule) {
  WCHAR systemPath[MAX_PATH + 1];
  if (!ConstructSystem32Path(aModule, systemPath, MAX_PATH + 1)) {
    return NULL;
  }
  return LoadLibraryW(systemPath);
}

}  // namespace

// for UniquePtr
struct LocalFreeDeleter {
  void operator()(void* aPtr) { ::LocalFree(aPtr); }
};

// for UniquePtr to store a PSID
struct FreeSidDeleter {
  void operator()(void* aPtr) { ::FreeSid(aPtr); }
};
// Unfortunately, although SID is a struct, PSID is a void*
// This typedef will work for storing a PSID in a UniquePtr and should make
// things a bit more readable.
typedef mozilla::UniquePtr<void, FreeSidDeleter> UniqueSidPtr;
#endif
