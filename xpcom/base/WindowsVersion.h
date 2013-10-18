/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WindowsVersion_h
#define mozilla_WindowsVersion_h

#include "nscore.h"
#include <windows.h>

namespace mozilla
{
  inline bool
  IsWindowsVersionOrLater(uint64_t aVersion)
  {
    static uint64_t minVersion = 0;
    static uint64_t maxVersion = UINT64_MAX;

    if (minVersion >= aVersion) {
      return true;
    }

    if (aVersion >= maxVersion) {
      return false;
    }

    OSVERSIONINFOEX info;
    ZeroMemory(&info, sizeof(OSVERSIONINFOEX));
    info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    info.dwMajorVersion = aVersion >> 48;
    info.dwMinorVersion = (aVersion >> 32) & 0xFFFF;
    info.wServicePackMajor = (aVersion >> 16) & 0xFFFF;
    info.wServicePackMinor = aVersion & 0xFFFF;

    DWORDLONG conditionMask = 0;
    VER_SET_CONDITION(conditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditionMask, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditionMask, VER_SERVICEPACKMINOR, VER_GREATER_EQUAL);

    if (VerifyVersionInfo(&info,
                          VER_MAJORVERSION | VER_MINORVERSION |
                          VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR,
                          conditionMask)) {
      minVersion = aVersion;
      return true;
    }

    maxVersion = aVersion;
    return false;
  }

  MOZ_ALWAYS_INLINE bool
  IsVistaOrLater()
  { return IsWindowsVersionOrLater(0x0006000000000000ull); }

  MOZ_ALWAYS_INLINE bool
  IsWin7OrLater()
  { return IsWindowsVersionOrLater(0x0006000100000000ull); }

  MOZ_ALWAYS_INLINE bool
  IsWin7SP1OrLater()
  { return IsWindowsVersionOrLater(0x0006000100010000ull); }

  MOZ_ALWAYS_INLINE bool
  IsWin8OrLater()
  { return IsWindowsVersionOrLater(0x0006000200000000ull); }
}

#endif /* mozilla_WindowsVersion_h */
