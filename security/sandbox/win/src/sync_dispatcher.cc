// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/sync_dispatcher.h"

#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/interceptors.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_broker.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sync_interception.h"
#include "sandbox/win/src/sync_policy.h"

namespace sandbox {

SyncDispatcher::SyncDispatcher(PolicyBase* policy_base)
    : policy_base_(policy_base) {
  static const IPCCall create_params = {
    {IPC_CREATEEVENT_TAG, WCHAR_TYPE, ULONG_TYPE, ULONG_TYPE},
    reinterpret_cast<CallbackGeneric>(&SyncDispatcher::CreateEvent)
  };

  static const IPCCall open_params = {
    {IPC_OPENEVENT_TAG, WCHAR_TYPE, ULONG_TYPE, ULONG_TYPE},
    reinterpret_cast<CallbackGeneric>(&SyncDispatcher::OpenEvent)
  };

  ipc_calls_.push_back(create_params);
  ipc_calls_.push_back(open_params);
}

bool SyncDispatcher::SetupService(InterceptionManager* manager,
                                  int service) {
  if (IPC_CREATEEVENT_TAG == service)
      return INTERCEPT_EAT(manager, L"kernel32.dll", CreateEventW,
                           CREATE_EVENT_ID, 20);

  if (IPC_OPENEVENT_TAG == service)
    return INTERCEPT_EAT(manager, L"kernel32.dll", OpenEventW,
                         OPEN_EVENT_ID, 16);

  return false;
}

bool SyncDispatcher::CreateEvent(IPCInfo* ipc, std::wstring* name,
                                 DWORD manual_reset, DWORD initial_state) {
  const wchar_t* event_name = name->c_str();
  CountedParameterSet<NameBased> params;
  params[NameBased::NAME] = ParamPickerMake(event_name);

  EvalResult result = policy_base_->EvalPolicy(IPC_CREATEEVENT_TAG,
                                               params.GetBase());
  HANDLE handle = NULL;
  DWORD ret = SyncPolicy::CreateEventAction(result, *ipc->client_info, *name,
                                            manual_reset, initial_state,
                                            &handle);
  // Return operation status on the IPC.
  ipc->return_info.win32_result = ret;
  ipc->return_info.handle = handle;
  return true;
}

bool SyncDispatcher::OpenEvent(IPCInfo* ipc, std::wstring* name,
                               DWORD desired_access, DWORD inherit_handle) {
  const wchar_t* event_name = name->c_str();

  CountedParameterSet<OpenEventParams> params;
  params[OpenEventParams::NAME] = ParamPickerMake(event_name);
  params[OpenEventParams::ACCESS] = ParamPickerMake(desired_access);

  EvalResult result = policy_base_->EvalPolicy(IPC_OPENEVENT_TAG,
                                               params.GetBase());
  HANDLE handle = NULL;
  DWORD ret = SyncPolicy::OpenEventAction(result, *ipc->client_info, *name,
                                          desired_access, inherit_handle,
                                          &handle);
  // Return operation status on the IPC.
  ipc->return_info.win32_result = ret;
  ipc->return_info.handle = handle;
  return true;
}

}  // namespace sandbox
