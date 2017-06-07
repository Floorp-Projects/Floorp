// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <aclapi.h>
#include <sddl.h>
#include <vector>

#include "sandbox/win/src/restricted_token_utils.h"

#include "base/logging.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/job.h"
#include "sandbox/win/src/restricted_token.h"
#include "sandbox/win/src/security_level.h"
#include "sandbox/win/src/sid.h"

namespace sandbox {

DWORD CreateRestrictedToken(TokenLevel security_level,
                            IntegrityLevel integrity_level,
                            TokenType token_type,
                            bool lockdown_default_dacl,
                            base::win::ScopedHandle* token) {
  RestrictedToken restricted_token;
  restricted_token.Init(NULL);  // Initialized with the current process token
  if (lockdown_default_dacl)
    restricted_token.SetLockdownDefaultDacl();

  std::vector<base::string16> privilege_exceptions;
  std::vector<Sid> sid_exceptions;
  std::vector<Sid> deny_only_sids;

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
      deny_sids = false;
      deny_only_sids.push_back(WinBuiltinAdministratorsSid);
      deny_only_sids.push_back(WinAccountAdministratorSid);
      deny_only_sids.push_back(WinAccountDomainAdminsSid);
      deny_only_sids.push_back(WinAccountCertAdminsSid);
      deny_only_sids.push_back(WinAccountSchemaAdminsSid);
      deny_only_sids.push_back(WinAccountEnterpriseAdminsSid);
      deny_only_sids.push_back(WinAccountPolicyAdminsSid);
      deny_only_sids.push_back(WinBuiltinHyperVAdminsSid);
      deny_only_sids.push_back(WinLocalAccountAndAdministratorSid);
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
      restricted_token.AddUserSidForDenyOnly();
      restricted_token.AddRestrictingSid(WinBuiltinUsersSid);
      restricted_token.AddRestrictingSid(WinWorldSid);
      restricted_token.AddRestrictingSid(WinRestrictedCodeSid);

      // This token has to be able to create objects in BNO.
      // Unfortunately, on Vista+, it needs the current logon sid
      // in the token to achieve this. You should also set the process to be
      // low integrity level so it can't access object created by other
      // processes.
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
  } else if (!deny_only_sids.empty()) {
    err_code = restricted_token.AddDenyOnlySids(deny_only_sids);
    if (ERROR_SUCCESS != err_code) {
      return err_code;
    }
  }

  if (remove_privileges) {
    err_code = restricted_token.DeleteAllPrivileges(&privilege_exceptions);
    if (ERROR_SUCCESS != err_code)
      return err_code;
  }

  restricted_token.SetIntegrityLevel(integrity_level);

  switch (token_type) {
    case PRIMARY: {
      err_code = restricted_token.GetRestrictedToken(token);
      break;
    }
    case IMPERSONATION: {
      err_code = restricted_token.GetRestrictedTokenForImpersonation(token);
      break;
    }
    default: {
      err_code = ERROR_BAD_ARGUMENTS;
      break;
    }
  }

  return err_code;
}

DWORD SetObjectIntegrityLabel(HANDLE handle, SE_OBJECT_TYPE type,
                              const wchar_t* ace_access,
                              const wchar_t* integrity_level_sid) {
  // Build the SDDL string for the label.
  base::string16 sddl = L"S:(";   // SDDL for a SACL.
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

  const wchar_t* integrity_level_str = GetIntegrityLevelString(integrity_level);
  if (!integrity_level_str) {
    // No mandatory level specified, we don't change it.
    return ERROR_SUCCESS;
  }

  PSID integrity_sid = NULL;
  if (!::ConvertStringSidToSid(integrity_level_str, &integrity_sid))
    return ::GetLastError();

  TOKEN_MANDATORY_LABEL label = {};
  label.Label.Attributes = SE_GROUP_INTEGRITY;
  label.Label.Sid = integrity_sid;

  DWORD size = sizeof(TOKEN_MANDATORY_LABEL) + ::GetLengthSid(integrity_sid);
  BOOL result = ::SetTokenInformation(token, TokenIntegrityLevel, &label,
                                      size);
  auto last_error = ::GetLastError();
  ::LocalFree(integrity_sid);

  return result ? ERROR_SUCCESS : last_error;
}

DWORD SetProcessIntegrityLevel(IntegrityLevel integrity_level) {

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

DWORD HardenTokenIntegrityLevelPolicy(HANDLE token) {

  DWORD last_error = 0;
  DWORD length_needed = 0;

  ::GetKernelObjectSecurity(token, LABEL_SECURITY_INFORMATION,
                            NULL, 0, &length_needed);

  last_error = ::GetLastError();
  if (last_error != ERROR_INSUFFICIENT_BUFFER)
    return last_error;

  std::vector<char> security_desc_buffer(length_needed);
  PSECURITY_DESCRIPTOR security_desc =
      reinterpret_cast<PSECURITY_DESCRIPTOR>(&security_desc_buffer[0]);

  if (!::GetKernelObjectSecurity(token, LABEL_SECURITY_INFORMATION,
                                 security_desc, length_needed,
                                 &length_needed))
    return ::GetLastError();

  PACL sacl = NULL;
  BOOL sacl_present = FALSE;
  BOOL sacl_defaulted = FALSE;

  if (!::GetSecurityDescriptorSacl(security_desc, &sacl_present,
                                   &sacl, &sacl_defaulted))
    return ::GetLastError();

  for (DWORD ace_index = 0; ace_index < sacl->AceCount; ++ace_index) {
    PSYSTEM_MANDATORY_LABEL_ACE ace;

    if (::GetAce(sacl, ace_index, reinterpret_cast<LPVOID*>(&ace))
        && ace->Header.AceType == SYSTEM_MANDATORY_LABEL_ACE_TYPE) {
      ace->Mask |= SYSTEM_MANDATORY_LABEL_NO_READ_UP
                |  SYSTEM_MANDATORY_LABEL_NO_EXECUTE_UP;
      break;
    }
  }

  if (!::SetKernelObjectSecurity(token, LABEL_SECURITY_INFORMATION,
                                 security_desc))
    return ::GetLastError();

  return ERROR_SUCCESS;
}

DWORD HardenProcessIntegrityLevelPolicy() {

  HANDLE token_handle;
  if (!::OpenProcessToken(GetCurrentProcess(), READ_CONTROL | WRITE_OWNER,
                          &token_handle))
    return ::GetLastError();

  base::win::ScopedHandle token(token_handle);

  return HardenTokenIntegrityLevelPolicy(token.Get());
}

}  // namespace sandbox
