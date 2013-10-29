// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox_types.h"

#ifndef SANDBOX_SRC_HANDLE_INTERCEPTION_H_
#define SANDBOX_SRC_HANDLE_INTERCEPTION_H_

namespace sandbox {

// TODO(jschuh) Add an interception to catch dangerous DuplicateHandle calls.

ResultCode DuplicateHandleProxy(HANDLE source_handle,
                                DWORD target_process_id,
                                HANDLE* target_handle,
                                DWORD desired_access,
                                DWORD options);

}  // namespace sandbox

#endif  // SANDBOX_SRC_HANDLE_INTERCEPTION_H_

