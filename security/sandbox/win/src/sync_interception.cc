// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/sync_interception.h"

#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/policy_target.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/sharedmem_ipc_client.h"
#include "sandbox/win/src/target_services.h"

namespace sandbox {

HANDLE WINAPI TargetCreateEventW(CreateEventWFunction orig_CreateEvent,
                                 LPSECURITY_ATTRIBUTES security_attributes,
                                 BOOL manual_reset, BOOL initial_state,
                                 LPCWSTR name) {
  // Check if the process can create it first.
  HANDLE handle = orig_CreateEvent(security_attributes, manual_reset,
                                   initial_state, name);
  DWORD original_error = ::GetLastError();
  if (NULL != handle)
    return handle;

  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return NULL;

  do {
    if (security_attributes)
      break;

    void* memory = GetGlobalIPCMemory();
    if (NULL == memory)
      break;

    CountedParameterSet<NameBased> params;
    params[NameBased::NAME] = ParamPickerMake(name);

    if (!QueryBroker(IPC_CREATEEVENT_TAG, params.GetBase()))
      break;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    ResultCode code = CrossCall(ipc, IPC_CREATEEVENT_TAG, name, manual_reset,
                                initial_state, &answer);

    if (SBOX_ALL_OK != code)
      break;

    ::SetLastError(answer.win32_result);
    return answer.handle;
  } while (false);

  ::SetLastError(original_error);
  return NULL;
}

// Interception of OpenEventW on the child process.
// It should never be called directly
HANDLE WINAPI TargetOpenEventW(OpenEventWFunction orig_OpenEvent,
                               ACCESS_MASK desired_access, BOOL inherit_handle,
                               LPCWSTR name) {
  // Check if the process can open it first.
  HANDLE handle = orig_OpenEvent(desired_access, inherit_handle, name);
  DWORD original_error = ::GetLastError();
  if (NULL != handle)
    return handle;

  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return NULL;

  do {
    void* memory = GetGlobalIPCMemory();
    if (NULL == memory)
      break;

    uint32 inherit_handle_ipc = inherit_handle;
    CountedParameterSet<OpenEventParams> params;
    params[OpenEventParams::NAME] = ParamPickerMake(name);
    params[OpenEventParams::ACCESS] = ParamPickerMake(desired_access);

    if (!QueryBroker(IPC_OPENEVENT_TAG, params.GetBase()))
      break;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    ResultCode code = CrossCall(ipc, IPC_OPENEVENT_TAG, name, desired_access,
                                inherit_handle_ipc, &answer);

    if (SBOX_ALL_OK != code)
      break;

    ::SetLastError(answer.win32_result);
    return answer.handle;
  } while (false);

  ::SetLastError(original_error);
  return NULL;
}

}  // namespace sandbox
