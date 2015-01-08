// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/wow_helper/target_code.h"

namespace sandbox {

// Hooks NtMapViewOfSection to detect the load of dlls.
#pragma code_seg(push, code, ".TargetCode$A")
NTSTATUS WINAPI TargetNtMapViewOfSection(
    PatchInfo *patch_info, HANDLE process, PVOID *base, ULONG_PTR zero_bits,
    SIZE_T commit_size, PLARGE_INTEGER offset, PSIZE_T view_size,
    SECTION_INHERIT inherit, ULONG allocation_type, ULONG protect) {
  NTSTATUS ret = patch_info->orig_MapViewOfSection(patch_info->section, process,
                                                   base, zero_bits, commit_size,
                                                   offset, view_size, inherit,
                                                   allocation_type, protect);

  LARGE_INTEGER timeout;
  timeout.QuadPart = -(5 * 10000000);  // 5 seconds.

  // The wait is alertable.
  patch_info->signal_and_wait(patch_info->dll_load, patch_info->continue_load,
                              TRUE, &timeout);

  return ret;
}
#pragma code_seg(pop, code)

// Marks the end of the code to copy to the target process.
#pragma code_seg(push, code, ".TargetCode$B")
NTSTATUS WINAPI TargetEnd() {
  return STATUS_SUCCESS;
}
#pragma code_seg(pop, code)

}  // namespace sandbox
