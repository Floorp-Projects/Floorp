// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/named_pipe_dispatcher.h"

#include "base/basictypes.h"

#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/interceptors.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/named_pipe_interception.h"
#include "sandbox/win/src/named_pipe_policy.h"
#include "sandbox/win/src/policy_broker.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/sandbox.h"


namespace sandbox {

NamedPipeDispatcher::NamedPipeDispatcher(PolicyBase* policy_base)
    : policy_base_(policy_base) {
  static const IPCCall create_params = {
    {IPC_CREATENAMEDPIPEW_TAG, WCHAR_TYPE, ULONG_TYPE, ULONG_TYPE, ULONG_TYPE,
     ULONG_TYPE, ULONG_TYPE, ULONG_TYPE},
    reinterpret_cast<CallbackGeneric>(&NamedPipeDispatcher::CreateNamedPipe)
  };

  ipc_calls_.push_back(create_params);
}

bool NamedPipeDispatcher::SetupService(InterceptionManager* manager,
                                       int service) {
  if (IPC_CREATENAMEDPIPEW_TAG == service)
    return INTERCEPT_EAT(manager, L"kernel32.dll", CreateNamedPipeW,
                         CREATE_NAMED_PIPE_ID, 36);

  return false;
}

bool NamedPipeDispatcher::CreateNamedPipe(
    IPCInfo* ipc, std::wstring* name, DWORD open_mode, DWORD pipe_mode,
    DWORD max_instances, DWORD out_buffer_size, DWORD in_buffer_size,
    DWORD default_timeout) {
  const wchar_t* pipe_name = name->c_str();
  CountedParameterSet<NameBased> params;
  params[NameBased::NAME] = ParamPickerMake(pipe_name);

  EvalResult eval = policy_base_->EvalPolicy(IPC_CREATENAMEDPIPEW_TAG,
                                             params.GetBase());

  HANDLE pipe;
  DWORD ret = NamedPipePolicy::CreateNamedPipeAction(eval, *ipc->client_info,
                                                     *name, open_mode,
                                                     pipe_mode, max_instances,
                                                     out_buffer_size,
                                                     in_buffer_size,
                                                     default_timeout, &pipe);

  ipc->return_info.win32_result = ret;
  ipc->return_info.handle = pipe;
  return true;
}

}  // namespace sandbox
