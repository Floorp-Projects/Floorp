/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_FileUtilsWin_h
#define mozilla_FileUtilsWin_h

#include <windows.h>

#include "nsString.h"

namespace mozilla {

inline bool EnsureLongPath(nsAString& aDosPath) {
  nsAutoString inputPath(aDosPath);
  while (true) {
    DWORD requiredLength = GetLongPathNameW(
        inputPath.get(), reinterpret_cast<wchar_t*>(aDosPath.BeginWriting()),
        aDosPath.Length());
    if (!requiredLength) {
      return false;
    }
    if (requiredLength < aDosPath.Length()) {
      // When GetLongPathNameW deems the last argument too small,
      // it returns a value, but when you pass that value back, it's
      // satisfied and returns a number that's one smaller. If the above
      // check was == instead of <, the loop would go on forever with
      // GetLongPathNameW returning oscillating values!
      aDosPath.Truncate(requiredLength);
      return true;
    }
    aDosPath.SetLength(requiredLength);
  }
}

inline bool NtPathToDosPath(const nsAString& aNtPath, nsAString& aDosPath) {
  aDosPath.Truncate();
  if (aNtPath.IsEmpty()) {
    return true;
  }
  constexpr auto symLinkPrefix = u"\\??\\"_ns;
  uint32_t ntPathLen = aNtPath.Length();
  uint32_t symLinkPrefixLen = symLinkPrefix.Length();
  if (ntPathLen >= 6 && aNtPath.CharAt(5) == L':' &&
      ntPathLen >= symLinkPrefixLen &&
      Substring(aNtPath, 0, symLinkPrefixLen).Equals(symLinkPrefix)) {
    // Symbolic link for DOS device. Just strip off the prefix.
    aDosPath = aNtPath;
    aDosPath.Cut(0, 4);
    return true;
  }
  nsAutoString logicalDrives;
  while (true) {
    DWORD requiredLength = GetLogicalDriveStringsW(
        logicalDrives.Length(),
        reinterpret_cast<wchar_t*>(logicalDrives.BeginWriting()));
    if (!requiredLength) {
      return false;
    }
    if (requiredLength < logicalDrives.Length()) {
      // When GetLogicalDriveStringsW deems the first argument too small,
      // it returns a value, but when you pass that value back, it's
      // satisfied and returns a number that's one smaller. If the above
      // check was == instead of <, the loop would go on forever with
      // GetLogicalDriveStringsW returning oscillating values!
      logicalDrives.Truncate(requiredLength);
      // logicalDrives now has the format "C:\\\0D:\\\0Z:\\\0". That is,
      // the sequence drive letter, colon, backslash, U+0000 repeats.
      break;
    }
    logicalDrives.SetLength(requiredLength);
  }

  const char16_t* cur = logicalDrives.BeginReading();
  const char16_t* end = logicalDrives.EndReading();
  nsString targetPath;
  targetPath.SetLength(MAX_PATH);
  wchar_t driveTemplate[] = L" :";
  while (cur < end) {
    // Unfortunately QueryDosDevice doesn't support the idiom for querying the
    // output buffer size, so it may require retries.
    driveTemplate[0] = *cur;
    DWORD targetPathLen = 0;
    SetLastError(ERROR_SUCCESS);
    while (true) {
      targetPathLen = QueryDosDeviceW(
          driveTemplate, reinterpret_cast<wchar_t*>(targetPath.BeginWriting()),
          targetPath.Length());
      if (targetPathLen || GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        break;
      }
      targetPath.SetLength(targetPath.Length() * 2);
    }
    if (targetPathLen) {
      // Need to use wcslen here because targetPath contains embedded NULL chars
      size_t firstTargetPathLen = wcslen(targetPath.get());
      const char16_t* pathComponent =
          aNtPath.BeginReading() + firstTargetPathLen;
      bool found = _wcsnicmp(char16ptr_t(aNtPath.BeginReading()),
                             targetPath.get(), firstTargetPathLen) == 0 &&
                   *pathComponent == L'\\';
      if (found) {
        aDosPath = driveTemplate;
        aDosPath += pathComponent;
        return EnsureLongPath(aDosPath);
      }
    }
    // Find the next U+0000 within the logical string
    while (*cur) {
      // This loop skips the drive letter, the colon
      // and the backslash.
      cur++;
    }
    // Skip over the U+0000 that ends a drive entry
    // within the logical string
    cur++;
  }
  // Try to handle UNC paths. NB: This must happen after we've checked drive
  // mappings in case a UNC path is mapped to a drive!
  constexpr auto uncPrefix = u"\\\\"_ns;
  constexpr auto deviceMupPrefix = u"\\Device\\Mup\\"_ns;
  if (StringBeginsWith(aNtPath, deviceMupPrefix)) {
    aDosPath = uncPrefix;
    aDosPath += Substring(aNtPath, deviceMupPrefix.Length());
    return true;
  }
  constexpr auto deviceLanmanRedirectorPrefix =
      u"\\Device\\LanmanRedirector\\"_ns;
  if (StringBeginsWith(aNtPath, deviceLanmanRedirectorPrefix)) {
    aDosPath = uncPrefix;
    aDosPath += Substring(aNtPath, deviceLanmanRedirectorPrefix.Length());
    return true;
  }
  return false;
}

bool HandleToFilename(HANDLE aHandle, const LARGE_INTEGER& aOffset,
                      nsAString& aFilename);

uint32_t GetExecutableArchitecture(const wchar_t* aPath);

}  // namespace mozilla

#endif  // mozilla_FileUtilsWin_h
