// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_SYNC_POLICY_TEST_H_
#define SANDBOX_WIN_SRC_SYNC_POLICY_TEST_H_

#include "sandbox/win/tests/common/controller.h"

namespace sandbox {

// Opens the named event received on argv[1]. The requested access is
// EVENT_ALL_ACCESS if argv[0] starts with 'f', or SYNCHRONIZE otherwise.
SBOX_TESTS_COMMAND int Event_Open(int argc, wchar_t **argv);

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SYNC_POLICY_TEST_H_
