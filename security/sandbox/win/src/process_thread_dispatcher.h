// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_PROCESS_THREAD_DISPATCHER_H_
#define SANDBOX_SRC_PROCESS_THREAD_DISPATCHER_H_

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/sandbox_policy_base.h"

namespace sandbox {

// This class handles process and thread-related IPC calls.
class ThreadProcessDispatcher : public Dispatcher {
 public:
  explicit ThreadProcessDispatcher(PolicyBase* policy_base);
  ~ThreadProcessDispatcher() {}

  // Dispatcher interface.
  virtual bool SetupService(InterceptionManager* manager, int service);

 private:
  // Processes IPC requests coming from calls to NtOpenThread() in the target.
  bool NtOpenThread(IPCInfo* ipc, DWORD desired_access, DWORD thread_id);

  // Processes IPC requests coming from calls to NtOpenProcess() in the target.
  bool NtOpenProcess(IPCInfo* ipc, DWORD desired_access, DWORD process_id);

  // Processes IPC requests from calls to NtOpenProcessToken() in the target.
  bool NtOpenProcessToken(IPCInfo* ipc, HANDLE process, DWORD desired_access);

  // Processes IPC requests from calls to NtOpenProcessTokenEx() in the target.
  bool NtOpenProcessTokenEx(IPCInfo* ipc, HANDLE process, DWORD desired_access,
                            DWORD attributes);

  // Processes IPC requests coming from calls to CreateProcessW() in the target.
  bool CreateProcessW(IPCInfo* ipc,
                      base::string16* name,
                      base::string16* cmd_line,
                      base::string16* cur_dir,
                      CountedBuffer* info);

  PolicyBase* policy_base_;
  DISALLOW_COPY_AND_ASSIGN(ThreadProcessDispatcher);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_PROCESS_THREAD_DISPATCHER_H_
