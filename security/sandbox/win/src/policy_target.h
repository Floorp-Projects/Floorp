// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox_types.h"

#ifndef SANDBOX_SRC_POLICY_TARGET_H__
#define SANDBOX_SRC_POLICY_TARGET_H__

namespace sandbox {

struct CountedParameterSetBase;

// Performs a policy lookup and returns true if the request should be passed to
// the broker process.
bool QueryBroker(int ipc_id, CountedParameterSetBase* params);

extern "C" {

// Interception of NtSetInformationThread on the child process.
// It should never be called directly.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtSetInformationThread(
    NtSetInformationThreadFunction orig_SetInformationThread, HANDLE thread,
    NT_THREAD_INFORMATION_CLASS thread_info_class, PVOID thread_information,
    ULONG thread_information_bytes);

// Interception of NtOpenThreadToken on the child process.
// It should never be called directly
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtOpenThreadToken(
    NtOpenThreadTokenFunction orig_OpenThreadToken, HANDLE thread,
    ACCESS_MASK desired_access, BOOLEAN open_as_self, PHANDLE token);

// Interception of NtOpenThreadTokenEx on the child process.
// It should never be called directly
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtOpenThreadTokenEx(
    NtOpenThreadTokenExFunction orig_OpenThreadTokenEx, HANDLE thread,
    ACCESS_MASK desired_access, BOOLEAN open_as_self, ULONG handle_attributes,
    PHANDLE token);

}  // extern "C"

}  // namespace sandbox

#endif  // SANDBOX_SRC_POLICY_TARGET_H__
