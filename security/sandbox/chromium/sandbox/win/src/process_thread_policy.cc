// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/process_thread_policy.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/policy_engine_opcodes.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/sandbox_types.h"
#include "sandbox/win/src/win_utils.h"

namespace {

// These are the only safe rights that can be given to a sandboxed
// process for the process created by the broker. All others are potential
// vectors of privilege elevation.
const DWORD kProcessRights = SYNCHRONIZE |
                             PROCESS_QUERY_INFORMATION |
                             PROCESS_QUERY_LIMITED_INFORMATION |
                             PROCESS_TERMINATE |
                             PROCESS_SUSPEND_RESUME;

const DWORD kThreadRights = SYNCHRONIZE |
                            THREAD_TERMINATE |
                            THREAD_SUSPEND_RESUME |
                            THREAD_QUERY_INFORMATION |
                            THREAD_QUERY_LIMITED_INFORMATION |
                            THREAD_SET_LIMITED_INFORMATION;

// Creates a child process and duplicates the handles to 'target_process'. The
// remaining parameters are the same as CreateProcess().
BOOL CreateProcessExWHelper(HANDLE target_process, BOOL give_full_access,
                            LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
                            LPSECURITY_ATTRIBUTES lpProcessAttributes,
                            LPSECURITY_ATTRIBUTES lpThreadAttributes,
                            BOOL bInheritHandles, DWORD dwCreationFlags,
                            LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory,
                            LPSTARTUPINFOW lpStartupInfo,
                            LPPROCESS_INFORMATION lpProcessInformation) {
  if (!::CreateProcessW(lpApplicationName, lpCommandLine, lpProcessAttributes,
                        lpThreadAttributes, bInheritHandles, dwCreationFlags,
                        lpEnvironment, lpCurrentDirectory, lpStartupInfo,
                        lpProcessInformation)) {
    return FALSE;
  }

  DWORD process_access = kProcessRights;
  DWORD thread_access = kThreadRights;
  if (give_full_access) {
    process_access = PROCESS_ALL_ACCESS;
    thread_access = THREAD_ALL_ACCESS;
  }
  if (!::DuplicateHandle(::GetCurrentProcess(), lpProcessInformation->hProcess,
                         target_process, &lpProcessInformation->hProcess,
                         process_access, FALSE, DUPLICATE_CLOSE_SOURCE)) {
    ::CloseHandle(lpProcessInformation->hThread);
    return FALSE;
  }
  if (!::DuplicateHandle(::GetCurrentProcess(), lpProcessInformation->hThread,
                         target_process, &lpProcessInformation->hThread,
                         thread_access, FALSE, DUPLICATE_CLOSE_SOURCE)) {
    return FALSE;
  }
  return TRUE;
}

}

namespace sandbox {

bool ProcessPolicy::GenerateRules(const wchar_t* name,
                                  TargetPolicy::Semantics semantics,
                                  LowLevelPolicy* policy) {
  scoped_ptr<PolicyRule> process;
  switch (semantics) {
    case TargetPolicy::PROCESS_MIN_EXEC: {
      process.reset(new PolicyRule(GIVE_READONLY));
      break;
    };
    case TargetPolicy::PROCESS_ALL_EXEC: {
      process.reset(new PolicyRule(GIVE_ALLACCESS));
      break;
    };
    default: {
      return false;
    };
  }

  if (!process->AddStringMatch(IF, NameBased::NAME, name, CASE_INSENSITIVE)) {
    return false;
  }
  if (!policy->AddRule(IPC_CREATEPROCESSW_TAG, process.get())) {
    return false;
  }
  return true;
}

NTSTATUS ProcessPolicy::OpenThreadAction(const ClientInfo& client_info,
                                         uint32 desired_access,
                                         uint32 thread_id,
                                         HANDLE* handle) {
  *handle = NULL;

  NtOpenThreadFunction NtOpenThread = NULL;
  ResolveNTFunctionPtr("NtOpenThread", &NtOpenThread);

  OBJECT_ATTRIBUTES attributes = {0};
  attributes.Length = sizeof(attributes);
  CLIENT_ID client_id = {0};
  client_id.UniqueProcess = reinterpret_cast<PVOID>(
                                static_cast<ULONG_PTR>(client_info.process_id));
  client_id.UniqueThread =
      reinterpret_cast<PVOID>(static_cast<ULONG_PTR>(thread_id));

  HANDLE local_handle;
  NTSTATUS status = NtOpenThread(&local_handle, desired_access, &attributes,
                                 &client_id);
  if (NT_SUCCESS(status)) {
    if (!::DuplicateHandle(::GetCurrentProcess(), local_handle,
                           client_info.process, handle, 0, FALSE,
                           DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
      return STATUS_ACCESS_DENIED;
    }
  }

  return status;
}

NTSTATUS ProcessPolicy::OpenProcessAction(const ClientInfo& client_info,
                                          uint32 desired_access,
                                          uint32 process_id,
                                          HANDLE* handle) {
  *handle = NULL;

  NtOpenProcessFunction NtOpenProcess = NULL;
  ResolveNTFunctionPtr("NtOpenProcess", &NtOpenProcess);

  if (client_info.process_id != process_id)
    return STATUS_ACCESS_DENIED;

  OBJECT_ATTRIBUTES attributes = {0};
  attributes.Length = sizeof(attributes);
  CLIENT_ID client_id = {0};
  client_id.UniqueProcess = reinterpret_cast<PVOID>(
                                static_cast<ULONG_PTR>(client_info.process_id));
  HANDLE local_handle;
  NTSTATUS status = NtOpenProcess(&local_handle, desired_access, &attributes,
                                  &client_id);
  if (NT_SUCCESS(status)) {
    if (!::DuplicateHandle(::GetCurrentProcess(), local_handle,
                           client_info.process, handle, 0, FALSE,
                           DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
      return STATUS_ACCESS_DENIED;
    }
  }

  return status;
}

NTSTATUS ProcessPolicy::OpenProcessTokenAction(const ClientInfo& client_info,
                                               HANDLE process,
                                               uint32 desired_access,
                                               HANDLE* handle) {
  *handle = NULL;
  NtOpenProcessTokenFunction NtOpenProcessToken = NULL;
  ResolveNTFunctionPtr("NtOpenProcessToken", &NtOpenProcessToken);

  if (CURRENT_PROCESS != process)
    return STATUS_ACCESS_DENIED;

  HANDLE local_handle;
  NTSTATUS status = NtOpenProcessToken(client_info.process, desired_access,
                                       &local_handle);
  if (NT_SUCCESS(status)) {
    if (!::DuplicateHandle(::GetCurrentProcess(), local_handle,
                           client_info.process, handle, 0, FALSE,
                           DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
      return STATUS_ACCESS_DENIED;
    }
  }
  return status;
}

NTSTATUS ProcessPolicy::OpenProcessTokenExAction(const ClientInfo& client_info,
                                                 HANDLE process,
                                                 uint32 desired_access,
                                                 uint32 attributes,
                                                 HANDLE* handle) {
  *handle = NULL;
  NtOpenProcessTokenExFunction NtOpenProcessTokenEx = NULL;
  ResolveNTFunctionPtr("NtOpenProcessTokenEx", &NtOpenProcessTokenEx);

  if (CURRENT_PROCESS != process)
    return STATUS_ACCESS_DENIED;

  HANDLE local_handle;
  NTSTATUS status = NtOpenProcessTokenEx(client_info.process, desired_access,
                                         attributes, &local_handle);
  if (NT_SUCCESS(status)) {
    if (!::DuplicateHandle(::GetCurrentProcess(), local_handle,
                           client_info.process, handle, 0, FALSE,
                           DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
      return STATUS_ACCESS_DENIED;
    }
  }
  return status;
}

DWORD ProcessPolicy::CreateProcessWAction(EvalResult eval_result,
                                          const ClientInfo& client_info,
                                          const base::string16 &app_name,
                                          const base::string16 &command_line,
                                          PROCESS_INFORMATION* process_info) {
  // The only action supported is ASK_BROKER which means create the process.
  if (GIVE_ALLACCESS != eval_result && GIVE_READONLY != eval_result) {
    return ERROR_ACCESS_DENIED;
  }

  STARTUPINFO startup_info = {0};
  startup_info.cb = sizeof(startup_info);
  scoped_ptr<wchar_t, base::FreeDeleter>
      cmd_line(_wcsdup(command_line.c_str()));

  BOOL should_give_full_access = (GIVE_ALLACCESS == eval_result);
  if (!CreateProcessExWHelper(client_info.process, should_give_full_access,
                              app_name.c_str(), cmd_line.get(), NULL, NULL,
                              FALSE, 0, NULL, NULL, &startup_info,
                              process_info)) {
    return ERROR_ACCESS_DENIED;
  }
  return ERROR_SUCCESS;
}

}  // namespace sandbox
