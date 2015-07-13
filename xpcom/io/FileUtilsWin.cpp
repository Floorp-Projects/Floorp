/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileUtilsWin.h"

#include <windows.h>
#include <psapi.h>

#include "nsWindowsHelpers.h"

namespace {

// Scoped type used by HandleToFilename
struct ScopedMappedViewTraits
{
  typedef void* type;
  static void* empty()
  {
    return nullptr;
  }
  static void release(void* aPtr)
  {
    UnmapViewOfFile(aPtr);
  }
};
typedef mozilla::Scoped<ScopedMappedViewTraits> ScopedMappedView;

} // namespace

namespace mozilla {

bool
HandleToFilename(HANDLE aHandle, const LARGE_INTEGER& aOffset,
                 nsAString& aFilename)
{
  aFilename.Truncate();
  // This implementation is nice because it uses fully documented APIs that
  // are available on all Windows versions that we support.
  nsAutoHandle fileMapping(CreateFileMapping(aHandle, nullptr, PAGE_READONLY,
                                             0, 1, nullptr));
  if (!fileMapping) {
    return false;
  }
  ScopedMappedView view(MapViewOfFile(fileMapping, FILE_MAP_READ,
                                      aOffset.HighPart, aOffset.LowPart, 1));
  if (!view) {
    return false;
  }
  nsAutoString mappedFilename;
  DWORD len = 0;
  SetLastError(ERROR_SUCCESS);
  do {
    mappedFilename.SetLength(mappedFilename.Length() + MAX_PATH);
    len = GetMappedFileNameW(GetCurrentProcess(), view,
                             wwc(mappedFilename.BeginWriting()),
                             mappedFilename.Length());
  } while (!len && GetLastError() == ERROR_INSUFFICIENT_BUFFER);
  if (!len) {
    return false;
  }
  mappedFilename.Truncate(len);
  return NtPathToDosPath(mappedFilename, aFilename);
}

} // namespace mozilla

