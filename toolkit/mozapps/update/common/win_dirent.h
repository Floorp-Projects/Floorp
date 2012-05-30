/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WINDIRENT_H__
#define WINDIRENT_H__

#ifndef XP_WIN
#error This library should only be used on Windows
#endif

#include <windows.h>

struct DIR {
  explicit DIR(const WCHAR* path);
  ~DIR();
  HANDLE findHandle;
  WCHAR name[MAX_PATH];
};

struct dirent {
  dirent();
  WCHAR d_name[MAX_PATH];
};

DIR* opendir(const WCHAR* path);
int closedir(DIR* dir);
dirent* readdir(DIR* dir);

#endif  // WINDIRENT_H__
