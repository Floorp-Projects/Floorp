/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_FileUtilsWin_h
#define mozilla_FileUtilsWin_h

#include <windows.h>

#include "mozilla/Scoped.h"
#include "nsStringGlue.h"

namespace mozilla {

inline bool
NtPathToDosPath(const nsAString& aNtPath, nsAString& aDosPath)
{
  aDosPath.Truncate();
  if (aNtPath.IsEmpty()) {
    return true;
  }
  NS_NAMED_LITERAL_STRING(symLinkPrefix, "\\??\\");
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
  DWORD len = 0;
  while(true) {
    len = GetLogicalDriveStringsW(len, logicalDrives.BeginWriting());
    if (!len) {
      return false;
    } else if (len > logicalDrives.Length()) {
      logicalDrives.SetLength(len);
    } else {
      break;
    }
  }
  const char16_t* cur = logicalDrives.BeginReading();
  const char16_t* end = logicalDrives.EndReading();
  nsString targetPath;
  targetPath.SetLength(MAX_PATH);
  wchar_t driveTemplate[] = L" :";
  do {
    // Unfortunately QueryDosDevice doesn't support the idiom for querying the
    // output buffer size, so it may require retries.
    driveTemplate[0] = *cur;
    DWORD targetPathLen = 0;
    SetLastError(ERROR_SUCCESS);
    while (true) {
      targetPathLen = QueryDosDeviceW(driveTemplate, targetPath.BeginWriting(),
                                      targetPath.Length());
      if (targetPathLen || GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        break;
      }
      targetPath.SetLength(targetPath.Length() * 2);
    }
    if (targetPathLen) {
      // Need to use wcslen here because targetPath contains embedded NULL chars
      size_t firstTargetPathLen = wcslen(targetPath.get());
      const char16_t* pathComponent = aNtPath.BeginReading() +
                                      firstTargetPathLen;
      bool found = _wcsnicmp(aNtPath.BeginReading(), targetPath.get(),
                             firstTargetPathLen) == 0 &&
                   *pathComponent == L'\\';
      if (found) {
        aDosPath = driveTemplate;
        aDosPath += pathComponent;
        return true;
      }
    }
    // Advance to the next NUL character in logicalDrives
    while (*cur++);
  } while (cur != end);
  // Code for handling UNC paths would go here, if eventually required.
#if defined(DEBUG)
  NS_NAMED_LITERAL_STRING(deviceMupPrefix, "\\Device\\Mup\\");
  uint32_t deviceMupPrefixLen = deviceMupPrefix.Length();
  if (ntPathLen >= deviceMupPrefixLen &&
      Substring(aNtPath, 0, deviceMupPrefixLen).Equals(deviceMupPrefix)) {
    NS_WARNING("UNC paths not yet supported in NtPathToDosPath");
  }
#endif // defined(DEBUG)
  return false;
}

bool
HandleToFilename(HANDLE aHandle, const LARGE_INTEGER& aOffset,
                 nsAString& aFilename);

} // namespace mozilla

#endif // mozilla_FileUtilsWin_h
