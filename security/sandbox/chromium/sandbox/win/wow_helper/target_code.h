// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WOW_HELPER_TARGET_CODE_H__
#define SANDBOX_WOW_HELPER_TARGET_CODE_H__

#include "sandbox/win/src/nt_internals.h"

namespace sandbox {

extern "C" {

// Holds the information needed for the interception of NtMapViewOfSection.
// Changes of this structure must be synchronized with changes of PatchInfo32
// on sandbox/win/src/wow64.cc.
struct PatchInfo {
  HANDLE dll_load;  // Event to signal the broker.
  HANDLE continue_load;  // Event to wait for the broker.
  HANDLE section;  // First argument of the call.
  NtMapViewOfSectionFunction orig_MapViewOfSection;
  NtSignalAndWaitForSingleObjectFunction signal_and_wait;
  void* patch_location;
};

// Interception of NtMapViewOfSection on the child process.
// It should never be called directly. This function provides the means to
// detect dlls being loaded, so we can patch them if needed.
NTSTATUS WINAPI TargetNtMapViewOfSection(
    PatchInfo* patch_info, HANDLE process, PVOID* base, ULONG_PTR zero_bits,
    SIZE_T commit_size, PLARGE_INTEGER offset, PSIZE_T view_size,
    SECTION_INHERIT inherit, ULONG allocation_type, ULONG protect);

// Marker of the end of TargetNtMapViewOfSection.
NTSTATUS WINAPI TargetEnd();

} // extern "C"

}  // namespace sandbox

#endif  // SANDBOX_WOW_HELPER_TARGET_CODE_H__
