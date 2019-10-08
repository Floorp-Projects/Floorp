/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WINDIRENT_H__
#define WINDIRENT_H__

/**
 * Note: The reason that these functions are separated from those in
 *       updatehelper.h/updatehelper.cpp is that those functions are strictly
 *       used within the updater, whereas changing functions in updateutils_win
 *       will have effects reaching beyond application update.
 */

#ifndef XP_WIN
#  error This library should only be used on Windows
#endif

#include <windows.h>

struct DIR {
  explicit DIR(const WCHAR* path);
  ~DIR();
  HANDLE findHandle;
  WCHAR name[MAX_PATH + 1];
};

struct dirent {
  dirent();
  WCHAR d_name[MAX_PATH + 1];
};

DIR* opendir(const WCHAR* path);
int closedir(DIR* dir);
dirent* readdir(DIR* dir);

// This is the length of the UUID string including null termination returned by
// GetUUIDString.
#define UUID_LEN 37

BOOL PathAppendSafe(LPWSTR base, LPCWSTR extra);
BOOL GetUUIDString(LPWSTR outBuf);
BOOL GetUUIDTempFilePath(LPCWSTR basePath, LPCWSTR prefix, LPWSTR tmpPath);

#endif  // WINDIRENT_H__
