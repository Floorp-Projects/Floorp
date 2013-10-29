// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox_types.h"

#ifndef SANDBOX_SRC_INTERCEPTORS_64_H_
#define SANDBOX_SRC_INTERCEPTORS_64_H_

namespace sandbox {

extern "C" {

// Interception of NtMapViewOfSection on the child process.
// It should never be called directly. This function provides the means to
// detect dlls being loaded, so we can patch them if needed.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtMapViewOfSection64(
    HANDLE section, HANDLE process, PVOID *base, ULONG_PTR zero_bits,
    SIZE_T commit_size, PLARGE_INTEGER offset, PSIZE_T view_size,
    SECTION_INHERIT inherit, ULONG allocation_type, ULONG protect);

// Interception of NtUnmapViewOfSection on the child process.
// It should never be called directly. This function provides the means to
// detect dlls being unloaded, so we can clean up our interceptions.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtUnmapViewOfSection64(HANDLE process,
                                                               PVOID base);

// -----------------------------------------------------------------------
// Interceptors without IPC.

// Interception of NtSetInformationThread on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtSetInformationThread64(
    HANDLE thread, NT_THREAD_INFORMATION_CLASS thread_info_class,
    PVOID thread_information, ULONG thread_information_bytes);

// Interception of NtOpenThreadToken on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtOpenThreadToken64(
    HANDLE thread, ACCESS_MASK desired_access, BOOLEAN open_as_self,
    PHANDLE token);

// Interception of NtOpenThreadTokenEx on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtOpenThreadTokenEx64(
    HANDLE thread, ACCESS_MASK desired_access, BOOLEAN open_as_self,
    ULONG handle_attributes, PHANDLE token);

// Interception of CreateThread on the child process.
SANDBOX_INTERCEPT HANDLE WINAPI TargetCreateThread64(
    LPSECURITY_ATTRIBUTES thread_attributes, SIZE_T stack_size,
    LPTHREAD_START_ROUTINE start_address, PVOID parameter,
    DWORD creation_flags, LPDWORD thread_id);

// Interception of GetUserDefaultLCID on the child process.
SANDBOX_INTERCEPT LCID WINAPI TargetGetUserDefaultLCID64();

// -----------------------------------------------------------------------
// Interceptors handled by the file system dispatcher.

// Interception of NtCreateFile on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtCreateFile64(
    PHANDLE file, ACCESS_MASK desired_access,
    POBJECT_ATTRIBUTES object_attributes, PIO_STATUS_BLOCK io_status,
    PLARGE_INTEGER allocation_size, ULONG file_attributes, ULONG sharing,
    ULONG disposition, ULONG options, PVOID ea_buffer, ULONG ea_length);

// Interception of NtOpenFile on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtOpenFile64(
    PHANDLE file, ACCESS_MASK desired_access,
    POBJECT_ATTRIBUTES object_attributes, PIO_STATUS_BLOCK io_status,
    ULONG sharing, ULONG options);

// Interception of NtQueryAtttributesFile on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtQueryAttributesFile64(
    POBJECT_ATTRIBUTES object_attributes,
    PFILE_BASIC_INFORMATION file_attributes);

// Interception of NtQueryFullAtttributesFile on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtQueryFullAttributesFile64(
    POBJECT_ATTRIBUTES object_attributes,
    PFILE_NETWORK_OPEN_INFORMATION file_attributes);

// Interception of NtSetInformationFile on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtSetInformationFile64(
    HANDLE file, PIO_STATUS_BLOCK io_status, PVOID file_information,
    ULONG length, FILE_INFORMATION_CLASS file_information_class);

// -----------------------------------------------------------------------
// Interceptors handled by the named pipe dispatcher.

// Interception of CreateNamedPipeW in kernel32.dll
SANDBOX_INTERCEPT HANDLE WINAPI TargetCreateNamedPipeW64(
    LPCWSTR pipe_name, DWORD open_mode, DWORD pipe_mode, DWORD max_instance,
    DWORD out_buffer_size, DWORD in_buffer_size, DWORD default_timeout,
    LPSECURITY_ATTRIBUTES security_attributes);

// -----------------------------------------------------------------------
// Interceptors handled by the process-thread dispatcher.

// Interception of NtOpenThread on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtOpenThread64(
    PHANDLE thread, ACCESS_MASK desired_access,
    POBJECT_ATTRIBUTES object_attributes, PCLIENT_ID client_id);

// Interception of NtOpenProcess on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtOpenProcess64(
    PHANDLE process, ACCESS_MASK desired_access,
    POBJECT_ATTRIBUTES object_attributes, PCLIENT_ID client_id);

// Interception of NtOpenProcessToken on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtOpenProcessToken64(
    HANDLE process, ACCESS_MASK desired_access, PHANDLE token);

// Interception of NtOpenProcessTokenEx on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtOpenProcessTokenEx64(
    HANDLE process, ACCESS_MASK desired_access, ULONG handle_attributes,
    PHANDLE token);

// Interception of CreateProcessW in kernel32.dll.
SANDBOX_INTERCEPT BOOL WINAPI TargetCreateProcessW64(
    LPCWSTR application_name, LPWSTR command_line,
    LPSECURITY_ATTRIBUTES process_attributes,
    LPSECURITY_ATTRIBUTES thread_attributes, BOOL inherit_handles, DWORD flags,
    LPVOID environment, LPCWSTR current_directory, LPSTARTUPINFOW startup_info,
    LPPROCESS_INFORMATION process_information);

// Interception of CreateProcessA in kernel32.dll.
SANDBOX_INTERCEPT BOOL WINAPI TargetCreateProcessA64(
    LPCSTR application_name, LPSTR command_line,
    LPSECURITY_ATTRIBUTES process_attributes,
    LPSECURITY_ATTRIBUTES thread_attributes, BOOL inherit_handles, DWORD flags,
    LPVOID environment, LPCSTR current_directory, LPSTARTUPINFOA startup_info,
    LPPROCESS_INFORMATION process_information);

// -----------------------------------------------------------------------
// Interceptors handled by the registry dispatcher.

// Interception of NtCreateKey on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtCreateKey64(
    PHANDLE key, ACCESS_MASK desired_access,
    POBJECT_ATTRIBUTES object_attributes, ULONG title_index,
    PUNICODE_STRING class_name, ULONG create_options, PULONG disposition);

// Interception of NtOpenKey on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtOpenKey64(
    PHANDLE key, ACCESS_MASK desired_access,
    POBJECT_ATTRIBUTES object_attributes);

// Interception of NtOpenKeyEx on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtOpenKeyEx64(
    PHANDLE key, ACCESS_MASK desired_access,
    POBJECT_ATTRIBUTES object_attributes, ULONG open_options);

// -----------------------------------------------------------------------
// Interceptors handled by the sync dispatcher.

// Interception of CreateEventW on the child process.
SANDBOX_INTERCEPT HANDLE WINAPI TargetCreateEventW64(
    LPSECURITY_ATTRIBUTES security_attributes, BOOL manual_reset,
    BOOL initial_state, LPCWSTR name);

// Interception of OpenEventW on the child process.
SANDBOX_INTERCEPT HANDLE WINAPI TargetOpenEventW64(
    ACCESS_MASK desired_access, BOOL inherit_handle, LPCWSTR name);

}  // extern "C"

}  // namespace sandbox

#endif  // SANDBOX_SRC_INTERCEPTORS_64_H_
