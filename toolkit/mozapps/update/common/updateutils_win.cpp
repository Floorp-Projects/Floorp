/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "updateutils_win.h"
#include <errno.h>
#include <shlwapi.h>
#include <string.h>

/**
 * Note: The reason that these functions are separated from those in
 *       updatehelper.h/updatehelper.cpp is that those functions are strictly
 *       used within the updater, whereas changing functions in updateutils_win
 *       will have effects reaching beyond application update.
 */

// This section implements the minimum set of dirent APIs used by updater.cpp on
// Windows.  If updater.cpp is modified to use more of this API, we need to
// implement those parts here too.
static dirent gDirEnt;

DIR::DIR(const WCHAR* path) : findHandle(INVALID_HANDLE_VALUE) {
  memset(name, 0, sizeof(name));
  wcsncpy(name, path, sizeof(name) / sizeof(name[0]));
  wcsncat(name, L"\\*", sizeof(name) / sizeof(name[0]) - wcslen(name) - 1);
}

DIR::~DIR() {
  if (findHandle != INVALID_HANDLE_VALUE) {
    FindClose(findHandle);
  }
}

dirent::dirent() { d_name[0] = L'\0'; }

DIR* opendir(const WCHAR* path) { return new DIR(path); }

int closedir(DIR* dir) {
  delete dir;
  return 0;
}

dirent* readdir(DIR* dir) {
  WIN32_FIND_DATAW data;
  if (dir->findHandle != INVALID_HANDLE_VALUE) {
    BOOL result = FindNextFileW(dir->findHandle, &data);
    if (!result) {
      if (GetLastError() != ERROR_NO_MORE_FILES) {
        errno = ENOENT;
      }
      return 0;
    }
  } else {
    // Reading the first directory entry
    dir->findHandle = FindFirstFileW(dir->name, &data);
    if (dir->findHandle == INVALID_HANDLE_VALUE) {
      if (GetLastError() == ERROR_FILE_NOT_FOUND) {
        errno = ENOENT;
      } else {
        errno = EBADF;
      }
      return 0;
    }
  }
  size_t direntBufferLength =
      sizeof(gDirEnt.d_name) / sizeof(gDirEnt.d_name[0]);
  wcsncpy(gDirEnt.d_name, data.cFileName, direntBufferLength);
  // wcsncpy does not guarantee a null-terminated string if the source string is
  // too long.
  gDirEnt.d_name[direntBufferLength - 1] = '\0';
  return &gDirEnt;
}

/**
 * Joins a base directory path with a filename.
 *
 * @param  base  The base directory path of size MAX_PATH + 1
 * @param  extra The filename to append
 * @return TRUE if the file name was successful appended to base
 */
BOOL PathAppendSafe(LPWSTR base, LPCWSTR extra) {
  if (wcslen(base) + wcslen(extra) >= MAX_PATH) {
    return FALSE;
  }

  return PathAppendW(base, extra);
}

/**
 * Build a temporary file path whose name component is a UUID.
 *
 * @param  basePath  The base directory path for the temp file
 * @param  prefix    Optional prefix for the beginning of the file name
 * @param  tmpPath   Output full path, with the base directory and the file
 * name. Must already have been allocated with size >= MAX_PATH.
 * @return TRUE if tmpPath was successfully filled in, FALSE on errors
 */
BOOL GetUUIDTempFilePath(LPCWSTR basePath, LPCWSTR prefix, LPWSTR tmpPath) {
  WCHAR filename[MAX_PATH + 1] = {L"\0"};
  if (prefix) {
    wcsncpy(filename, prefix, MAX_PATH);
  }

  UUID tmpFileNameUuid;
  RPC_WSTR tmpFileNameString = nullptr;
  if (UuidCreate(&tmpFileNameUuid) != RPC_S_OK) {
    return FALSE;
  }
  if (UuidToStringW(&tmpFileNameUuid, &tmpFileNameString) != RPC_S_OK) {
    return FALSE;
  }
  if (!tmpFileNameString) {
    return FALSE;
  }

  wcsncat(filename, (LPCWSTR)tmpFileNameString, MAX_PATH);
  RpcStringFreeW(&tmpFileNameString);

  wcsncpy(tmpPath, basePath, MAX_PATH);
  if (!PathAppendSafe(tmpPath, filename)) {
    return FALSE;
  }

  return TRUE;
}
