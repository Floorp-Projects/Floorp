// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/process_mitigations.h"

#include <algorithm>

#include "base/win/windows_version.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/win_utils.h"

namespace {

// Functions for enabling policies.
typedef BOOL (WINAPI *SetProcessDEPPolicyFunction)(DWORD dwFlags);

typedef BOOL (WINAPI *SetProcessMitigationPolicyFunction)(
    PROCESS_MITIGATION_POLICY mitigation_policy,
    PVOID buffer,
    SIZE_T length);

typedef BOOL (WINAPI *SetDefaultDllDirectoriesFunction)(
    DWORD DirectoryFlags);

}  // namespace

namespace sandbox {

bool ApplyProcessMitigationsToCurrentProcess(MitigationFlags flags) {
  if (!CanSetProcessMitigationsPostStartup(flags))
    return false;

  base::win::Version version = base::win::GetVersion();
  HMODULE module = ::GetModuleHandleA("kernel32.dll");

  if (version >= base::win::VERSION_VISTA &&
      (flags & MITIGATION_DLL_SEARCH_ORDER)) {
    SetDefaultDllDirectoriesFunction set_default_dll_directories =
        reinterpret_cast<SetDefaultDllDirectoriesFunction>(
            ::GetProcAddress(module, "SetDefaultDllDirectories"));

    // Check for SetDefaultDllDirectories since it requires KB2533623.
    if (set_default_dll_directories) {
      if (!set_default_dll_directories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS) &&
          ERROR_ACCESS_DENIED != ::GetLastError()) {
        return false;
      }
    }
  }

  // Set the heap to terminate on corruption
  if (version >= base::win::VERSION_VISTA &&
      (flags & MITIGATION_HEAP_TERMINATE)) {
    if (!::HeapSetInformation(NULL, HeapEnableTerminationOnCorruption,
                              NULL, 0) &&
        ERROR_ACCESS_DENIED != ::GetLastError()) {
      return false;
    }
  }

#if !defined(_WIN64)  // DEP is always enabled on 64-bit.
  if (flags & MITIGATION_DEP) {
    DWORD dep_flags = PROCESS_DEP_ENABLE;
    // DEP support is quirky on XP, so don't force a failure in that case.
    const bool return_on_fail = version >= base::win::VERSION_VISTA;

    if (flags & MITIGATION_DEP_NO_ATL_THUNK)
      dep_flags |= PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION;

    SetProcessDEPPolicyFunction set_process_dep_policy =
        reinterpret_cast<SetProcessDEPPolicyFunction>(
            ::GetProcAddress(module, "SetProcessDEPPolicy"));
    if (set_process_dep_policy) {
      if (!set_process_dep_policy(dep_flags) &&
          ERROR_ACCESS_DENIED != ::GetLastError() && return_on_fail) {
        return false;
      }
    } else {
      // We're on XP sp2, so use the less standard approach.
      // For reference: http://www.uninformed.org/?v=2&a=4
      static const int MEM_EXECUTE_OPTION_ENABLE = 1;
      static const int MEM_EXECUTE_OPTION_DISABLE = 2;
      static const int MEM_EXECUTE_OPTION_ATL7_THUNK_EMULATION = 4;
      static const int MEM_EXECUTE_OPTION_PERMANENT = 8;

      NtSetInformationProcessFunction set_information_process = NULL;
      ResolveNTFunctionPtr("NtSetInformationProcess",
                           &set_information_process);
      if (!set_information_process)
        return false;
      ULONG dep = MEM_EXECUTE_OPTION_DISABLE | MEM_EXECUTE_OPTION_PERMANENT;
      if (!(dep_flags & PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION))
        dep |= MEM_EXECUTE_OPTION_ATL7_THUNK_EMULATION;
      if (!SUCCEEDED(set_information_process(GetCurrentProcess(),
                                             ProcessExecuteFlags,
                                             &dep, sizeof(dep))) &&
          ERROR_ACCESS_DENIED != ::GetLastError() && return_on_fail) {
        return false;
      }
    }
  }
#endif

  // This is all we can do in Win7 and below.
  if (version < base::win::VERSION_WIN8)
    return true;

  SetProcessMitigationPolicyFunction set_process_mitigation_policy =
      reinterpret_cast<SetProcessMitigationPolicyFunction>(
          ::GetProcAddress(module, "SetProcessMitigationPolicy"));
  if (!set_process_mitigation_policy)
    return false;

  // Enable ASLR policies.
  if (flags & MITIGATION_RELOCATE_IMAGE) {
    PROCESS_MITIGATION_ASLR_POLICY policy = { 0 };
    policy.EnableForceRelocateImages = true;
    policy.DisallowStrippedImages = (flags &
        MITIGATION_RELOCATE_IMAGE_REQUIRED) ==
        MITIGATION_RELOCATE_IMAGE_REQUIRED;

    if (!set_process_mitigation_policy(ProcessASLRPolicy, &policy,
                                       sizeof(policy)) &&
        ERROR_ACCESS_DENIED != ::GetLastError()) {
      return false;
    }
  }

  // Enable strict handle policies.
  if (flags & MITIGATION_STRICT_HANDLE_CHECKS) {
    PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY policy = { 0 };
    policy.HandleExceptionsPermanentlyEnabled =
        policy.RaiseExceptionOnInvalidHandleReference = true;

    if (!set_process_mitigation_policy(ProcessStrictHandleCheckPolicy, &policy,
                                       sizeof(policy)) &&
        ERROR_ACCESS_DENIED != ::GetLastError()) {
      return false;
    }
  }

  // Enable system call policies.
  if (flags & MITIGATION_WIN32K_DISABLE) {
    PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY policy = { 0 };
    policy.DisallowWin32kSystemCalls = true;

    if (!set_process_mitigation_policy(ProcessSystemCallDisablePolicy, &policy,
                                       sizeof(policy)) &&
        ERROR_ACCESS_DENIED != ::GetLastError()) {
      return false;
    }
  }

  // Enable system call policies.
  if (flags & MITIGATION_EXTENSION_DLL_DISABLE) {
    PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY policy = { 0 };
    policy.DisableExtensionPoints = true;

    if (!set_process_mitigation_policy(ProcessExtensionPointDisablePolicy,
                                       &policy, sizeof(policy)) &&
        ERROR_ACCESS_DENIED != ::GetLastError()) {
      return false;
    }
  }

  return true;
}

void ConvertProcessMitigationsToPolicy(MitigationFlags flags,
                                       DWORD64* policy_flags, size_t* size) {
  base::win::Version version = base::win::GetVersion();

  *policy_flags = 0;
#if defined(_WIN64)
  *size = sizeof(*policy_flags);
#elif defined(_M_IX86)
  // A 64-bit flags attribute is illegal on 32-bit Win 7 and below.
  if (version < base::win::VERSION_WIN8)
    *size = sizeof(DWORD);
  else
    *size = sizeof(*policy_flags);
#else
#error This platform is not supported.
#endif

  // Nothing for Win XP or Vista.
  if (version <= base::win::VERSION_VISTA)
    return;

  // DEP and SEHOP are not valid for 64-bit Windows
#if !defined(_WIN64)
  if (flags & MITIGATION_DEP) {
    *policy_flags |= PROCESS_CREATION_MITIGATION_POLICY_DEP_ENABLE;
    if (!(flags & MITIGATION_DEP_NO_ATL_THUNK))
      *policy_flags |= PROCESS_CREATION_MITIGATION_POLICY_DEP_ATL_THUNK_ENABLE;
  }

  if (flags & MITIGATION_SEHOP)
    *policy_flags |= PROCESS_CREATION_MITIGATION_POLICY_SEHOP_ENABLE;
#endif

  // Win 7
  if (version < base::win::VERSION_WIN8)
    return;

  if (flags & MITIGATION_RELOCATE_IMAGE) {
    *policy_flags |=
        PROCESS_CREATION_MITIGATION_POLICY_FORCE_RELOCATE_IMAGES_ALWAYS_ON;
    if (flags & MITIGATION_RELOCATE_IMAGE_REQUIRED) {
      *policy_flags |=
          PROCESS_CREATION_MITIGATION_POLICY_FORCE_RELOCATE_IMAGES_ALWAYS_ON_REQ_RELOCS;
    }
  }

  if (flags & MITIGATION_HEAP_TERMINATE) {
    *policy_flags |=
        PROCESS_CREATION_MITIGATION_POLICY_HEAP_TERMINATE_ALWAYS_ON;
  }

  if (flags & MITIGATION_BOTTOM_UP_ASLR) {
    *policy_flags |=
        PROCESS_CREATION_MITIGATION_POLICY_BOTTOM_UP_ASLR_ALWAYS_ON;
  }

  if (flags & MITIGATION_HIGH_ENTROPY_ASLR) {
    *policy_flags |=
        PROCESS_CREATION_MITIGATION_POLICY_HIGH_ENTROPY_ASLR_ALWAYS_ON;
  }

  if (flags & MITIGATION_STRICT_HANDLE_CHECKS) {
    *policy_flags |=
        PROCESS_CREATION_MITIGATION_POLICY_STRICT_HANDLE_CHECKS_ALWAYS_ON;
  }

  if (flags & MITIGATION_WIN32K_DISABLE) {
    *policy_flags |=
        PROCESS_CREATION_MITIGATION_POLICY_WIN32K_SYSTEM_CALL_DISABLE_ALWAYS_ON;
  }

  if (flags & MITIGATION_EXTENSION_DLL_DISABLE) {
    *policy_flags |=
        PROCESS_CREATION_MITIGATION_POLICY_EXTENSION_POINT_DISABLE_ALWAYS_ON;
  }
}

MitigationFlags FilterPostStartupProcessMitigations(MitigationFlags flags) {
  base::win::Version version = base::win::GetVersion();

  // Windows XP SP2+.
  if (version < base::win::VERSION_VISTA) {
    return flags & (MITIGATION_DEP |
                    MITIGATION_DEP_NO_ATL_THUNK);
  }

  // Windows Vista
  if (version < base::win::VERSION_WIN7) {
    return flags & (MITIGATION_BOTTOM_UP_ASLR |
                    MITIGATION_DLL_SEARCH_ORDER |
                    MITIGATION_HEAP_TERMINATE);
  }

  // Windows 7.
  if (version < base::win::VERSION_WIN8) {
    return flags & (MITIGATION_BOTTOM_UP_ASLR |
                    MITIGATION_DLL_SEARCH_ORDER |
                    MITIGATION_HEAP_TERMINATE);
  }

  // Windows 8 and above.
  return flags & (MITIGATION_BOTTOM_UP_ASLR |
                  MITIGATION_DLL_SEARCH_ORDER);
}

bool ApplyProcessMitigationsToSuspendedProcess(HANDLE process,
                                               MitigationFlags flags) {
// This is a hack to fake a weak bottom-up ASLR on 32-bit Windows.
#if !defined(_WIN64)
  if (flags & MITIGATION_BOTTOM_UP_ASLR) {
    unsigned int limit;
    rand_s(&limit);
    char* ptr = 0;
    const size_t kMask64k = 0xFFFF;
    // Random range (512k-16.5mb) in 64k steps.
    const char* end = ptr + ((((limit % 16384) + 512) * 1024) & ~kMask64k);
    while (ptr < end) {
      MEMORY_BASIC_INFORMATION memory_info;
      if (!::VirtualQueryEx(process, ptr, &memory_info, sizeof(memory_info)))
        break;
      size_t size = std::min((memory_info.RegionSize + kMask64k) & ~kMask64k,
                             static_cast<SIZE_T>(end - ptr));
      if (ptr && memory_info.State == MEM_FREE)
        ::VirtualAllocEx(process, ptr, size, MEM_RESERVE, PAGE_NOACCESS);
      ptr += size;
    }
  }
#endif

  return true;
}

bool CanSetProcessMitigationsPostStartup(MitigationFlags flags) {
  // All of these mitigations can be enabled after startup.
  return !(flags & ~(MITIGATION_HEAP_TERMINATE |
                     MITIGATION_DEP |
                     MITIGATION_DEP_NO_ATL_THUNK |
                     MITIGATION_RELOCATE_IMAGE |
                     MITIGATION_RELOCATE_IMAGE_REQUIRED |
                     MITIGATION_BOTTOM_UP_ASLR |
                     MITIGATION_STRICT_HANDLE_CHECKS |
                     MITIGATION_EXTENSION_DLL_DISABLE |
                     MITIGATION_DLL_SEARCH_ORDER));
}

bool CanSetProcessMitigationsPreStartup(MitigationFlags flags) {
  // These mitigations cannot be enabled prior to startup.
  return !(flags & (MITIGATION_STRICT_HANDLE_CHECKS |
                    MITIGATION_DLL_SEARCH_ORDER));
}

}  // namespace sandbox

