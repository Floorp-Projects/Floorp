/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef COMMONUPDATEDIR_H
#define COMMONUPDATEDIR_H

#include "mozilla/UniquePtr.h"
#include "nsError.h"

#ifdef XP_WIN
#  include <windows.h>
typedef WCHAR NS_tchar;
#else
typedef char NS_tchar;
#endif

bool GetInstallHash(const char16_t* installPath, const char* vendor,
                    mozilla::UniquePtr<NS_tchar[]>& result,
                    bool useCompatibilityMode = false);

#ifdef XP_WIN
enum class SetPermissionsOf {
  BaseDirIfNotExists,
  AllFilesAndDirs,
  FilesAndDirsWithBadPerms,
};
// This function does two things. It retrieves the update directory and it sets
// the permissions of the directory and, optionally, its contents.
HRESULT GetCommonUpdateDirectory(const wchar_t* installPath,
                                 SetPermissionsOf dirPermsToSet,
                                 mozilla::UniquePtr<wchar_t[]>& result);
HRESULT GetUserUpdateDirectory(const wchar_t* installPath, const char* vendor,
                               const char* appName,
                               mozilla::UniquePtr<wchar_t[]>& result);
#endif

#endif
