// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_PROCESS_THREAD_INTERCEPTION_H_
#define SANDBOX_WIN_SRC_PROCESS_THREAD_INTERCEPTION_H_

#include <windows.h>

#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox_types.h"

namespace sandbox {

namespace {

using CreateProcessWFunction = decltype(&::CreateProcessW);

using CreateProcessAFunction = decltype(&::CreateProcessA);

using CreateThreadFunction = decltype(&::CreateThread);

using GetUserDefaultLCIDFunction = decltype(&::GetUserDefaultLCID);

}  // namespace

extern "C" {

// Interception of NtOpenThread on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtOpenThread(NtOpenThreadFunction orig_OpenThread,
                   PHANDLE thread,
                   ACCESS_MASK desired_access,
                   POBJECT_ATTRIBUTES object_attributes,
                   PCLIENT_ID client_id);

// Interception of NtOpenProcess on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtOpenProcess(NtOpenProcessFunction orig_OpenProcess,
                    PHANDLE process,
                    ACCESS_MASK desired_access,
                    POBJECT_ATTRIBUTES object_attributes,
                    PCLIENT_ID client_id);

// Interception of NtOpenProcessToken on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtOpenProcessToken(NtOpenProcessTokenFunction orig_OpenProcessToken,
                         HANDLE process,
                         ACCESS_MASK desired_access,
                         PHANDLE token);

// Interception of NtOpenProcessTokenEx on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtOpenProcessTokenEx(NtOpenProcessTokenExFunction orig_OpenProcessTokenEx,
                           HANDLE process,
                           ACCESS_MASK desired_access,
                           ULONG handle_attributes,
                           PHANDLE token);

// Interception of CreateProcessW and A in kernel32.dll.
SANDBOX_INTERCEPT BOOL WINAPI
TargetCreateProcessW(CreateProcessWFunction orig_CreateProcessW,
                     LPCWSTR application_name,
                     LPWSTR command_line,
                     LPSECURITY_ATTRIBUTES process_attributes,
                     LPSECURITY_ATTRIBUTES thread_attributes,
                     BOOL inherit_handles,
                     DWORD flags,
                     LPVOID environment,
                     LPCWSTR current_directory,
                     LPSTARTUPINFOW startup_info,
                     LPPROCESS_INFORMATION process_information);

SANDBOX_INTERCEPT BOOL WINAPI
TargetCreateProcessA(CreateProcessAFunction orig_CreateProcessA,
                     LPCSTR application_name,
                     LPSTR command_line,
                     LPSECURITY_ATTRIBUTES process_attributes,
                     LPSECURITY_ATTRIBUTES thread_attributes,
                     BOOL inherit_handles,
                     DWORD flags,
                     LPVOID environment,
                     LPCSTR current_directory,
                     LPSTARTUPINFOA startup_info,
                     LPPROCESS_INFORMATION process_information);

// Interception of CreateThread in kernel32.dll.
SANDBOX_INTERCEPT HANDLE WINAPI
TargetCreateThread(CreateThreadFunction orig_CreateThread,
                   LPSECURITY_ATTRIBUTES thread_attributes,
                   SIZE_T stack_size,
                   LPTHREAD_START_ROUTINE start_address,
                   LPVOID parameter,
                   DWORD creation_flags,
                   LPDWORD thread_id);

}  // extern "C"

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_PROCESS_THREAD_INTERCEPTION_H_
