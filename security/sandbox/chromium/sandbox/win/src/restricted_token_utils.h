// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_RESTRICTED_TOKEN_UTILS_H__
#define SANDBOX_SRC_RESTRICTED_TOKEN_UTILS_H__

#include <accctrl.h>
#include <windows.h>

#include "base/win/scoped_handle.h"
#include "sandbox/win/src/restricted_token.h"
#include "sandbox/win/src/security_level.h"

// Contains the utility functions to be able to create restricted tokens based
// on a security profiles.

namespace sandbox {

// The type of the token returned by the CreateNakedToken.
enum TokenType {
  IMPERSONATION = 0,
  PRIMARY
};

// Creates a restricted token based on the effective token of the current
// process. The parameter security_level determines how much the token is
// restricted. The token_type determines if the token will be used as a primary
// token or impersonation token. The integrity level of the token is set to
// |integrity level| on Vista only.
// |token| is the output value containing the handle of the newly created
// restricted token.
// |lockdown_default_dacl| indicates the token's default DACL should be locked
// down to restrict what other process can open kernel resources created while
// running under the token.
// If the function succeeds, the return value is ERROR_SUCCESS. If the
// function fails, the return value is the win32 error code corresponding to
// the error.
DWORD CreateRestrictedToken(TokenLevel security_level,
                            IntegrityLevel integrity_level,
                            TokenType token_type,
                            bool lockdown_default_dacl,
                            base::win::ScopedHandle* token);

// Sets the integrity label on a object handle.
DWORD SetObjectIntegrityLabel(HANDLE handle, SE_OBJECT_TYPE type,
                              const wchar_t* ace_access,
                              const wchar_t* integrity_level_sid);

// Sets the integrity level on a token. This is only valid on Vista. It returns
// without failing on XP. If the integrity level that you specify is greater
// than the current integrity level, the function will fail.
DWORD SetTokenIntegrityLevel(HANDLE token, IntegrityLevel integrity_level);

// Returns the integrity level SDDL string associated with a given
// IntegrityLevel value.
const wchar_t* GetIntegrityLevelString(IntegrityLevel integrity_level);

// Sets the integrity level on the current process on Vista. It returns without
// failing on XP. If the integrity level that you specify is greater than the
// current integrity level, the function will fail.
DWORD SetProcessIntegrityLevel(IntegrityLevel integrity_level);

// Hardens the integrity level policy on a token. This is only valid on Win 7
// and above. Specifically it sets the policy to block read and execute so
// that a lower privileged process cannot open the token for impersonate or
// duplicate permissions. This should limit potential security holes.
DWORD HardenTokenIntegrityLevelPolicy(HANDLE token);

// Hardens the integrity level policy on the current process. This is only
// valid on Win 7 and above. Specifically it sets the policy to block read
// and execute so that a lower privileged process cannot open the token for
// impersonate or duplicate permissions. This should limit potential security
// holes.
DWORD HardenProcessIntegrityLevelPolicy();

}  // namespace sandbox

#endif  // SANDBOX_SRC_RESTRICTED_TOKEN_UTILS_H__
