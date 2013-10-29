// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_HANDLE_POLICY_H_
#define SANDBOX_SRC_HANDLE_POLICY_H_

#include <string>

#include "base/basictypes.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/policy_low_level.h"
#include "sandbox/win/src/sandbox_policy.h"

namespace sandbox {

enum EvalResult;

// This class centralizes most of the knowledge related to handle policy.
class HandlePolicy {
 public:
  // Creates the required low-level policy rules to evaluate a high-level
  // policy rule for handles, in particular duplicate action.
  static bool GenerateRules(const wchar_t* type_name,
                            TargetPolicy::Semantics semantics,
                            LowLevelPolicy* policy);

  // Processes a 'TargetPolicy::DuplicateHandle()' request from the target.
  static DWORD DuplicateHandleProxyAction(EvalResult eval_result,
                                          const ClientInfo& client_info,
                                          HANDLE source_handle,
                                          DWORD target_process_id,
                                          HANDLE* target_handle,
                                          DWORD desired_access,
                                          DWORD options);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_HANDLE_POLICY_H_

