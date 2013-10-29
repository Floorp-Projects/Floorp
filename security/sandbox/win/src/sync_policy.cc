// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "sandbox/win/src/sync_policy.h"

#include "base/logging.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_engine_opcodes.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/sandbox_types.h"
#include "sandbox/win/src/sandbox_utils.h"

namespace sandbox {

bool SyncPolicy::GenerateRules(const wchar_t* name,
                               TargetPolicy::Semantics semantics,
                               LowLevelPolicy* policy) {
  std::wstring mod_name(name);
  if (mod_name.empty()) {
    return false;
  }

  if (TargetPolicy::EVENTS_ALLOW_ANY != semantics &&
      TargetPolicy::EVENTS_ALLOW_READONLY != semantics) {
    // Other flags are not valid for sync policy yet.
    NOTREACHED();
    return false;
  }

  // Add the open rule.
  EvalResult result = ASK_BROKER;
  PolicyRule open(result);

  if (!open.AddStringMatch(IF, OpenEventParams::NAME, name, CASE_INSENSITIVE))
    return false;

  if (TargetPolicy::EVENTS_ALLOW_READONLY == semantics) {
    // We consider all flags that are not known to be readonly as potentially
    // used for write.
    DWORD allowed_flags = SYNCHRONIZE | GENERIC_READ | READ_CONTROL;
    DWORD restricted_flags = ~allowed_flags;
    open.AddNumberMatch(IF_NOT, OpenEventParams::ACCESS, restricted_flags, AND);
  }

  if (!policy->AddRule(IPC_OPENEVENT_TAG, &open))
    return false;

  // If it's not a read only, add the create rule.
  if (TargetPolicy::EVENTS_ALLOW_READONLY != semantics) {
    PolicyRule create(result);
    if (!create.AddStringMatch(IF, NameBased::NAME, name, CASE_INSENSITIVE))
      return false;

    if (!policy->AddRule(IPC_CREATEEVENT_TAG, &create))
      return false;
  }

  return true;
}

DWORD SyncPolicy::CreateEventAction(EvalResult eval_result,
                                    const ClientInfo& client_info,
                                    const std::wstring &event_name,
                                    uint32 manual_reset,
                                    uint32 initial_state,
                                    HANDLE *handle) {
  // The only action supported is ASK_BROKER which means create the requested
  // file as specified.
  if (ASK_BROKER != eval_result)
    return false;

  HANDLE local_handle = ::CreateEvent(NULL, manual_reset, initial_state,
                                     event_name.c_str());
  if (NULL == local_handle)
    return ::GetLastError();

  if (!::DuplicateHandle(::GetCurrentProcess(), local_handle,
                         client_info.process, handle, 0, FALSE,
                         DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
    return ERROR_ACCESS_DENIED;
  }
  return ERROR_SUCCESS;
}

DWORD SyncPolicy::OpenEventAction(EvalResult eval_result,
                                  const ClientInfo& client_info,
                                  const std::wstring &event_name,
                                  uint32 desired_access,
                                  uint32 inherit_handle,
                                  HANDLE *handle) {
  // The only action supported is ASK_BROKER which means create the requested
  // file as specified.
  if (ASK_BROKER != eval_result)
    return false;

  HANDLE local_handle = ::OpenEvent(desired_access, FALSE,
                                    event_name.c_str());
  if (NULL == local_handle)
    return ::GetLastError();

  if (!::DuplicateHandle(::GetCurrentProcess(), local_handle,
                         client_info.process, handle, 0, inherit_handle,
                         DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
    return ERROR_ACCESS_DENIED;
  }
  return ERROR_SUCCESS;
}

}  // namespace sandbox
