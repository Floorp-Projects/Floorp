/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef COMMONUPDATEDIR_H
#define COMMONUPDATEDIR_H

#include "mozilla/UniquePtr.h"

#ifdef XP_WIN
#  include <windows.h>
typedef WCHAR NS_tchar;
#else
typedef char NS_tchar;
#endif

bool GetInstallHash(const char16_t* installPath,
                    mozilla::UniquePtr<NS_tchar[]>& result);

#ifdef XP_WIN
// In addition to getting the update directory, this function also creates it.
// This is to ensure that, when it is created, it is created with the correct
// permissions. The default permissions on the containing directory can cause
// problems, so it is very, very important that we make sure that the
// permissions are set properly. Thus, we won't even give out the path of the
// update directory without ensuring that it was created with the correct
// permissions.
HRESULT GetCommonUpdateDirectory(const wchar_t* installPath,
                                 mozilla::UniquePtr<wchar_t[]>& result);
// Returns the old common update directory. Since this directory was used before
// we made sure to always set the correct permissions, it is possible that the
// permissions on this directory are set such that files can only be modified
// or deleted by the user that created them. This function exists entirely to
// allow us to migrate files out of the old update directory and into the new
// one.
HRESULT GetOldUpdateDirectory(const wchar_t* installPath,
                              mozilla::UniquePtr<wchar_t[]>& result);
#endif

#endif
