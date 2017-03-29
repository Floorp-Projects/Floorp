// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox_types.h"

#ifndef SANDBOX_SRC_PROCESS_THREAD_INTERCEPTION_H__
#define SANDBOX_SRC_PROCESS_THREAD_INTERCEPTION_H__

namespace sandbox {

extern "C" {

typedef BOOL (WINAPI *CreateProcessWFunction)(
    LPCWSTR lpApplicationName,
    LPWSTR lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    BOOL bInheritHandles,
    DWORD dwCreationFlags,
    LPVOID lpEnvironment,
    LPCWSTR lpCurrentDirectory,
    LPSTARTUPINFOW lpStartupInfo,
    LPPROCESS_INFORMATION lpProcessInformation);

typedef BOOL (WINAPI *CreateProcessAFunction)(
    LPCSTR lpApplicationName,
    LPSTR lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    BOOL bInheritHandles,
    DWORD dwCreationFlags,
    LPVOID lpEnvironment,
    LPCSTR lpCurrentDirectory,
    LPSTARTUPINFOA lpStartupInfo,
    LPPROCESS_INFORMATION lpProcessInformation);

typedef HANDLE(WINAPI* CreateThreadFunction)(
    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    SIZE_T dwStackSize,
    LPTHREAD_START_ROUTINE lpStartAddress,
    LPVOID lpParameter,
    DWORD dwCreationFlags,
    LPDWORD lpThreadId);

typedef LCID (WINAPI *GetUserDefaultLCIDFunction)();

// Interception of NtOpenThread on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtOpenThread(
    NtOpenThreadFunction orig_OpenThread, PHANDLE thread,
    ACCESS_MASK desired_access, POBJECT_ATTRIBUTES object_attributes,
    PCLIENT_ID client_id);

// Interception of NtOpenProcess on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtOpenProcess(
    NtOpenProcessFunction orig_OpenProcess, PHANDLE process,
    ACCESS_MASK desired_access, POBJECT_ATTRIBUTES object_attributes,
    PCLIENT_ID client_id);

// Interception of NtOpenProcessToken on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtOpenProcessToken(
    NtOpenProcessTokenFunction orig_OpenProcessToken, HANDLE process,
    ACCESS_MASK desired_access, PHANDLE token);

// Interception of NtOpenProcessTokenEx on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtOpenProcessTokenEx(
    NtOpenProcessTokenExFunction orig_OpenProcessTokenEx, HANDLE process,
    ACCESS_MASK desired_access, ULONG handle_attributes, PHANDLE token);

// Interception of CreateProcessW and A in kernel32.dll.
SANDBOX_INTERCEPT BOOL WINAPI TargetCreateProcessW(
    CreateProcessWFunction orig_CreateProcessW, LPCWSTR application_name,
    LPWSTR command_line, LPSECURITY_ATTRIBUTES process_attributes,
    LPSECURITY_ATTRIBUTES thread_attributes, BOOL inherit_handles, DWORD flags,
    LPVOID environment, LPCWSTR current_directory, LPSTARTUPINFOW startup_info,
    LPPROCESS_INFORMATION process_information);

SANDBOX_INTERCEPT BOOL WINAPI TargetCreateProcessA(
    CreateProcessAFunction orig_CreateProcessA, LPCSTR application_name,
    LPSTR command_line, LPSECURITY_ATTRIBUTES process_attributes,
    LPSECURITY_ATTRIBUTES thread_attributes, BOOL inherit_handles, DWORD flags,
    LPVOID environment, LPCSTR current_directory, LPSTARTUPINFOA startup_info,
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

#endif  // SANDBOX_SRC_PROCESS_THREAD_INTERCEPTION_H__
