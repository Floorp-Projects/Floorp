// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/handle_dispatcher.h"

#include "base/win/scoped_handle.h"
#include "sandbox/win/src/handle_interception.h"
#include "sandbox/win/src/handle_policy.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_broker.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/sandbox_types.h"
#include "sandbox/win/src/sandbox_utils.h"

namespace sandbox {

HandleDispatcher::HandleDispatcher(PolicyBase* policy_base)
    : policy_base_(policy_base) {
  static const IPCCall duplicate_handle_proxy = {
    {IPC_DUPLICATEHANDLEPROXY_TAG, VOIDPTR_TYPE, ULONG_TYPE, ULONG_TYPE,
     ULONG_TYPE},
    reinterpret_cast<CallbackGeneric>(&HandleDispatcher::DuplicateHandleProxy)
  };

  ipc_calls_.push_back(duplicate_handle_proxy);
}

bool HandleDispatcher::SetupService(InterceptionManager* manager,
                                    int service) {
  // We perform no interceptions for handles right now.
  switch (service) {
    case IPC_DUPLICATEHANDLEPROXY_TAG:
    return true;
  }

  return false;
}

bool HandleDispatcher::DuplicateHandleProxy(IPCInfo* ipc,
                                            HANDLE source_handle,
                                            DWORD target_process_id,
                                            DWORD desired_access,
                                            DWORD options) {
  static NtQueryObject QueryObject = NULL;
  if (!QueryObject)
    ResolveNTFunctionPtr("NtQueryObject", &QueryObject);

  // Get a copy of the handle for use in the broker process.
  HANDLE handle_temp;
  if (!::DuplicateHandle(ipc->client_info->process, source_handle,
                         ::GetCurrentProcess(), &handle_temp,
                         0, FALSE, DUPLICATE_SAME_ACCESS | options)) {
    ipc->return_info.win32_result = ::GetLastError();
    return false;
  }
  options &= ~DUPLICATE_CLOSE_SOURCE;
  base::win::ScopedHandle handle(handle_temp);

  // Get the object type (32 characters is safe; current max is 14).
  BYTE buffer[sizeof(OBJECT_TYPE_INFORMATION) + 32 * sizeof(wchar_t)];
  OBJECT_TYPE_INFORMATION* type_info =
      reinterpret_cast<OBJECT_TYPE_INFORMATION*>(buffer);
  ULONG size = sizeof(buffer) - sizeof(wchar_t);
  NTSTATUS error =
      QueryObject(handle, ObjectTypeInformation, type_info, size, &size);
  if (!NT_SUCCESS(error)) {
    ipc->return_info.nt_status = error;
    return false;
  }
  type_info->Name.Buffer[type_info->Name.Length / sizeof(wchar_t)] = L'\0';

  CountedParameterSet<HandleTarget> params;
  params[HandleTarget::NAME] = ParamPickerMake(type_info->Name.Buffer);
  params[HandleTarget::TARGET] = ParamPickerMake(target_process_id);

  EvalResult eval = policy_base_->EvalPolicy(IPC_DUPLICATEHANDLEPROXY_TAG,
                                             params.GetBase());
  ipc->return_info.win32_result =
      HandlePolicy::DuplicateHandleProxyAction(eval, handle,
                                               target_process_id,
                                               &ipc->return_info.handle,
                                               desired_access, options);
  return true;
}

}  // namespace sandbox

