// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/handle_policy.h"

#include <string>

#include "base/win/scoped_handle.h"
#include "sandbox/win/src/broker_services.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_engine_opcodes.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/sandbox_types.h"
#include "sandbox/win/src/sandbox_utils.h"

namespace sandbox {

bool HandlePolicy::GenerateRules(const wchar_t* type_name,
                                 TargetPolicy::Semantics semantics,
                                 LowLevelPolicy* policy) {
  PolicyRule duplicate_rule(ASK_BROKER);

  switch (semantics) {
    case TargetPolicy::HANDLES_DUP_ANY: {
      if (!duplicate_rule.AddNumberMatch(IF_NOT, HandleTarget::TARGET,
                                         ::GetCurrentProcessId(), EQUAL)) {
        return false;
      }
      break;
    }

    case TargetPolicy::HANDLES_DUP_BROKER: {
      if (!duplicate_rule.AddNumberMatch(IF, HandleTarget::TARGET,
                                         ::GetCurrentProcessId(), EQUAL)) {
        return false;
      }
      break;
    }

    default:
     return false;
  }
  if (!duplicate_rule.AddStringMatch(IF, HandleTarget::NAME, type_name,
                                     CASE_INSENSITIVE)) {
    return false;
  }
  if (!policy->AddRule(IPC_DUPLICATEHANDLEPROXY_TAG, &duplicate_rule)) {
    return false;
  }
  return true;
}

DWORD HandlePolicy::DuplicateHandleProxyAction(EvalResult eval_result,
                                               HANDLE source_handle,
                                               DWORD target_process_id,
                                               HANDLE* target_handle,
                                               DWORD desired_access,
                                               DWORD options) {
  // The only action supported is ASK_BROKER which means duplicate the handle.
  if (ASK_BROKER != eval_result) {
    return ERROR_ACCESS_DENIED;
  }

  base::win::ScopedHandle remote_target_process;
  if (target_process_id != ::GetCurrentProcessId()) {
    // Sandboxed children are dynamic, so we check that manually.
    if (!BrokerServicesBase::GetInstance()->IsActiveTarget(target_process_id)) {
      return ERROR_ACCESS_DENIED;
    }

    remote_target_process.Set(::OpenProcess(PROCESS_DUP_HANDLE, FALSE,
                                            target_process_id));
    if (!remote_target_process.IsValid())
      return ::GetLastError();
  }

  // If the policy didn't block us and we have no valid target, then the broker
  // (this process) is the valid target.
  HANDLE target_process = remote_target_process.IsValid() ?
                          remote_target_process.Get() : ::GetCurrentProcess();
  if (!::DuplicateHandle(::GetCurrentProcess(), source_handle, target_process,
                         target_handle, desired_access, FALSE,
                         options)) {
    return ::GetLastError();
  }

  return ERROR_SUCCESS;
}

}  // namespace sandbox

