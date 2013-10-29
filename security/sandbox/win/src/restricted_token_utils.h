// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_RESTRICTED_TOKEN_UTILS_H__
#define SANDBOX_SRC_RESTRICTED_TOKEN_UTILS_H__

#include <accctrl.h>
#include <windows.h>

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
// token_handle is the output value containing the handle of the
// newly created restricted token.
// If the function succeeds, the return value is ERROR_SUCCESS. If the
// function fails, the return value is the win32 error code corresponding to
// the error.
DWORD CreateRestrictedToken(HANDLE *token_handle,
                            TokenLevel security_level,
                            IntegrityLevel integrity_level,
                            TokenType token_type);

// Starts the process described by the input parameter command_line in a job
// with a restricted token. Also set the main thread of this newly created
// process to impersonate a user with more rights so it can initialize
// correctly.
//
// Parameters: primary_level is the security level of the primary token.
// impersonation_level is the security level of the impersonation token used
// to initialize the process. job_level is the security level of the job
// object used to encapsulate the process.
//
// The output parameter job_handle is the handle to the job object. It has
// to be closed with CloseHandle() when not needed. Closing this handle will
// kill the process started.
//
// Note: The process started with this function has to call RevertToSelf() as
// soon as possible to stop using the impersonation token and start being
// secure.
//
// Note: The Unicode version of this function will fail if the command_line
// parameter is a const string.
DWORD StartRestrictedProcessInJob(wchar_t *command_line,
                                  TokenLevel primary_level,
                                  TokenLevel impersonation_level,
                                  JobLevel job_level,
                                  HANDLE *job_handle);

// Sets the integrity label on a object handle.
DWORD SetObjectIntegrityLabel(HANDLE handle, SE_OBJECT_TYPE type,
                              const wchar_t* ace_access,
                              const wchar_t* integrity_level_sid);

// Sets the integrity level on a token. This is only valid on Vista. It returns
// without failing on XP. If the integrity level that you specify is greater
// than the current integrity level, the function will fail.
DWORD SetTokenIntegrityLevel(HANDLE token, IntegrityLevel integrity_level);

// Sets the integrity level on the current process on Vista. It returns without
// failing on XP. If the integrity level that you specify is greater than the
// current integrity level, the function will fail.
DWORD SetProcessIntegrityLevel(IntegrityLevel integrity_level);

}  // namespace sandbox

#endif  // SANDBOX_SRC_RESTRICTED_TOKEN_UTILS_H__
