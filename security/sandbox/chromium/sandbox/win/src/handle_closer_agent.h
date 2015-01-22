// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_HANDLE_CLOSER_AGENT_H_
#define SANDBOX_SRC_HANDLE_CLOSER_AGENT_H_

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "sandbox/win/src/handle_closer.h"
#include "sandbox/win/src/sandbox_types.h"

namespace sandbox {

// Target process code to close the handle list copied over from the broker.
class HandleCloserAgent {
 public:
  HandleCloserAgent() {}

  // Reads the serialized list from the broker and creates the lookup map.
  void InitializeHandlesToClose();

  // Closes any handles matching those in the lookup map.
  bool CloseHandles();

  // True if we have handles waiting to be closed
  static bool NeedsHandlesClosed();

 private:
  HandleMap handles_to_close_;

  DISALLOW_COPY_AND_ASSIGN(HandleCloserAgent);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_HANDLE_CLOSER_AGENT_H_
