/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

