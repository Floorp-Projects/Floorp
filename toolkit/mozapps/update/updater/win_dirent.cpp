/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "win_dirent.h"
#include <errno.h>
#include <string.h>

// This file implements the minimum set of dirent APIs used by updater.cpp on
// Windows.  If updater.cpp is modified to use more of this API, we need to
// implement those parts here too.

static dirent gDirEnt;

DIR::DIR(const WCHAR* path)
  : findHandle(INVALID_HANDLE_VALUE)
{
  memset(name, 0, sizeof(name));
  wcsncpy(name, path, sizeof(name)/sizeof(name[0]));
  wcsncat(name, L"\\*", sizeof(name)/sizeof(name[0]) - wcslen(name) - 1);
}

DIR::~DIR()
{
  if (findHandle != INVALID_HANDLE_VALUE) {
    FindClose(findHandle);
  }
}

dirent::dirent()
{
  d_name[0] = L'\0';
}

DIR*
opendir(const WCHAR* path)
{
  return new DIR(path);
}

int
closedir(DIR* dir)
{
  delete dir;
  return 0;
}

dirent* readdir(DIR* dir)
{
  WIN32_FIND_DATAW data;
  if (dir->findHandle != INVALID_HANDLE_VALUE) {
    BOOL result = FindNextFileW(dir->findHandle, &data);
    if (!result) {
      if (GetLastError() != ERROR_FILE_NOT_FOUND) {
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
  memset(gDirEnt.d_name, 0, sizeof(gDirEnt.d_name));
  wcsncpy(gDirEnt.d_name, data.cFileName,
           sizeof(gDirEnt.d_name)/sizeof(gDirEnt.d_name[0]));
  return &gDirEnt;
}

