// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/app_container.h"

#include <Sddl.h>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/win/startup_information.h"
#include "sandbox/win/src/internal_types.h"

namespace {

// Converts the passed in sid string to a PSID that must be relased with
// LocalFree.
PSID ConvertSid(const base::string16& sid) {
  PSID local_sid;
  if (!ConvertStringSidToSid(sid.c_str(), &local_sid))
    return NULL;
  return local_sid;
}

template <typename T>
T BindFunction(const char* name) {
  HMODULE module = GetModuleHandle(sandbox::kKerneldllName);
  void* function = GetProcAddress(module, name);
  if (!function) {
    module = GetModuleHandle(sandbox::kKernelBasedllName);
    function = GetProcAddress(module, name);
  }
  return reinterpret_cast<T>(function);
}

}  // namespace

namespace sandbox {

AppContainerAttributes::AppContainerAttributes() {
  memset(&capabilities_, 0, sizeof(capabilities_));
}

AppContainerAttributes::~AppContainerAttributes() {
  for (size_t i = 0; i < attributes_.size(); i++)
    LocalFree(attributes_[i].Sid);
  LocalFree(capabilities_.AppContainerSid);
}

ResultCode AppContainerAttributes::SetAppContainer(
    const base::string16& app_container_sid,
    const std::vector<base::string16>& capabilities) {
  DCHECK(!capabilities_.AppContainerSid);
  DCHECK(attributes_.empty());
  capabilities_.AppContainerSid = ConvertSid(app_container_sid);
  if (!capabilities_.AppContainerSid)
    return SBOX_ERROR_INVALID_APP_CONTAINER;

  for (size_t i = 0; i < capabilities.size(); i++)  {
    SID_AND_ATTRIBUTES sid_and_attributes;
    sid_and_attributes.Sid = ConvertSid(capabilities[i]);
    if (!sid_and_attributes.Sid)
      return SBOX_ERROR_INVALID_CAPABILITY;

    sid_and_attributes.Attributes = SE_GROUP_ENABLED;
    attributes_.push_back(sid_and_attributes);
  }

  if (capabilities.size()) {
    capabilities_.CapabilityCount = static_cast<DWORD>(capabilities.size());
    capabilities_.Capabilities = &attributes_[0];
  }
  return SBOX_ALL_OK;
}

ResultCode AppContainerAttributes::ShareForStartup(
      base::win::StartupInformation* startup_information) const {
  // The only thing we support so far is an AppContainer.
  if (!capabilities_.AppContainerSid)
    return SBOX_ERROR_INVALID_APP_CONTAINER;

  if (!startup_information->UpdateProcThreadAttribute(
           PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES,
           const_cast<SECURITY_CAPABILITIES*>(&capabilities_),
           sizeof(capabilities_)))  {
    DPLOG(ERROR) << "Failed UpdateProcThreadAttribute";
    return SBOX_ERROR_CANNOT_INIT_APPCONTAINER;
  }
  return SBOX_ALL_OK;
}

bool AppContainerAttributes::HasAppContainer() const {
  return (capabilities_.AppContainerSid != NULL);
}

ResultCode CreateAppContainer(const base::string16& sid,
                              const base::string16& name) {
  PSID local_sid;
  if (!ConvertStringSidToSid(sid.c_str(), &local_sid))
    return SBOX_ERROR_INVALID_APP_CONTAINER;

  typedef HRESULT (WINAPI* AppContainerRegisterSidPtr)(PSID sid,
                                                       LPCWSTR moniker,
                                                       LPCWSTR display_name);
  static AppContainerRegisterSidPtr AppContainerRegisterSid = NULL;

  if (!AppContainerRegisterSid) {
    AppContainerRegisterSid =
        BindFunction<AppContainerRegisterSidPtr>("AppContainerRegisterSid");
  }

  ResultCode operation_result = SBOX_ERROR_GENERIC;
  if (AppContainerRegisterSid) {
    HRESULT rv = AppContainerRegisterSid(local_sid, name.c_str(), name.c_str());
    if (SUCCEEDED(rv))
      operation_result = SBOX_ALL_OK;
    else
      DLOG(ERROR) << "AppContainerRegisterSid error:" << std::hex << rv;
  }
  LocalFree(local_sid);
  return operation_result;
}

ResultCode DeleteAppContainer(const base::string16& sid) {
  PSID local_sid;
  if (!ConvertStringSidToSid(sid.c_str(), &local_sid))
    return SBOX_ERROR_INVALID_APP_CONTAINER;

  typedef HRESULT (WINAPI* AppContainerUnregisterSidPtr)(PSID sid);
  static AppContainerUnregisterSidPtr AppContainerUnregisterSid = NULL;

  if (!AppContainerUnregisterSid) {
    AppContainerUnregisterSid =
        BindFunction<AppContainerUnregisterSidPtr>("AppContainerUnregisterSid");
  }

  ResultCode operation_result = SBOX_ERROR_GENERIC;
  if (AppContainerUnregisterSid) {
    HRESULT rv = AppContainerUnregisterSid(local_sid);
    if (SUCCEEDED(rv))
      operation_result = SBOX_ALL_OK;
    else
      DLOG(ERROR) << "AppContainerUnregisterSid error:" << std::hex << rv;
  }
  LocalFree(local_sid);
  return operation_result;
}

base::string16 LookupAppContainer(const base::string16& sid) {
  PSID local_sid;
  if (!ConvertStringSidToSid(sid.c_str(), &local_sid))
    return base::string16();

  typedef HRESULT (WINAPI* AppContainerLookupMonikerPtr)(PSID sid,
                                                         LPWSTR* moniker);
  typedef BOOLEAN (WINAPI* AppContainerFreeMemoryPtr)(void* ptr);

  static AppContainerLookupMonikerPtr AppContainerLookupMoniker = NULL;
  static AppContainerFreeMemoryPtr AppContainerFreeMemory = NULL;

  if (!AppContainerLookupMoniker || !AppContainerFreeMemory) {
    AppContainerLookupMoniker =
        BindFunction<AppContainerLookupMonikerPtr>("AppContainerLookupMoniker");
    AppContainerFreeMemory =
        BindFunction<AppContainerFreeMemoryPtr>("AppContainerFreeMemory");
  }

  if (!AppContainerLookupMoniker || !AppContainerFreeMemory)
    return base::string16();

  wchar_t* buffer = NULL;
  HRESULT rv = AppContainerLookupMoniker(local_sid, &buffer);
  if (FAILED(rv))
    return base::string16();

  base::string16 name(buffer);
  if (!AppContainerFreeMemory(buffer))
    NOTREACHED();
  return name;
}

}  // namespace sandbox
