// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Sandbox is a sandbox library for windows processes. Use when you want a
// 'privileged' process and a 'locked down process' to interact with.
// The privileged process is called the broker and it is started by external
// means (such as the user starting it). The 'sandboxed' process is called the
// target and it is started by the broker. There can be many target processes
// started by a single broker process. This library provides facilities
// for both the broker and the target.
//
// The design rationale and relevant documents can be found at http://go/sbox.
//
// Note: this header does not include the SandboxFactory definitions because
// there are cases where the Sandbox library is linked against the main .exe
// while its API needs to be used in a DLL.

#ifndef SANDBOX_WIN_SRC_SANDBOX_H_
#define SANDBOX_WIN_SRC_SANDBOX_H_

#include <windows.h>

#include "base/memory/ref_counted.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sandbox_types.h"

// sandbox: Google User-Land Application Sandbox
namespace sandbox {

class BrokerServices;
class ProcessState;
class TargetPolicy;
class TargetServices;

// BrokerServices exposes all the broker API.
// The basic use is to start the target(s) and wait for them to end.
//
// This API is intended to be called in the following order
// (error checking omitted):
//  BrokerServices* broker = SandboxFactory::GetBrokerServices();
//  broker->Init();
//  PROCESS_INFORMATION target;
//  broker->SpawnTarget(target_exe_path, target_args, &target);
//  ::ResumeThread(target->hThread);
//  // -- later you can call:
//  broker->WaitForAllTargets(option);
//
class BrokerServices {
 public:
  // Initializes the broker. Must be called before any other on this class.
  // returns ALL_OK if successful. All other return values imply failure.
  // If the return is ERROR_GENERIC, you can call ::GetLastError() to get
  // more information.
  virtual ResultCode Init() = 0;

  // Returns the interface pointer to a new, empty policy object. Use this
  // interface to specify the sandbox policy for new processes created by
  // SpawnTarget()
  virtual scoped_refptr<TargetPolicy> CreatePolicy() = 0;

  // Creates a new target (child process) in a suspended state.
  // Parameters:
  //   exe_path: This is the full path to the target binary. This parameter
  //   can be null and in this case the exe path must be the first argument
  //   of the command_line.
  //   command_line: The arguments to be passed as command line to the new
  //   process. This can be null if the exe_path parameter is not null.
  //   policy: This is the pointer to the policy object for the sandbox to
  //   be created.
  //   last_warning: The argument will contain an indication on whether
  //   the process security was initialized completely, Only set if the
  //   process can be used without a serious compromise in security.
  //   last_error: If an error or warning is returned from this method this
  //   parameter will hold the last Win32 error value.
  //   target: returns the resulting target process information such as process
  //   handle and PID just as if CreateProcess() had been called. The caller is
  //   responsible for closing the handles returned in this structure.
  // Returns:
  //   ALL_OK if successful. All other return values imply failure.
  virtual ResultCode SpawnTarget(const wchar_t* exe_path,
                                 const wchar_t* command_line,
                                 scoped_refptr<TargetPolicy> policy,
                                 ResultCode* last_warning,
                                 DWORD* last_error,
                                 PROCESS_INFORMATION* target) = 0;

  // This call blocks (waits) for all the targets to terminate.
  // Returns:
  //   ALL_OK if successful. All other return values imply failure.
  //   If the return is ERROR_GENERIC, you can call ::GetLastError() to get
  //   more information.
  virtual ResultCode WaitForAllTargets() = 0;

  // Adds an unsandboxed process as a peer for policy decisions (e.g.
  // HANDLES_DUP_ANY policy).
  // Returns:
  //   ALL_OK if successful. All other return values imply failure.
  //   If the return is ERROR_GENERIC, you can call ::GetLastError() to get
  //   more information.
  virtual ResultCode AddTargetPeer(HANDLE peer_process) = 0;
};

// TargetServices models the current process from the perspective
// of a target process. To obtain a pointer to it use
// Sandbox::GetTargetServices(). Note that this call returns a non-null
// pointer only if this process is in fact a target. A process is a target
// only if the process was spawned by a call to BrokerServices::SpawnTarget().
//
// This API allows the target to gain access to resources with a high
// privilege token and then when it is ready to perform dangerous activities
// (such as download content from the web) it can lower its token and
// enter into locked-down (sandbox) mode.
// The typical usage is as follows:
//
//   TargetServices* target_services = Sandbox::GetTargetServices();
//   if (NULL != target_services) {
//     // We are the target.
//     target_services->Init();
//     // Do work that requires high privileges here.
//     // ....
//     // When ready to enter lock-down mode call LowerToken:
//     target_services->LowerToken();
//   }
//
// For more information see the BrokerServices API documentation.
class TargetServices {
 public:
  // Initializes the target. Must call this function before any other.
  // returns ALL_OK if successful. All other return values imply failure.
  // If the return is ERROR_GENERIC, you can call ::GetLastError() to get
  // more information.
  virtual ResultCode Init() = 0;

  // Discards the impersonation token and uses the lower token, call before
  // processing any untrusted data or running third-party code. If this call
  // fails the current process could be terminated immediately.
  virtual void LowerToken() = 0;

  // Returns the ProcessState object. Through that object it's possible to have
  // information about the current state of the process, such as whether
  // LowerToken has been called or not.
  virtual ProcessState* GetState() = 0;

  // Requests the broker to duplicate the supplied handle into the target
  // process. The target process must be an active sandbox child process
  // and the source process must have a corresponding policy allowing
  // handle duplication for this object type.
  // Returns:
  //   ALL_OK if successful. All other return values imply failure.
  //   If the return is ERROR_GENERIC, you can call ::GetLastError() to get
  //   more information.
  virtual ResultCode DuplicateHandle(HANDLE source_handle,
                                     DWORD target_process_id,
                                     HANDLE* target_handle,
                                     DWORD desired_access,
                                     DWORD options) = 0;
};

}  // namespace sandbox


#endif  // SANDBOX_WIN_SRC_SANDBOX_H_
