// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_PROCESS_THREAD_POLICY_H_
#define SANDBOX_SRC_PROCESS_THREAD_POLICY_H_

#include <string>

#include "sandbox/win/src/policy_low_level.h"

#include "base/basictypes.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/sandbox_policy.h"

namespace sandbox {

enum EvalResult;

// This class centralizes most of the knowledge related to process execution.
class ProcessPolicy {
 public:
  // Creates the required low-level policy rules to evaluate a high-level.
  // policy rule for process creation
  // 'name' is the executable to be spawn.
  // 'semantics' is the desired semantics.
  // 'policy' is the policy generator to which the rules are going to be added.
  static bool GenerateRules(const wchar_t* name,
                            TargetPolicy::Semantics semantics,
                            LowLevelPolicy* policy);

  // Opens a thread from the child process and returns the handle.
  // client_info contains the information about the child process,
  // desired_access is the access requested by the child and thread_id
  // is the thread_id to be opened.
  // The function returns the return value of NtOpenThread.
  static NTSTATUS OpenThreadAction(const ClientInfo& client_info,
                                   uint32 desired_access,
                                   uint32 thread_id,
                                   HANDLE* handle);

  // Opens the process id passed in and returns the duplicated handle to
  // the child. We only allow the child processes to open themselves. Any other
  // pid open is denied.
  static NTSTATUS OpenProcessAction(const ClientInfo& client_info,
                                    uint32 desired_access,
                                    uint32 process_id,
                                    HANDLE* handle);

  // Opens the token associated with the process and returns the duplicated
  // handle to the child. We only allow the child processes to open his own
  // token (using ::GetCurrentProcess()).
  static NTSTATUS OpenProcessTokenAction(const ClientInfo& client_info,
                                         HANDLE process,
                                         uint32 desired_access,
                                         HANDLE* handle);

  // Opens the token associated with the process and returns the duplicated
  // handle to the child. We only allow the child processes to open his own
  // token (using ::GetCurrentProcess()).
  static NTSTATUS OpenProcessTokenExAction(const ClientInfo& client_info,
                                           HANDLE process,
                                           uint32 desired_access,
                                           uint32 attributes,
                                           HANDLE* handle);

  // Processes a 'CreateProcessW()' request from the target.
  // 'client_info' : the target process that is making the request.
  // 'eval_result' : The desired policy action to accomplish.
  // 'app_name' : The full path of the process to be created.
  // 'command_line' : The command line passed to the created process.
  static DWORD CreateProcessWAction(EvalResult eval_result,
                                    const ClientInfo& client_info,
                                    const std::wstring &app_name,
                                    const std::wstring &command_line,
                                    PROCESS_INFORMATION* process_info);
};

}  // namespace sandbox


#endif  // SANDBOX_SRC_PROCESS_THREAD_POLICY_H_
