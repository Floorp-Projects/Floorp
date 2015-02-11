// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/errorcode.h"

#include <errno.h>

#include "base/macros.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/bpf_dsl/policy_compiler.h"
#include "sandbox/linux/seccomp-bpf/linux_seccomp.h"
#include "sandbox/linux/seccomp-bpf/trap.h"
#include "sandbox/linux/tests/unit_tests.h"

namespace sandbox {

namespace {

class DummyPolicy : public bpf_dsl::Policy {
 public:
  DummyPolicy() {}
  virtual ~DummyPolicy() {}

  virtual bpf_dsl::ResultExpr EvaluateSyscall(int sysno) const override {
    return bpf_dsl::Allow();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyPolicy);
};

SANDBOX_TEST(ErrorCode, ErrnoConstructor) {
  ErrorCode e0;
  SANDBOX_ASSERT(e0.err() == SECCOMP_RET_INVALID);

  ErrorCode e1(ErrorCode::ERR_ALLOWED);
  SANDBOX_ASSERT(e1.err() == SECCOMP_RET_ALLOW);

  ErrorCode e2(EPERM);
  SANDBOX_ASSERT(e2.err() == SECCOMP_RET_ERRNO + EPERM);

  DummyPolicy dummy_policy;
  bpf_dsl::PolicyCompiler compiler(&dummy_policy, Trap::Registry());
  ErrorCode e3 = compiler.Trap(NULL, NULL);
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
  DummyPolicy dummy_policy;
  bpf_dsl::PolicyCompiler compiler(&dummy_policy, Trap::Registry());
  ErrorCode e0 = compiler.Trap(NULL, "a");
  ErrorCode e1 = compiler.Trap(NULL, "b");
  SANDBOX_ASSERT((e0.err() & SECCOMP_RET_DATA) + 1 ==
                 (e1.err() & SECCOMP_RET_DATA));

  ErrorCode e2 = compiler.Trap(NULL, "a");
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

  DummyPolicy dummy_policy;
  bpf_dsl::PolicyCompiler compiler(&dummy_policy, Trap::Registry());
  ErrorCode e4 = compiler.Trap(NULL, "a");
  ErrorCode e5 = compiler.Trap(NULL, "b");
  ErrorCode e6 = compiler.Trap(NULL, "a");
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

  DummyPolicy dummy_policy;
  bpf_dsl::PolicyCompiler compiler(&dummy_policy, Trap::Registry());
  ErrorCode e4 = compiler.Trap(NULL, "a");
  ErrorCode e5 = compiler.Trap(NULL, "b");
  ErrorCode e6 = compiler.Trap(NULL, "a");
  SANDBOX_ASSERT(e1.LessThan(e4));
  SANDBOX_ASSERT(e3.LessThan(e4));
  SANDBOX_ASSERT(e4.LessThan(e5));
  SANDBOX_ASSERT(!e4.LessThan(e6));
  SANDBOX_ASSERT(!e6.LessThan(e4));
}

}  // namespace

}  // namespace sandbox
