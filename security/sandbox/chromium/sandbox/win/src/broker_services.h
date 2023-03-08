// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_BROKER_SERVICES_H_
#define SANDBOX_WIN_SRC_BROKER_SERVICES_H_

#include <list>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/compiler_specific.h"
#include "base/environment.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/win/scoped_handle.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/job.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_policy_base.h"
#include "sandbox/win/src/sharedmem_ipc_server.h"
#include "sandbox/win/src/win2k_threadpool.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {

// BrokerServicesBase ---------------------------------------------------------
// Broker implementation version 0
//
// This is an implementation of the interface BrokerServices and
// of the associated TargetProcess interface. In this implementation
// TargetProcess is a friend of BrokerServices where the later manages a
// collection of the former.
class BrokerServicesBase final : public BrokerServices,
                                 public SingletonBase<BrokerServicesBase> {
 public:
  BrokerServicesBase();

  ~BrokerServicesBase();

  // BrokerServices interface.
  ResultCode Init() override;
  scoped_refptr<TargetPolicy> CreatePolicy() override;
  ResultCode SpawnTarget(const wchar_t* exe_path,
                         const wchar_t* command_line,
                         base::EnvironmentMap& env_map,
                         scoped_refptr<TargetPolicy> policy,
                         ResultCode* last_warning,
                         DWORD* last_error,
                         PROCESS_INFORMATION* target) override;
  ResultCode WaitForAllTargets() override;
  ResultCode AddTargetPeer(HANDLE peer_process) override;

  // Checks if the supplied process ID matches one of the broker's active
  // target processes.  We use this method for the specific purpose of
  // checking if we can safely duplicate a handle to the supplied process
  // in DuplicateHandleProxyAction.
  bool IsSafeDuplicationTarget(DWORD process_id);

  ResultCode GetPolicyDiagnostics(
      std::unique_ptr<PolicyDiagnosticsReceiver> receiver) override;

  bool DeriveCapabilitySidFromName(const wchar_t* name, PSID derived_sid,
                                   DWORD sid_buffer_length) override;

 private:
  // The routine that the worker thread executes. It is in charge of
  // notifications and cleanup-related tasks.
  static DWORD WINAPI TargetEventsThread(PVOID param);

  // The completion port used by the job objects to communicate events to
  // the worker thread.
  base::win::ScopedHandle job_port_;

  // Handle to a manual-reset event that is signaled when the total target
  // process count reaches zero.
  base::win::ScopedHandle no_targets_;

  // Handle to the worker thread that reacts to job notifications.
  base::win::ScopedHandle job_thread_;

  // Provides a pool of threads that are used to wait on the IPC calls.
  std::unique_ptr<ThreadProvider> thread_pool_;

  // The set representing the broker's active target processes including
  // both sandboxed and unsandboxed peer processes.
  std::set<DWORD> active_targets_;

  // Lock used to protect active_targets_ from being simultaneously accessed
  // by multiple threads.
  CRITICAL_SECTION lock_;

  ResultCode AddTargetPeerInternal(HANDLE peer_process_handle,
                                   DWORD peer_process_id,
                                   scoped_refptr<PolicyBase> policy_base,
                                   DWORD* last_error);

  DISALLOW_COPY_AND_ASSIGN(BrokerServicesBase);
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_BROKER_SERVICES_H_
