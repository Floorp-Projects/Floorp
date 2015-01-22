// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/target_interceptions.h"

#include "sandbox/win/src/interception_agent.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/target_services.h"

namespace sandbox {

SANDBOX_INTERCEPT NtExports g_nt;

// Hooks NtMapViewOfSection to detect the load of DLLs. If hot patching is
// required for this dll, this functions patches it.
NTSTATUS WINAPI TargetNtMapViewOfSection(
    NtMapViewOfSectionFunction orig_MapViewOfSection, HANDLE section,
    HANDLE process, PVOID *base, ULONG_PTR zero_bits, SIZE_T commit_size,
    PLARGE_INTEGER offset, PSIZE_T view_size, SECTION_INHERIT inherit,
    ULONG allocation_type, ULONG protect) {
  NTSTATUS ret = orig_MapViewOfSection(section, process, base, zero_bits,
                                       commit_size, offset, view_size, inherit,
                                       allocation_type, protect);

  static int s_load_count = 0;
  if (1 == s_load_count) {
    SandboxFactory::GetTargetServices()->GetState()->SetKernel32Loaded();
    s_load_count = 2;
  }

  do {
    if (!NT_SUCCESS(ret))
      break;

    if (!InitHeap())
      break;

    if (!IsSameProcess(process))
      break;

    if (!IsValidImageSection(section, base, offset, view_size))
      break;

    UINT image_flags;
    UNICODE_STRING* module_name =
        GetImageInfoFromModule(reinterpret_cast<HMODULE>(*base), &image_flags);
    UNICODE_STRING* file_name = GetBackingFilePath(*base);

    if ((!module_name) && (image_flags & MODULE_HAS_CODE)) {
      // If the module has no exports we retrieve the module name from the
      // full path of the mapped section.
      module_name = ExtractModuleName(file_name);
    }

    InterceptionAgent* agent = InterceptionAgent::GetInterceptionAgent();

    if (agent) {
      if (!agent->OnDllLoad(file_name, module_name, *base)) {
        // Interception agent is demanding to un-map the module.
        g_nt.UnmapViewOfSection(process, *base);
        ret = STATUS_UNSUCCESSFUL;
      }
    }

    if (module_name)
      operator delete(module_name, NT_ALLOC);

    if (file_name)
      operator delete(file_name, NT_ALLOC);

  } while (false);

  if (!s_load_count)
    s_load_count = 1;

  return ret;
}

NTSTATUS WINAPI TargetNtUnmapViewOfSection(
    NtUnmapViewOfSectionFunction orig_UnmapViewOfSection, HANDLE process,
    PVOID base) {
  NTSTATUS ret = orig_UnmapViewOfSection(process, base);

  if (!NT_SUCCESS(ret))
    return ret;

  if (!IsSameProcess(process))
    return ret;

  InterceptionAgent* agent = InterceptionAgent::GetInterceptionAgent();

  if (agent)
    agent->OnDllUnload(base);

  return ret;
}

}  // namespace sandbox
