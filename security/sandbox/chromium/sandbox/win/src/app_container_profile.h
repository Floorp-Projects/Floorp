// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_APP_CONTAINER_PROFILE_H_
#define SANDBOX_SRC_APP_CONTAINER_PROFILE_H_

#include <windows.h>

#include <accctrl.h>

#include "base/files/file_path.h"
#include "base/win/scoped_handle.h"
#include "sandbox/win/src/sid.h"

namespace sandbox {

class AppContainerProfile {
 public:
  // Increments the reference count of this object. The reference count must
  // be incremented if this interface is given to another component.
  virtual void AddRef() = 0;

  // Decrements the reference count of this object. When the reference count
  // is zero the object is automatically destroyed.
  // Indicates that the caller is done with this interface. After calling
  // release no other method should be called.
  virtual void Release() = 0;

  // Get a handle to a registry key for this package.
  virtual bool GetRegistryLocation(REGSAM desired_access,
                                   base::win::ScopedHandle* key) = 0;

  // Get a folder path to a location for this package.
  virtual bool GetFolderPath(base::FilePath* file_path) = 0;

  // Get a pipe name usable by this AC.
  virtual bool GetPipePath(const wchar_t* pipe_name,
                           base::FilePath* pipe_path) = 0;

  // Do an access check based on this profile for a named object. If method
  // returns true then access_status reflects whether access was granted and
  // granted_access gives the final access rights. The object_type can be one of
  // SE_FILE_OBJECT, SE_REGISTRY_KEY, SE_REGISTRY_WOW64_32KEY. See
  // ::GetNamedSecurityInfo for more information about how the enumeration is
  // used and what format object_name needs to be.
  virtual bool AccessCheck(const wchar_t* object_name,
                           SE_OBJECT_TYPE object_type,
                           DWORD desired_access,
                           DWORD* granted_access,
                           BOOL* access_status) = 0;

  // Adds a capability by name to this profile.
  virtual bool AddCapability(const wchar_t* capability_name) = 0;
  // Adds a capability from a known list.
  virtual bool AddCapability(WellKnownCapabilities capability) = 0;
  // Adds a capability from a SID
  virtual bool AddCapabilitySddl(const wchar_t* sddl_sid) = 0;

  // Adds an impersonation capability by name to this profile.
  virtual bool AddImpersonationCapability(const wchar_t* capability_name) = 0;
  // Adds an impersonation capability from a known list.
  virtual bool AddImpersonationCapability(WellKnownCapabilities capability) = 0;
  // Adds an impersonation capability from a SID
  virtual bool AddImpersonationCapabilitySddl(const wchar_t* sddl_sid) = 0;

  // Enable Low Privilege AC.
  virtual void SetEnableLowPrivilegeAppContainer(bool enable) = 0;
  virtual bool GetEnableLowPrivilegeAppContainer() = 0;
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_APP_CONTAINER_PROFILE_H_
