/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SANDBOX_SRC_LINE_BREAK_DISPATCHER_H_
#define SANDBOX_SRC_LINE_BREAK_DISPATCHER_H_

#include "base/macros.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/sandbox_policy_base.h"

namespace sandbox {

// This class handles line break related IPC calls.
class LineBreakDispatcher final : public Dispatcher {
 public:
  explicit LineBreakDispatcher(PolicyBase* policy_base);
  ~LineBreakDispatcher() final {}

  // Dispatcher interface.
  bool SetupService(InterceptionManager* manager, IpcTag service) final;

 private:
  // Processes IPC requests coming from calls to
  // TargetServices::GetComplexLineBreaks() in the target.
  bool GetComplexLineBreaksCall(IPCInfo* ipc, CountedBuffer* text_buf,
                                uint32_t length,
                                CountedBuffer* break_before_buf);

  PolicyBase* policy_base_;
  DISALLOW_COPY_AND_ASSIGN(LineBreakDispatcher);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_LINE_BREAK_DISPATCHER_H_
