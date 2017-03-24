// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_BROKER_SERVICES_H_
#define SANDBOX_WIN_SRC_BROKER_SERVICES_H_

#include <list>
#include <map>
#include <set>
#include <utility>
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/win/scoped_handle.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/job.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sharedmem_ipc_server.h"
#include "sandbox/win/src/win2k_threadpool.h"
#include "sandbox/win/src/win_utils.h"

namespace {

struct JobTracker;
struct PeerTracker;

}  // namespace

namespace sandbox {

class PolicyBase;

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
  TargetPolicy* CreatePolicy() override;
  ResultCode SpawnTarget(const wchar_t* exe_path,
                         const wchar_t* command_line,
                         TargetPolicy* policy,
                         PROCESS_INFORMATION* target) override;
  ResultCode WaitForAllTargets() override;
  ResultCode AddTargetPeer(HANDLE peer_process) override;
  ResultCode InstallAppContainer(const wchar_t* sid,
                                 const wchar_t* name) override;
  ResultCode UninstallAppContainer(const wchar_t* sid) override;

  // Checks if the supplied process ID matches one of the broker's active
  // target processes
  // Returns:
  //   true if there is an active target process for this ID, otherwise false.
  bool IsActiveTarget(DWORD process_id);

 private:
  typedef std::list<JobTracker*> JobTrackerList;
  typedef std::map<DWORD, PeerTracker*> PeerTrackerMap;

  // The routine that the worker thread executes. It is in charge of
  // notifications and cleanup-related tasks.
  static DWORD WINAPI TargetEventsThread(PVOID param);

  // Removes a target peer from the process list if it expires.
  static VOID CALLBACK RemovePeer(PVOID parameter, BOOLEAN timeout);

  // The completion port used by the job objects to communicate events to
  // the worker thread.
  base::win::ScopedHandle job_port_;

  // Handle to a manual-reset event that is signaled when the total target
  // process count reaches zero.
  base::win::ScopedHandle no_targets_;

  // Handle to the worker thread that reacts to job notifications.
  base::win::ScopedHandle job_thread_;

  // Lock used to protect the list of targets from being modified by 2
  // threads at the same time.
  CRITICAL_SECTION lock_;

  // provides a pool of threads that are used to wait on the IPC calls.
  ThreadProvider* thread_pool_;

  // List of the trackers for closing and cleanup purposes.
  JobTrackerList tracker_list_;

  // Maps peer process IDs to the saved handle and wait event.
  // Prevents peer callbacks from accessing the broker after destruction.
  PeerTrackerMap peer_map_;

  // Provides a fast lookup to identify sandboxed processes that belong to a
  // job. Consult |jobless_process_handles_| for handles of pocess without job.
  std::set<DWORD> child_process_ids_;

  DISALLOW_COPY_AND_ASSIGN(BrokerServicesBase);
};

}  // namespace sandbox


#endif  // SANDBOX_WIN_SRC_BROKER_SERVICES_H_
