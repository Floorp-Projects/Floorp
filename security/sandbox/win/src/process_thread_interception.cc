// Copyright (c) 2006-2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/process_thread_interception.h"

#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/policy_target.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/sharedmem_ipc_client.h"
#include "sandbox/win/src/target_services.h"

namespace sandbox {

SANDBOX_INTERCEPT NtExports g_nt;

// Hooks NtOpenThread and proxy the call to the broker if it's trying to
// open a thread in the same process.
NTSTATUS WINAPI TargetNtOpenThread(NtOpenThreadFunction orig_OpenThread,
                                   PHANDLE thread, ACCESS_MASK desired_access,
                                   POBJECT_ATTRIBUTES object_attributes,
                                   PCLIENT_ID client_id) {
  NTSTATUS status = orig_OpenThread(thread, desired_access, object_attributes,
                                    client_id);
  if (NT_SUCCESS(status))
    return status;

  do {
    if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
      break;
    if (!client_id)
      break;

    uint32 thread_id = 0;
    bool should_break = false;
    __try {
      // We support only the calls for the current process
      if (NULL != client_id->UniqueProcess)
        should_break = true;

      // Object attributes should be NULL or empty.
      if (!should_break && NULL != object_attributes) {
        if (0 != object_attributes->Attributes ||
            NULL != object_attributes->ObjectName ||
            NULL != object_attributes->RootDirectory ||
            NULL != object_attributes->SecurityDescriptor ||
            NULL != object_attributes->SecurityQualityOfService) {
          should_break = true;
        }
      }

      thread_id = static_cast<uint32>(
                      reinterpret_cast<ULONG_PTR>(client_id->UniqueThread));
    } __except(EXCEPTION_EXECUTE_HANDLER) {
      break;
    }

    if (should_break)
      break;

    if (!ValidParameter(thread, sizeof(HANDLE), WRITE))
      break;

    void* memory = GetGlobalIPCMemory();
    if (NULL == memory)
      break;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    ResultCode code = CrossCall(ipc, IPC_NTOPENTHREAD_TAG, desired_access,
                                thread_id, &answer);
    if (SBOX_ALL_OK != code)
      break;

    if (!NT_SUCCESS(answer.nt_status))
      // The nt_status here is most likely STATUS_INVALID_CID because
      // in the broker we set the process id in the CID (client ID) param
      // to be the current process. If you try to open a thread from another
      // process you will get this INVALID_CID error. On the other hand, if you
      // try to open a thread in your own process, it should return success.
      // We don't want to return STATUS_INVALID_CID here, so we return the
      // return of the original open thread status, which is most likely
      // STATUS_ACCESS_DENIED.
      break;

    __try {
      // Write the output parameters.
      *thread = answer.handle;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
      break;
    }

    return answer.nt_status;
  } while (false);

  return status;
}

// Hooks NtOpenProcess and proxy the call to the broker if it's trying to
// open the current process.
NTSTATUS WINAPI TargetNtOpenProcess(NtOpenProcessFunction orig_OpenProcess,
                                   PHANDLE process, ACCESS_MASK desired_access,
                                   POBJECT_ATTRIBUTES object_attributes,
                                   PCLIENT_ID client_id) {
  NTSTATUS status = orig_OpenProcess(process, desired_access, object_attributes,
                                     client_id);
  if (NT_SUCCESS(status))
    return status;

  do {
    if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
      break;
    if (!client_id)
      break;

    uint32 process_id = 0;
    bool should_break = false;
    __try {
      // Object attributes should be NULL or empty.
      if (!should_break && NULL != object_attributes) {
        if (0 != object_attributes->Attributes ||
            NULL != object_attributes->ObjectName ||
            NULL != object_attributes->RootDirectory ||
            NULL != object_attributes->SecurityDescriptor ||
            NULL != object_attributes->SecurityQualityOfService) {
          should_break = true;
        }
      }

      process_id = static_cast<uint32>(
                      reinterpret_cast<ULONG_PTR>(client_id->UniqueProcess));
    } __except(EXCEPTION_EXECUTE_HANDLER) {
      break;
    }

    if (should_break)
      break;

    if (!ValidParameter(process, sizeof(HANDLE), WRITE))
      break;

    void* memory = GetGlobalIPCMemory();
    if (NULL == memory)
      break;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    ResultCode code = CrossCall(ipc, IPC_NTOPENPROCESS_TAG, desired_access,
                                process_id, &answer);
    if (SBOX_ALL_OK != code)
      break;

    if (!NT_SUCCESS(answer.nt_status))
      return answer.nt_status;

    __try {
      // Write the output parameters.
      *process = answer.handle;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
      break;
    }

    return answer.nt_status;
  } while (false);

  return status;
}


NTSTATUS WINAPI TargetNtOpenProcessToken(
    NtOpenProcessTokenFunction orig_OpenProcessToken, HANDLE process,
    ACCESS_MASK desired_access, PHANDLE token) {
  NTSTATUS status = orig_OpenProcessToken(process, desired_access, token);
  if (NT_SUCCESS(status))
    return status;

  do {
    if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
      break;

    if (CURRENT_PROCESS != process)
      break;

    if (!ValidParameter(token, sizeof(HANDLE), WRITE))
      break;

    void* memory = GetGlobalIPCMemory();
    if (NULL == memory)
      break;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    ResultCode code = CrossCall(ipc, IPC_NTOPENPROCESSTOKEN_TAG, process,
                                desired_access, &answer);
    if (SBOX_ALL_OK != code)
      break;

    if (!NT_SUCCESS(answer.nt_status))
      return answer.nt_status;

    __try {
      // Write the output parameters.
      *token = answer.handle;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
      break;
    }

    return answer.nt_status;
  } while (false);

  return status;
}

NTSTATUS WINAPI TargetNtOpenProcessTokenEx(
    NtOpenProcessTokenExFunction orig_OpenProcessTokenEx, HANDLE process,
    ACCESS_MASK desired_access, ULONG handle_attributes, PHANDLE token) {
  NTSTATUS status = orig_OpenProcessTokenEx(process, desired_access,
                                            handle_attributes, token);
  if (NT_SUCCESS(status))
    return status;

  do {
    if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
      break;

    if (CURRENT_PROCESS != process)
      break;

    if (!ValidParameter(token, sizeof(HANDLE), WRITE))
      break;

    void* memory = GetGlobalIPCMemory();
    if (NULL == memory)
      break;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    ResultCode code = CrossCall(ipc, IPC_NTOPENPROCESSTOKENEX_TAG, process,
                                desired_access, handle_attributes, &answer);
    if (SBOX_ALL_OK != code)
      break;

    if (!NT_SUCCESS(answer.nt_status))
      return answer.nt_status;

    __try {
      // Write the output parameters.
      *token = answer.handle;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
      break;
    }

    return answer.nt_status;
  } while (false);

  return status;
}

BOOL WINAPI TargetCreateProcessW(CreateProcessWFunction orig_CreateProcessW,
                                 LPCWSTR application_name, LPWSTR command_line,
                                 LPSECURITY_ATTRIBUTES process_attributes,
                                 LPSECURITY_ATTRIBUTES thread_attributes,
                                 BOOL inherit_handles, DWORD flags,
                                 LPVOID environment, LPCWSTR current_directory,
                                 LPSTARTUPINFOW startup_info,
                                 LPPROCESS_INFORMATION process_information) {
  if (orig_CreateProcessW(application_name, command_line, process_attributes,
                          thread_attributes, inherit_handles, flags,
                          environment, current_directory, startup_info,
                          process_information)) {
    return TRUE;
  }
  DWORD original_error = ::GetLastError();

  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return FALSE;

  do {
    if (!ValidParameter(process_information, sizeof(PROCESS_INFORMATION),
                        WRITE))
      break;

    void* memory = GetGlobalIPCMemory();
    if (NULL == memory)
      break;

    const wchar_t* cur_dir = NULL;

    wchar_t current_directory[MAX_PATH];
    DWORD result = ::GetCurrentDirectory(MAX_PATH, current_directory);
    if (0 != result && result < MAX_PATH)
      cur_dir = current_directory;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};

    InOutCountedBuffer proc_info(process_information,
                                 sizeof(PROCESS_INFORMATION));

    ResultCode code = CrossCall(ipc, IPC_CREATEPROCESSW_TAG, application_name,
                                command_line, cur_dir, proc_info, &answer);
    if (SBOX_ALL_OK != code)
      break;

    ::SetLastError(answer.win32_result);
    if (ERROR_SUCCESS != answer.win32_result)
      return FALSE;

    return TRUE;
  } while (false);

  ::SetLastError(original_error);
  return FALSE;
}

BOOL WINAPI TargetCreateProcessA(CreateProcessAFunction orig_CreateProcessA,
                                 LPCSTR application_name, LPSTR command_line,
                                 LPSECURITY_ATTRIBUTES process_attributes,
                                 LPSECURITY_ATTRIBUTES thread_attributes,
                                 BOOL inherit_handles, DWORD flags,
                                 LPVOID environment, LPCSTR current_directory,
                                 LPSTARTUPINFOA startup_info,
                                 LPPROCESS_INFORMATION process_information) {
  if (orig_CreateProcessA(application_name, command_line, process_attributes,
                          thread_attributes, inherit_handles, flags,
                          environment, current_directory, startup_info,
                          process_information)) {
    return TRUE;
  }
  DWORD original_error = ::GetLastError();

  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return FALSE;

  do {
    if (!ValidParameter(process_information, sizeof(PROCESS_INFORMATION),
                        WRITE))
      break;

    void* memory = GetGlobalIPCMemory();
    if (NULL == memory)
      break;

    // Convert the input params to unicode.
    UNICODE_STRING *cmd_unicode = NULL;
    UNICODE_STRING *app_unicode = NULL;
    if (command_line) {
      cmd_unicode = AnsiToUnicode(command_line);
      if (!cmd_unicode)
        break;
    }

    if (application_name) {
      app_unicode = AnsiToUnicode(application_name);
      if (!app_unicode) {
        operator delete(cmd_unicode, NT_ALLOC);
        break;
      }
    }

    const wchar_t* cmd_line = cmd_unicode ? cmd_unicode->Buffer : NULL;
    const wchar_t* app_name = app_unicode ? app_unicode->Buffer : NULL;
    const wchar_t* cur_dir = NULL;

    wchar_t current_directory[MAX_PATH];
    DWORD result = ::GetCurrentDirectory(MAX_PATH, current_directory);
    if (0 != result && result < MAX_PATH)
      cur_dir = current_directory;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};

    InOutCountedBuffer proc_info(process_information,
                                 sizeof(PROCESS_INFORMATION));

    ResultCode code = CrossCall(ipc, IPC_CREATEPROCESSW_TAG, app_name,
                                cmd_line, cur_dir, proc_info, &answer);

    operator delete(cmd_unicode, NT_ALLOC);
    operator delete(app_unicode, NT_ALLOC);

    if (SBOX_ALL_OK != code)
      break;

    ::SetLastError(answer.win32_result);
    if (ERROR_SUCCESS != answer.win32_result)
      return FALSE;

    return TRUE;
  } while (false);

  ::SetLastError(original_error);
  return FALSE;
}

// Creates a thread without registering with CSRSS. This is required if we
// closed the CSRSS ALPC port after lockdown.
HANDLE WINAPI TargetCreateThread(CreateThreadFunction orig_CreateThread,
                                 LPSECURITY_ATTRIBUTES thread_attributes,
                                 SIZE_T stack_size,
                                 LPTHREAD_START_ROUTINE start_address,
                                 PVOID parameter,
                                 DWORD creation_flags,
                                 LPDWORD thread_id) {
// Try the normal CreateThread; switch to RtlCreateUserThread if needed.
  static bool use_create_thread = true;
  HANDLE thread;
  if (use_create_thread) {
    thread = orig_CreateThread(thread_attributes, stack_size, start_address,
                               parameter, creation_flags, thread_id);
    if (thread)
      return thread;
  }

  PSECURITY_DESCRIPTOR sd =
      thread_attributes ? thread_attributes->lpSecurityDescriptor : NULL;
  CLIENT_ID client_id;

  NTSTATUS result = g_nt.RtlCreateUserThread(NtCurrentProcess, sd,
                                             creation_flags & CREATE_SUSPENDED,
                                             0, stack_size, 0, start_address,
                                             parameter, &thread, &client_id);
  if (!NT_SUCCESS(result))
    return 0;

  // CSRSS is closed if we got here, so use RtlCreateUserThread from here on.
  use_create_thread = false;
  if (thread_id)
    *thread_id = HandleToUlong(client_id.UniqueThread);
  return thread;
}

// Cache the default LCID to avoid pinging CSRSS after lockdown.
// TODO(jschuh): This approach will miss a default locale changes after
// lockdown. In the future we may want to have the broker check instead.
LCID WINAPI TargetGetUserDefaultLCID(
    GetUserDefaultLCIDFunction orig_GetUserDefaultLCID) {
  static LCID default_lcid = orig_GetUserDefaultLCID();
  return default_lcid;
}

}  // namespace sandbox
