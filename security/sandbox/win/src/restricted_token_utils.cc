// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <aclapi.h>
#include <sddl.h>
#include <vector>

#include "sandbox/win/src/restricted_token_utils.h"

#include "base/logging.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/job.h"
#include "sandbox/win/src/restricted_token.h"
#include "sandbox/win/src/security_level.h"
#include "sandbox/win/src/sid.h"

namespace sandbox {

DWORD CreateRestrictedToken(HANDLE *token_handle,
                            TokenLevel security_level,
                            IntegrityLevel integrity_level,
                            TokenType token_type) {
  if (!token_handle)
    return ERROR_BAD_ARGUMENTS;

  RestrictedToken restricted_token;
  restricted_token.Init(NULL);  // Initialized with the current process token

  std::vector<std::wstring> privilege_exceptions;
  std::vector<Sid> sid_exceptions;

  bool deny_sids = true;
  bool remove_privileges = true;

  switch (security_level) {
    case USER_UNPROTECTED: {
      deny_sids = false;
      remove_privileges = false;
      break;
    }
    case USER_RESTRICTED_SAME_ACCESS: {
      deny_sids = false;
      remove_privileges = false;

      unsigned err_code = restricted_token.AddRestrictingSidAllSids();
      if (ERROR_SUCCESS != err_code)
        return err_code;

      break;
    }
    case USER_NON_ADMIN: {
      sid_exceptions.push_back(WinBuiltinUsersSid);
      sid_exceptions.push_back(WinWorldSid);
      sid_exceptions.push_back(WinInteractiveSid);
      sid_exceptions.push_back(WinAuthenticatedUserSid);
      privilege_exceptions.push_back(SE_CHANGE_NOTIFY_NAME);
      break;
    }
    case USER_INTERACTIVE: {
      sid_exceptions.push_back(WinBuiltinUsersSid);
      sid_exceptions.push_back(WinWorldSid);
      sid_exceptions.push_back(WinInteractiveSid);
      sid_exceptions.push_back(WinAuthenticatedUserSid);
      privilege_exceptions.push_back(SE_CHANGE_NOTIFY_NAME);
      restricted_token.AddRestrictingSid(WinBuiltinUsersSid);
      restricted_token.AddRestrictingSid(WinWorldSid);
      restricted_token.AddRestrictingSid(WinRestrictedCodeSid);
      restricted_token.AddRestrictingSidCurrentUser();
      restricted_token.AddRestrictingSidLogonSession();
      break;
    }
    case USER_LIMITED: {
      sid_exceptions.push_back(WinBuiltinUsersSid);
      sid_exceptions.push_back(WinWorldSid);
      sid_exceptions.push_back(WinInteractiveSid);
      privilege_exceptions.push_back(SE_CHANGE_NOTIFY_NAME);
      restricted_token.AddRestrictingSid(WinBuiltinUsersSid);
      restricted_token.AddRestrictingSid(WinWorldSid);
      restricted_token.AddRestrictingSid(WinRestrictedCodeSid);

      // This token has to be able to create objects in BNO.
      // Unfortunately, on vista, it needs the current logon sid
      // in the token to achieve this. You should also set the process to be
      // low integrity level so it can't access object created by other
      // processes.
      if (base::win::GetVersion() >= base::win::VERSION_VISTA)
        restricted_token.AddRestrictingSidLogonSession();
      break;
    }
    case USER_RESTRICTED: {
      privilege_exceptions.push_back(SE_CHANGE_NOTIFY_NAME);
      restricted_token.AddUserSidForDenyOnly();
      restricted_token.AddRestrictingSid(WinRestrictedCodeSid);
      break;
    }
    case USER_LOCKDOWN: {
      restricted_token.AddUserSidForDenyOnly();
      restricted_token.AddRestrictingSid(WinNullSid);
      break;
    }
    default: {
      return ERROR_BAD_ARGUMENTS;
    }
  }

  DWORD err_code = ERROR_SUCCESS;
  if (deny_sids) {
    err_code = restricted_token.AddAllSidsForDenyOnly(&sid_exceptions);
    if (ERROR_SUCCESS != err_code)
      return err_code;
  }

  if (remove_privileges) {
    err_code = restricted_token.DeleteAllPrivileges(&privilege_exceptions);
    if (ERROR_SUCCESS != err_code)
      return err_code;
  }

  restricted_token.SetIntegrityLevel(integrity_level);

  switch (token_type) {
    case PRIMARY: {
      err_code = restricted_token.GetRestrictedTokenHandle(token_handle);
      break;
    }
    case IMPERSONATION: {
      err_code = restricted_token.GetRestrictedTokenHandleForImpersonation(
          token_handle);
      break;
    }
    default: {
      err_code = ERROR_BAD_ARGUMENTS;
      break;
    }
  }

  return err_code;
}

DWORD StartRestrictedProcessInJob(wchar_t *command_line,
                                  TokenLevel primary_level,
                                  TokenLevel impersonation_level,
                                  JobLevel job_level,
                                  HANDLE *const job_handle_ret) {
  Job job;
  DWORD err_code = job.Init(job_level, NULL, 0);
  if (ERROR_SUCCESS != err_code)
    return err_code;

  if (JOB_UNPROTECTED != job_level) {
    // Share the Desktop handle to be able to use MessageBox() in the sandboxed
    // application.
    err_code = job.UserHandleGrantAccess(GetDesktopWindow());
    if (ERROR_SUCCESS != err_code)
      return err_code;
  }

  // Create the primary (restricted) token for the process
  HANDLE primary_token_handle = NULL;
  err_code = CreateRestrictedToken(&primary_token_handle,
                                   primary_level,
                                   INTEGRITY_LEVEL_LAST,
                                   PRIMARY);
  if (ERROR_SUCCESS != err_code) {
    return err_code;
  }
  base::win::ScopedHandle primary_token(primary_token_handle);

  // Create the impersonation token (restricted) to be able to start the
  // process.
  HANDLE impersonation_token_handle;
  err_code = CreateRestrictedToken(&impersonation_token_handle,
                                   impersonation_level,
                                   INTEGRITY_LEVEL_LAST,
                                   IMPERSONATION);
  if (ERROR_SUCCESS != err_code) {
    return err_code;
  }
  base::win::ScopedHandle impersonation_token(impersonation_token_handle);

  // Start the process
  STARTUPINFO startup_info = {0};
  base::win::ScopedProcessInformation process_info;
  DWORD flags = CREATE_SUSPENDED;

  if (base::win::GetVersion() < base::win::VERSION_WIN8) {
    // Windows 8 implements nested jobs, but for older systems we need to
    // break out of any job we're in to enforce our restrictions.
    flags |= CREATE_BREAKAWAY_FROM_JOB;
  }

  if (!::CreateProcessAsUser(primary_token.Get(),
                             NULL,   // No application name.
                             command_line,
                             NULL,   // No security attribute.
                             NULL,   // No thread attribute.
                             FALSE,  // Do not inherit handles.
                             flags,
                             NULL,   // Use the environment of the caller.
                             NULL,   // Use current directory of the caller.
                             &startup_info,
                             process_info.Receive())) {
    return ::GetLastError();
  }

  // Change the token of the main thread of the new process for the
  // impersonation token with more rights.
  {
    HANDLE temp_thread = process_info.thread_handle();
    if (!::SetThreadToken(&temp_thread, impersonation_token.Get())) {
      ::TerminateProcess(process_info.process_handle(),
                         0);  // exit code
      return ::GetLastError();
    }
  }

  err_code = job.AssignProcessToJob(process_info.process_handle());
  if (ERROR_SUCCESS != err_code) {
    ::TerminateProcess(process_info.process_handle(),
                       0);  // exit code
    return ::GetLastError();
  }

  // Start the application
  ::ResumeThread(process_info.thread_handle());

  (*job_handle_ret) = job.Detach();

  return ERROR_SUCCESS;
}

DWORD SetObjectIntegrityLabel(HANDLE handle, SE_OBJECT_TYPE type,
                              const wchar_t* ace_access,
                              const wchar_t* integrity_level_sid) {
  // Build the SDDL string for the label.
  std::wstring sddl = L"S:(";     // SDDL for a SACL.
  sddl += SDDL_MANDATORY_LABEL;   // Ace Type is "Mandatory Label".
  sddl += L";;";                  // No Ace Flags.
  sddl += ace_access;             // Add the ACE access.
  sddl += L";;;";                 // No ObjectType and Inherited Object Type.
  sddl += integrity_level_sid;    // Trustee Sid.
  sddl += L")";

  DWORD error = ERROR_SUCCESS;
  PSECURITY_DESCRIPTOR sec_desc = NULL;

  PACL sacl = NULL;
  BOOL sacl_present = FALSE;
  BOOL sacl_defaulted = FALSE;

  if (::ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),
                                                             SDDL_REVISION,
                                                             &sec_desc, NULL)) {
    if (::GetSecurityDescriptorSacl(sec_desc, &sacl_present, &sacl,
                                    &sacl_defaulted)) {
      error = ::SetSecurityInfo(handle, type,
                                LABEL_SECURITY_INFORMATION, NULL, NULL, NULL,
                                sacl);
    } else {
      error = ::GetLastError();
    }

    ::LocalFree(sec_desc);
  } else {
    return::GetLastError();
  }

  return error;
}

const wchar_t* GetIntegrityLevelString(IntegrityLevel integrity_level) {
  switch (integrity_level) {
    case INTEGRITY_LEVEL_SYSTEM:
      return L"S-1-16-16384";
    case INTEGRITY_LEVEL_HIGH:
      return L"S-1-16-12288";
    case INTEGRITY_LEVEL_MEDIUM:
      return L"S-1-16-8192";
    case INTEGRITY_LEVEL_MEDIUM_LOW:
      return L"S-1-16-6144";
    case INTEGRITY_LEVEL_LOW:
      return L"S-1-16-4096";
    case INTEGRITY_LEVEL_BELOW_LOW:
      return L"S-1-16-2048";
    case INTEGRITY_LEVEL_UNTRUSTED:
      return L"S-1-16-0";
    case INTEGRITY_LEVEL_LAST:
      return NULL;
  }

  NOTREACHED();
  return NULL;
}
DWORD SetTokenIntegrityLevel(HANDLE token, IntegrityLevel integrity_level) {
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return ERROR_SUCCESS;

  const wchar_t* integrity_level_str = GetIntegrityLevelString(integrity_level);
  if (!integrity_level_str) {
    // No mandatory level specified, we don't change it.
    return ERROR_SUCCESS;
  }

  PSID integrity_sid = NULL;
  if (!::ConvertStringSidToSid(integrity_level_str, &integrity_sid))
    return ::GetLastError();

  TOKEN_MANDATORY_LABEL label = {0};
  label.Label.Attributes = SE_GROUP_INTEGRITY;
  label.Label.Sid = integrity_sid;

  DWORD size = sizeof(TOKEN_MANDATORY_LABEL) + ::GetLengthSid(integrity_sid);
  BOOL result = ::SetTokenInformation(token, TokenIntegrityLevel, &label,
                                      size);
  ::LocalFree(integrity_sid);

  return result ? ERROR_SUCCESS : ::GetLastError();
}

DWORD SetProcessIntegrityLevel(IntegrityLevel integrity_level) {
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return ERROR_SUCCESS;

  // We don't check for an invalid level here because we'll just let it
  // fail on the SetTokenIntegrityLevel call later on.
  if (integrity_level == INTEGRITY_LEVEL_LAST) {
    // No mandatory level specified, we don't change it.
    return ERROR_SUCCESS;
  }

  HANDLE token_handle;
  if (!::OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_DEFAULT,
                          &token_handle))
    return ::GetLastError();

  base::win::ScopedHandle token(token_handle);

  return SetTokenIntegrityLevel(token.Get(), integrity_level);
}

}  // namespace sandbox
