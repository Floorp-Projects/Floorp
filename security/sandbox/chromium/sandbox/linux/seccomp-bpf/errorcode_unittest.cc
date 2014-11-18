// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>

#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/tests/unit_tests.h"

namespace sandbox {

namespace {

SANDBOX_TEST(ErrorCode, ErrnoConstructor) {
  ErrorCode e0;
  SANDBOX_ASSERT(e0.err() == SECCOMP_RET_INVALID);

  ErrorCode e1(ErrorCode::ERR_ALLOWED);
  SANDBOX_ASSERT(e1.err() == SECCOMP_RET_ALLOW);

  ErrorCode e2(EPERM);
  SANDBOX_ASSERT(e2.err() == SECCOMP_RET_ERRNO + EPERM);

  SandboxBPF sandbox;
  ErrorCode e3 = sandbox.Trap(NULL, NULL);
  SANDBOX_ASSERT((e3.err() & SECCOMP_RET_ACTION)  == SECCOMP_RET_TRAP);

  uint16_t data = 0xdead;
  ErrorCode e4(ErrorCode::ERR_TRACE + data);
  SANDBOX_ASSERT(e4.err() == SECCOMP_RET_TRACE + data);
}

SANDBOX_DEATH_TEST(ErrorCode,
                   InvalidSeccompRetTrace,
                   DEATH_MESSAGE("Invalid use of ErrorCode object")) {
  // Should die if the trace data does not fit in 16 bits.
  ErrorCode e(ErrorCode::ERR_TRACE + (1 << 16));
}

SANDBOX_TEST(ErrorCode, Trap) {
  SandboxBPF sandbox;
  ErrorCode e0 = sandbox.Trap(NULL, "a");
  ErrorCode e1 = sandbox.Trap(NULL, "b");
  SANDBOX_ASSERT((e0.err() & SECCOMP_RET_DATA) + 1 ==
                 (e1.err() & SECCOMP_RET_DATA));

  ErrorCode e2 = sandbox.Trap(NULL, "a");
  SANDBOX_ASSERT((e0.err() & SECCOMP_RET_DATA) ==
                 (e2.err() & SECCOMP_RET_DATA));
}

SANDBOX_TEST(ErrorCode, Equals) {
  ErrorCode e1(ErrorCode::ERR_ALLOWED);
  ErrorCode e2(ErrorCode::ERR_ALLOWED);
  SANDBOX_ASSERT(e1.Equals(e1));
  SANDBOX_ASSERT(e1.Equals(e2));
  SANDBOX_ASSERT(e2.Equals(e1));

  ErrorCode e3(EPERM);
  SANDBOX_ASSERT(!e1.Equals(e3));

  SandboxBPF sandbox;
  ErrorCode e4 = sandbox.Trap(NULL, "a");
  ErrorCode e5 = sandbox.Trap(NULL, "b");
  ErrorCode e6 = sandbox.Trap(NULL, "a");
  SANDBOX_ASSERT(!e1.Equals(e4));
  SANDBOX_ASSERT(!e3.Equals(e4));
  SANDBOX_ASSERT(!e5.Equals(e4));
  SANDBOX_ASSERT( e6.Equals(e4));
}

SANDBOX_TEST(ErrorCode, LessThan) {
  ErrorCode e1(ErrorCode::ERR_ALLOWED);
  ErrorCode e2(ErrorCode::ERR_ALLOWED);
  SANDBOX_ASSERT(!e1.LessThan(e1));
  SANDBOX_ASSERT(!e1.LessThan(e2));
  SANDBOX_ASSERT(!e2.LessThan(e1));

  ErrorCode e3(EPERM);
  SANDBOX_ASSERT(!e1.LessThan(e3));
  SANDBOX_ASSERT( e3.LessThan(e1));

  SandboxBPF sandbox;
  ErrorCode e4 = sandbox.Trap(NULL, "a");
  ErrorCode e5 = sandbox.Trap(NULL, "b");
  ErrorCode e6 = sandbox.Trap(NULL, "a");
  SANDBOX_ASSERT(e1.LessThan(e4));
  SANDBOX_ASSERT(e3.LessThan(e4));
  SANDBOX_ASSERT(e4.LessThan(e5));
  SANDBOX_ASSERT(!e4.LessThan(e6));
  SANDBOX_ASSERT(!e6.LessThan(e4));
}

}  // namespace

}  // namespace sandbox
