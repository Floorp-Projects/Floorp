// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/seccomp-bpf/syscall_iterator.h"
#include "sandbox/linux/tests/unit_tests.h"

namespace sandbox {

namespace {

SANDBOX_TEST(SyscallIterator, Monotonous) {
  for (int i = 0; i < 2; ++i) {
    bool invalid_only = !i;  // Testing both |invalid_only| cases.
    SyscallIterator iter(invalid_only);
    uint32_t next = iter.Next();

    if (!invalid_only) {
      // The iterator should start at 0.
      SANDBOX_ASSERT(next == 0);
    }
    for (uint32_t last = next; !iter.Done(); last = next) {
      next = iter.Next();
      SANDBOX_ASSERT(last < next);
    }
    // The iterator should always return 0xFFFFFFFFu as the last value.
    SANDBOX_ASSERT(next == 0xFFFFFFFFu);
  }
}

SANDBOX_TEST(SyscallIterator, PublicSyscallRange) {
  SyscallIterator iter(false);
  uint32_t next = iter.Next();

  // The iterator should cover the public syscall range
  // MIN_SYSCALL..MAX_PUBLIC_SYSCALL, without skipping syscalls.
  // We're assuming MIN_SYSCALL == 0 for all architectures,
  // this is currently valid for Intel and ARM EABI.
  SANDBOX_ASSERT(MIN_SYSCALL == 0);
  SANDBOX_ASSERT(next == MIN_SYSCALL);
  for (uint32_t last = next; next < MAX_PUBLIC_SYSCALL + 1; last = next) {
    SANDBOX_ASSERT((next = iter.Next()) == last + 1);
  }
  SANDBOX_ASSERT(next == MAX_PUBLIC_SYSCALL + 1);
}

#if defined(__arm__)
SANDBOX_TEST(SyscallIterator, ARMPrivateSyscallRange) {
  SyscallIterator iter(false);
  uint32_t next = iter.Next();
  while (next < MIN_PRIVATE_SYSCALL - 1) {
    next = iter.Next();
  }
  // The iterator should cover the ARM private syscall range
  // without skipping syscalls.
  SANDBOX_ASSERT(next == MIN_PRIVATE_SYSCALL - 1);
  for (uint32_t last = next; next < MAX_PRIVATE_SYSCALL + 1; last = next) {
    SANDBOX_ASSERT((next = iter.Next()) == last + 1);
  }
  SANDBOX_ASSERT(next == MAX_PRIVATE_SYSCALL + 1);
}

SANDBOX_TEST(SyscallIterator, ARMHiddenSyscallRange) {
  SyscallIterator iter(false);
  uint32_t next = iter.Next();
  while (next < MIN_GHOST_SYSCALL - 1) {
    next = iter.Next();
  }
  // The iterator should cover the ARM hidden syscall range
  // without skipping syscalls.
  SANDBOX_ASSERT(next == MIN_GHOST_SYSCALL - 1);
  for (uint32_t last = next; next < MAX_SYSCALL + 1; last = next) {
    SANDBOX_ASSERT((next = iter.Next()) == last + 1);
  }
  SANDBOX_ASSERT(next == MAX_SYSCALL + 1);
}
#endif

SANDBOX_TEST(SyscallIterator, Invalid) {
  for (int i = 0; i < 2; ++i) {
    bool invalid_only = !i;  // Testing both |invalid_only| cases.
    SyscallIterator iter(invalid_only);
    uint32_t next = iter.Next();

    while (next < MAX_SYSCALL + 1) {
      next = iter.Next();
    }

    SANDBOX_ASSERT(next == MAX_SYSCALL + 1);
    while (next < 0x7FFFFFFFu) {
      next = iter.Next();
    }

    // The iterator should return the signed/unsigned corner cases.
    SANDBOX_ASSERT(next == 0x7FFFFFFFu);
    next = iter.Next();
    SANDBOX_ASSERT(next == 0x80000000u);
    SANDBOX_ASSERT(!iter.Done());
    next = iter.Next();
    SANDBOX_ASSERT(iter.Done());
    SANDBOX_ASSERT(next == 0xFFFFFFFFu);
  }
}

SANDBOX_TEST(SyscallIterator, InvalidOnly) {
  bool invalid_only = true;
  SyscallIterator iter(invalid_only);
  uint32_t next = iter.Next();
  // We're assuming MIN_SYSCALL == 0 for all architectures,
  // this is currently valid for Intel and ARM EABI.
  // First invalid syscall should then be |MAX_PUBLIC_SYSCALL + 1|.
  SANDBOX_ASSERT(MIN_SYSCALL == 0);
  SANDBOX_ASSERT(next == MAX_PUBLIC_SYSCALL + 1);

#if defined(__arm__)
  next = iter.Next();
  // The iterator should skip until the last invalid syscall in this range.
  SANDBOX_ASSERT(next == MIN_PRIVATE_SYSCALL - 1);
  while (next <= MAX_PRIVATE_SYSCALL) {
    next = iter.Next();
  }

  next = iter.Next();
  // The iterator should skip until the last invalid syscall in this range.
  SANDBOX_ASSERT(next == MIN_GHOST_SYSCALL - 1);
  while (next <= MAX_SYSCALL) {
    next = iter.Next();
  }
  SANDBOX_ASSERT(next == MAX_SYSCALL + 1);
#endif
}

}  // namespace

}  // namespace sandbox
