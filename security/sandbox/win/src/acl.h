// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_ACL_H_
#define SANDBOX_SRC_ACL_H_

#include <windows.h>

#include "base/memory/scoped_ptr.h"
#include "sandbox/win/src/sid.h"

namespace sandbox {

// Returns the default dacl from the token passed in.
bool GetDefaultDacl(HANDLE token,
                    scoped_ptr_malloc<TOKEN_DEFAULT_DACL>* default_dacl);

// Appends an ACE represented by |sid| and |access| to |old_dacl|. If the
// function succeeds, new_dacl contains the new dacl and must be freed using
// LocalFree.
bool AddSidToDacl(const Sid& sid, ACL* old_dacl, ACCESS_MASK access,
                  ACL** new_dacl);

// Adds and ACE represented by |sid| and |access| to the default dacl present
// in the token.
bool AddSidToDefaultDacl(HANDLE token, const Sid& sid, ACCESS_MASK access);

// Adds an ACE represented by the user sid and |access| to the default dacl
// present in the token.
bool AddUserSidToDefaultDacl(HANDLE token, ACCESS_MASK access);

// Adds an ACE represented by |known_sid| and |access| to the dacl of the kernel
// object referenced by |object|.
bool AddKnownSidToKernelObject(HANDLE object, const Sid& sid,
                               ACCESS_MASK access);

}  // namespace sandbox


#endif  // SANDBOX_SRC_ACL_H_
