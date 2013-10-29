// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_NAMED_PIPE_DISPATCHER_H__
#define SANDBOX_SRC_NAMED_PIPE_DISPATCHER_H__

#include "base/basictypes.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/sandbox_policy_base.h"

namespace sandbox {

// This class handles named pipe related IPC calls.
class NamedPipeDispatcher : public Dispatcher {
 public:
  explicit NamedPipeDispatcher(PolicyBase* policy_base);
  ~NamedPipeDispatcher() {}

  // Dispatcher interface.
  virtual bool SetupService(InterceptionManager* manager, int service);

 private:
  // Processes IPC requests coming from calls to CreateNamedPipeW() in the
  // target.
  bool CreateNamedPipe(IPCInfo* ipc, std::wstring* name, DWORD open_mode,
                       DWORD pipe_mode, DWORD max_instances,
                       DWORD out_buffer_size, DWORD in_buffer_size,
                       DWORD default_timeout);

  PolicyBase* policy_base_;
  DISALLOW_COPY_AND_ASSIGN(NamedPipeDispatcher);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_NAMED_PIPE_DISPATCHER_H__
