// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_SYNC_DISPATCHER_H_
#define SANDBOX_SRC_SYNC_DISPATCHER_H_

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/sandbox_policy_base.h"

namespace sandbox {

// This class handles sync-related IPC calls.
class SyncDispatcher : public Dispatcher {
 public:
  explicit SyncDispatcher(PolicyBase* policy_base);
  ~SyncDispatcher() {}

  // Dispatcher interface.
  virtual bool SetupService(InterceptionManager* manager, int service);

private:
  // Processes IPC requests coming from calls to CreateEvent in the target.
  bool CreateEvent(IPCInfo* ipc,
                   base::string16* name,
                   uint32 event_type,
                   uint32 initial_state);

  // Processes IPC requests coming from calls to OpenEvent in the target.
  bool OpenEvent(IPCInfo* ipc, base::string16* name, uint32 desired_access);

  PolicyBase* policy_base_;
  DISALLOW_COPY_AND_ASSIGN(SyncDispatcher);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_SYNC_DISPATCHER_H_
