// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_HANDLE_DISPATCHER_H_
#define SANDBOX_SRC_HANDLE_DISPATCHER_H_

#include "base/basictypes.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/sandbox_policy_base.h"

namespace sandbox {

// This class handles handle-related IPC calls.
class HandleDispatcher : public Dispatcher {
 public:
  explicit HandleDispatcher(PolicyBase* policy_base);
  ~HandleDispatcher() {}

  // Dispatcher interface.
  virtual bool SetupService(InterceptionManager* manager, int service);

 private:
  // Processes IPC requests coming from calls to
  // TargetServices::DuplicateHandle() in the target.
  bool DuplicateHandleProxy(IPCInfo* ipc,
                            HANDLE source_handle,
                            uint32 target_process_id,
                            uint32 desired_access,
                            uint32 options);

  PolicyBase* policy_base_;
  DISALLOW_COPY_AND_ASSIGN(HandleDispatcher);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_HANDLE_DISPATCHER_H_

