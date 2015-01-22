// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/restricted_token.h"

#include <vector>

#include "base/logging.h"
#include "sandbox/win/src/acl.h"
#include "sandbox/win/src/win_utils.h"


namespace sandbox {

unsigned RestrictedToken::Init(const HANDLE effective_token) {
  if (init_)
    return ERROR_ALREADY_INITIALIZED;

  if (effective_token) {
    // We duplicate the handle to be able to use it even if the original handle
    // is closed.
    HANDLE effective_token_dup;
    if (::DuplicateHandle(::GetCurrentProcess(),
                          effective_token,
                          ::GetCurrentProcess(),
                          &effective_token_dup,
                          0,
                          FALSE,
                          DUPLICATE_SAME_ACCESS)) {
      effective_token_ = effective_token_dup;
    } else {
      return ::GetLastError();
    }
  } else {
    if (!::OpenProcessToken(::GetCurrentProcess(),
                            TOKEN_ALL_ACCESS,
                            &effective_token_))
      return ::GetLastError();
  }

  init_ = true;
  return ERROR_SUCCESS;
}

unsigned RestrictedToken::GetRestrictedTokenHandle(HANDLE *token_handle) const {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  size_t deny_size = sids_for_deny_only_.size();
  size_t restrict_size = sids_to_restrict_.size();
  size_t privileges_size = privileges_to_disable_.size();

  SID_AND_ATTRIBUTES *deny_only_array = NULL;
  if (deny_size) {
    deny_only_array = new SID_AND_ATTRIBUTES[deny_size];

    for (unsigned int i = 0; i < sids_for_deny_only_.size() ; ++i) {
      deny_only_array[i].Attributes = SE_GROUP_USE_FOR_DENY_ONLY;
      deny_only_array[i].Sid =
          const_cast<SID*>(sids_for_deny_only_[i].GetPSID());
    }
  }

  SID_AND_ATTRIBUTES *sids_to_restrict_array = NULL;
  if (restrict_size) {
    sids_to_restrict_array = new SID_AND_ATTRIBUTES[restrict_size];

    for (unsigned int i = 0; i < restrict_size; ++i) {
      sids_to_restrict_array[i].Attributes = 0;
      sids_to_restrict_array[i].Sid =
          const_cast<SID*>(sids_to_restrict_[i].GetPSID());
    }
  }

  LUID_AND_ATTRIBUTES *privileges_to_disable_array = NULL;
  if (privileges_size) {
    privileges_to_disable_array = new LUID_AND_ATTRIBUTES[privileges_size];

    for (unsigned int i = 0; i < privileges_size; ++i) {
      privileges_to_disable_array[i].Attributes = 0;
      privileges_to_disable_array[i].Luid = privileges_to_disable_[i];
    }
  }

  BOOL result = TRUE;
  HANDLE new_token = NULL;
  // The SANDBOX_INERT flag did nothing in XP and it was just a way to tell
  // if a token has ben restricted given the limiations of IsTokenRestricted()
  // but it appears that in Windows 7 it hints the AppLocker subsystem to
  // leave us alone.
  if (deny_size || restrict_size || privileges_size) {
    result = ::CreateRestrictedToken(effective_token_,
                                     SANDBOX_INERT,
                                     static_cast<DWORD>(deny_size),
                                     deny_only_array,
                                     static_cast<DWORD>(privileges_size),
                                     privileges_to_disable_array,
                                     static_cast<DWORD>(restrict_size),
                                     sids_to_restrict_array,
                                     &new_token);
  } else {
    // Duplicate the token even if it's not modified at this point
    // because any subsequent changes to this token would also affect the
    // current process.
    result = ::DuplicateTokenEx(effective_token_, TOKEN_ALL_ACCESS, NULL,
                                SecurityIdentification, TokenPrimary,
                                &new_token);
  }

  if (deny_only_array)
    delete[] deny_only_array;

  if (sids_to_restrict_array)
    delete[] sids_to_restrict_array;

  if (privileges_to_disable_array)
    delete[] privileges_to_disable_array;

  if (!result)
    return ::GetLastError();

  // Modify the default dacl on the token to contain Restricted and the user.
  if (!AddSidToDefaultDacl(new_token, WinRestrictedCodeSid, GENERIC_ALL))
    return ::GetLastError();

  if (!AddUserSidToDefaultDacl(new_token, GENERIC_ALL))
    return ::GetLastError();

  DWORD error = SetTokenIntegrityLevel(new_token, integrity_level_);
  if (ERROR_SUCCESS != error)
    return error;

  BOOL status = ::DuplicateHandle(::GetCurrentProcess(),
                                  new_token,
                                  ::GetCurrentProcess(),
                                  token_handle,
                                  TOKEN_ALL_ACCESS,
                                  FALSE,  // Don't inherit.
                                  0);

  if (new_token != effective_token_)
    ::CloseHandle(new_token);

  if (!status)
    return ::GetLastError();

  return ERROR_SUCCESS;
}

unsigned RestrictedToken::GetRestrictedTokenHandleForImpersonation(
    HANDLE *token_handle) const {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  HANDLE restricted_token_handle;
  unsigned err_code = GetRestrictedTokenHandle(&restricted_token_handle);
  if (ERROR_SUCCESS != err_code)
    return err_code;

  HANDLE impersonation_token;
  if (!::DuplicateToken(restricted_token_handle,
                        SecurityImpersonation,
                        &impersonation_token)) {
    ::CloseHandle(restricted_token_handle);
    return ::GetLastError();
  }

  ::CloseHandle(restricted_token_handle);

  BOOL status = ::DuplicateHandle(::GetCurrentProcess(),
                                  impersonation_token,
                                  ::GetCurrentProcess(),
                                  token_handle,
                                  TOKEN_ALL_ACCESS,
                                  FALSE,  // Don't inherit.
                                  0);

  ::CloseHandle(impersonation_token);

  if (!status)
    return ::GetLastError();

  return ERROR_SUCCESS;
}

unsigned RestrictedToken::AddAllSidsForDenyOnly(std::vector<Sid> *exceptions) {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  TOKEN_GROUPS *token_groups = NULL;
  DWORD size = 0;

  BOOL result = ::GetTokenInformation(effective_token_,
                                      TokenGroups,
                                      NULL,  // No buffer.
                                      0,  // Size is 0.
                                      &size);
  if (!size)
    return ::GetLastError();

  token_groups = reinterpret_cast<TOKEN_GROUPS*>(new BYTE[size]);
  result = ::GetTokenInformation(effective_token_,
                                 TokenGroups,
                                 token_groups,
                                 size,
                                 &size);
  if (!result) {
    delete[] reinterpret_cast<BYTE*>(token_groups);
    return ::GetLastError();
  }

  // Build the list of the deny only group SIDs
  for (unsigned int i = 0; i < token_groups->GroupCount ; ++i) {
    if ((token_groups->Groups[i].Attributes & SE_GROUP_INTEGRITY) == 0 &&
        (token_groups->Groups[i].Attributes & SE_GROUP_LOGON_ID) == 0) {
      bool should_ignore = false;
      if (exceptions) {
        for (unsigned int j = 0; j < exceptions->size(); ++j) {
          if (::EqualSid(const_cast<SID*>((*exceptions)[j].GetPSID()),
                          token_groups->Groups[i].Sid)) {
            should_ignore = true;
            break;
          }
        }
      }
      if (!should_ignore) {
        sids_for_deny_only_.push_back(
            reinterpret_cast<SID*>(token_groups->Groups[i].Sid));
      }
    }
  }

  delete[] reinterpret_cast<BYTE*>(token_groups);

  return ERROR_SUCCESS;
}

unsigned RestrictedToken::AddSidForDenyOnly(const Sid &sid) {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  sids_for_deny_only_.push_back(sid);
  return ERROR_SUCCESS;
}

unsigned RestrictedToken::AddUserSidForDenyOnly() {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  DWORD size = sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE;
  TOKEN_USER* token_user = reinterpret_cast<TOKEN_USER*>(new BYTE[size]);

  BOOL result = ::GetTokenInformation(effective_token_,
                                      TokenUser,
                                      token_user,
                                      size,
                                      &size);

  if (!result) {
    delete[] reinterpret_cast<BYTE*>(token_user);
    return ::GetLastError();
  }

  Sid user = reinterpret_cast<SID*>(token_user->User.Sid);
  sids_for_deny_only_.push_back(user);

  delete[] reinterpret_cast<BYTE*>(token_user);

  return ERROR_SUCCESS;
}

unsigned RestrictedToken::DeleteAllPrivileges(
    const std::vector<base::string16> *exceptions) {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  // Get the list of privileges in the token
  TOKEN_PRIVILEGES *token_privileges = NULL;
  DWORD size = 0;

  BOOL result = ::GetTokenInformation(effective_token_,
                                      TokenPrivileges,
                                      NULL,  // No buffer.
                                      0,  // Size is 0.
                                      &size);
  if (!size)
    return ::GetLastError();

  token_privileges = reinterpret_cast<TOKEN_PRIVILEGES*>(new BYTE[size]);
  result = ::GetTokenInformation(effective_token_,
                                 TokenPrivileges,
                                 token_privileges,
                                 size,
                                 &size);
  if (!result) {
    delete[] reinterpret_cast<BYTE *>(token_privileges);
    return ::GetLastError();
  }


  // Build the list of privileges to disable
  for (unsigned int i = 0; i < token_privileges->PrivilegeCount; ++i) {
    bool should_ignore = false;
    if (exceptions) {
      for (unsigned int j = 0; j < exceptions->size(); ++j) {
        LUID luid = {0};
        ::LookupPrivilegeValue(NULL, (*exceptions)[j].c_str(), &luid);
        if (token_privileges->Privileges[i].Luid.HighPart == luid.HighPart &&
            token_privileges->Privileges[i].Luid.LowPart == luid.LowPart) {
          should_ignore = true;
          break;
        }
      }
    }
    if (!should_ignore) {
        privileges_to_disable_.push_back(token_privileges->Privileges[i].Luid);
    }
  }

  delete[] reinterpret_cast<BYTE *>(token_privileges);

  return ERROR_SUCCESS;
}

unsigned RestrictedToken::DeletePrivilege(const wchar_t *privilege) {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  LUID luid = {0};
  if (LookupPrivilegeValue(NULL, privilege, &luid))
    privileges_to_disable_.push_back(luid);
  else
    return ::GetLastError();

  return ERROR_SUCCESS;
}

unsigned RestrictedToken::AddRestrictingSid(const Sid &sid) {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  sids_to_restrict_.push_back(sid);  // No attributes
  return ERROR_SUCCESS;
}

unsigned RestrictedToken::AddRestrictingSidLogonSession() {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  TOKEN_GROUPS *token_groups = NULL;
  DWORD size = 0;

  BOOL result = ::GetTokenInformation(effective_token_,
                                      TokenGroups,
                                      NULL,  // No buffer.
                                      0,  // Size is 0.
                                      &size);
  if (!size)
    return ::GetLastError();

  token_groups = reinterpret_cast<TOKEN_GROUPS*>(new BYTE[size]);
  result = ::GetTokenInformation(effective_token_,
                                 TokenGroups,
                                 token_groups,
                                 size,
                                 &size);
  if (!result) {
    delete[] reinterpret_cast<BYTE*>(token_groups);
    return ::GetLastError();
  }

  SID *logon_sid = NULL;
  for (unsigned int i = 0; i < token_groups->GroupCount ; ++i) {
    if ((token_groups->Groups[i].Attributes & SE_GROUP_LOGON_ID) != 0) {
        logon_sid = static_cast<SID*>(token_groups->Groups[i].Sid);
        break;
    }
  }

  if (logon_sid)
    sids_to_restrict_.push_back(logon_sid);

  delete[] reinterpret_cast<BYTE*>(token_groups);

  return ERROR_SUCCESS;
}

unsigned RestrictedToken::AddRestrictingSidCurrentUser() {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  DWORD size = sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE;
  TOKEN_USER* token_user = reinterpret_cast<TOKEN_USER*>(new BYTE[size]);

  BOOL result = ::GetTokenInformation(effective_token_,
                                      TokenUser,
                                      token_user,
                                      size,
                                      &size);

  if (!result) {
    delete[] reinterpret_cast<BYTE*>(token_user);
    return ::GetLastError();
  }

  Sid user = reinterpret_cast<SID*>(token_user->User.Sid);
  sids_to_restrict_.push_back(user);

  delete[] reinterpret_cast<BYTE*>(token_user);

  return ERROR_SUCCESS;
}

unsigned RestrictedToken::AddRestrictingSidAllSids() {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  // Add the current user to the list.
  unsigned error = AddRestrictingSidCurrentUser();
  if (ERROR_SUCCESS != error)
    return error;

  TOKEN_GROUPS *token_groups = NULL;
  DWORD size = 0;

  // Get the buffer size required.
  BOOL result = ::GetTokenInformation(effective_token_, TokenGroups, NULL, 0,
                                      &size);
  if (!size)
    return ::GetLastError();

  token_groups = reinterpret_cast<TOKEN_GROUPS*>(new BYTE[size]);
  result = ::GetTokenInformation(effective_token_,
                                 TokenGroups,
                                 token_groups,
                                 size,
                                 &size);
  if (!result) {
    delete[] reinterpret_cast<BYTE*>(token_groups);
    return ::GetLastError();
  }

  // Build the list of restricting sids from all groups.
  for (unsigned int i = 0; i < token_groups->GroupCount ; ++i) {
    if ((token_groups->Groups[i].Attributes & SE_GROUP_INTEGRITY) == 0)
      AddRestrictingSid(reinterpret_cast<SID*>(token_groups->Groups[i].Sid));
  }

  delete[] reinterpret_cast<BYTE*>(token_groups);

  return ERROR_SUCCESS;
}

unsigned RestrictedToken::SetIntegrityLevel(IntegrityLevel integrity_level) {
  integrity_level_ = integrity_level;
  return ERROR_SUCCESS;
}

}  // namespace sandbox
