// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/acl.h"

#include <aclapi.h>
#include <sddl.h>

#include "base/logging.h"

namespace sandbox {

bool GetDefaultDacl(HANDLE token,
                    scoped_ptr_malloc<TOKEN_DEFAULT_DACL>* default_dacl) {
  if (token == NULL)
    return false;

  DCHECK(default_dacl != NULL);

  unsigned long length = 0;
  ::GetTokenInformation(token, TokenDefaultDacl, NULL, 0, &length);
  if (length == 0) {
    NOTREACHED();
    return false;
  }

  TOKEN_DEFAULT_DACL* acl =
      reinterpret_cast<TOKEN_DEFAULT_DACL*>(malloc(length));
  default_dacl->reset(acl);

  if (!::GetTokenInformation(token, TokenDefaultDacl, default_dacl->get(),
                             length, &length))
      return false;

  return true;
}

bool AddSidToDacl(const Sid& sid, ACL* old_dacl, ACCESS_MASK access,
                  ACL** new_dacl) {
  EXPLICIT_ACCESS new_access = {0};
  new_access.grfAccessMode = GRANT_ACCESS;
  new_access.grfAccessPermissions = access;
  new_access.grfInheritance = NO_INHERITANCE;

  new_access.Trustee.pMultipleTrustee = NULL;
  new_access.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
  new_access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
  new_access.Trustee.ptstrName = reinterpret_cast<LPWSTR>(
                                    const_cast<SID*>(sid.GetPSID()));

  if (ERROR_SUCCESS != ::SetEntriesInAcl(1, &new_access, old_dacl, new_dacl))
    return false;

  return true;
}

bool AddSidToDefaultDacl(HANDLE token, const Sid& sid, ACCESS_MASK access) {
  if (token == NULL)
    return false;

  scoped_ptr_malloc<TOKEN_DEFAULT_DACL> default_dacl;
  if (!GetDefaultDacl(token, &default_dacl))
    return false;

  ACL* new_dacl = NULL;
  if (!AddSidToDacl(sid, default_dacl->DefaultDacl, access, &new_dacl))
    return false;

  TOKEN_DEFAULT_DACL new_token_dacl = {0};
  new_token_dacl.DefaultDacl = new_dacl;

  BOOL ret = ::SetTokenInformation(token, TokenDefaultDacl, &new_token_dacl,
                                   sizeof(new_token_dacl));
  ::LocalFree(new_dacl);
  return (TRUE == ret);
}

bool AddUserSidToDefaultDacl(HANDLE token, ACCESS_MASK access) {
  DWORD size = sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE;
  TOKEN_USER* token_user = reinterpret_cast<TOKEN_USER*>(malloc(size));

  scoped_ptr_malloc<TOKEN_USER> token_user_ptr(token_user);

  if (!::GetTokenInformation(token, TokenUser, token_user, size, &size))
    return false;

  return AddSidToDefaultDacl(token,
                             reinterpret_cast<SID*>(token_user->User.Sid),
                             access);
}

bool AddKnownSidToKernelObject(HANDLE object, const Sid& sid,
                               ACCESS_MASK access) {
  PSECURITY_DESCRIPTOR descriptor = NULL;
  PACL old_dacl = NULL;
  PACL new_dacl = NULL;

  if (ERROR_SUCCESS != ::GetSecurityInfo(object, SE_KERNEL_OBJECT,
                                         DACL_SECURITY_INFORMATION, NULL, NULL,
                                         &old_dacl, NULL, &descriptor))
    return false;

  if (!AddSidToDacl(sid.GetPSID(), old_dacl, access, &new_dacl)) {
    ::LocalFree(descriptor);
    return false;
  }

  DWORD result = ::SetSecurityInfo(object, SE_KERNEL_OBJECT,
                                   DACL_SECURITY_INFORMATION, NULL, NULL,
                                   new_dacl, NULL);

  ::LocalFree(new_dacl);
  ::LocalFree(descriptor);

  if (ERROR_SUCCESS != result)
    return false;

  return true;
}

}  // namespace sandbox
