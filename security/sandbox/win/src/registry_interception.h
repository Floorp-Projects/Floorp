// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox_types.h"

#ifndef SANDBOX_SRC_REGISTRY_INTERCEPTION_H__
#define SANDBOX_SRC_REGISTRY_INTERCEPTION_H__

namespace sandbox {

extern "C" {

// Interception of NtCreateKey on the child process.
// It should never be called directly
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtCreateKey(
    NtCreateKeyFunction orig_CreateKey, PHANDLE key, ACCESS_MASK desired_access,
    POBJECT_ATTRIBUTES object_attributes, ULONG title_index,
    PUNICODE_STRING class_name, ULONG create_options, PULONG disposition);

// Interception of NtOpenKey on the child process.
// It should never be called directly
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtOpenKey(
    NtOpenKeyFunction orig_OpenKey, PHANDLE key, ACCESS_MASK desired_access,
    POBJECT_ATTRIBUTES object_attributes);

// Interception of NtOpenKeyEx on the child process.
// It should never be called directly
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtOpenKeyEx(
    NtOpenKeyExFunction orig_OpenKeyEx, PHANDLE key, ACCESS_MASK desired_access,
    POBJECT_ATTRIBUTES object_attributes, ULONG open_options);

}  // extern "C"

}  // namespace sandbox

#endif  // SANDBOX_SRC_REGISTRY_INTERCEPTION_H__
