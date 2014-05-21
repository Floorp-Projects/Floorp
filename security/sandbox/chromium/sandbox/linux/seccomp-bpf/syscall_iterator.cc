// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/syscall_iterator.h"

#include "base/basictypes.h"
#include "sandbox/linux/seccomp-bpf/linux_seccomp.h"

namespace sandbox {

uint32_t SyscallIterator::Next() {
  if (done_) {
    return num_;
  }

  uint32_t val;
  do {
    // |num_| has been initialized to 0, which we assume is also MIN_SYSCALL.
    // This true for supported architectures (Intel and ARM EABI).
    COMPILE_ASSERT(MIN_SYSCALL == 0u, min_syscall_should_always_be_zero);
    val = num_;

    // First we iterate up to MAX_PUBLIC_SYSCALL, which is equal to MAX_SYSCALL
    // on Intel architectures, but leaves room for private syscalls on ARM.
    if (num_ <= MAX_PUBLIC_SYSCALL) {
      if (invalid_only_ && num_ < MAX_PUBLIC_SYSCALL) {
        num_ = MAX_PUBLIC_SYSCALL;
      } else {
        ++num_;
      }
#if defined(__arm__)
      // ARM EABI includes "ARM private" system calls starting at
      // MIN_PRIVATE_SYSCALL, and a "ghost syscall private to the kernel" at
      // MIN_GHOST_SYSCALL.
    } else if (num_ < MIN_PRIVATE_SYSCALL - 1) {
      num_ = MIN_PRIVATE_SYSCALL - 1;
    } else if (num_ <= MAX_PRIVATE_SYSCALL) {
      if (invalid_only_ && num_ < MAX_PRIVATE_SYSCALL) {
        num_ = MAX_PRIVATE_SYSCALL;
      } else {
        ++num_;
      }
    } else if (num_ < MIN_GHOST_SYSCALL - 1) {
      num_ = MIN_GHOST_SYSCALL - 1;
    } else if (num_ <= MAX_SYSCALL) {
      if (invalid_only_ && num_ < MAX_SYSCALL) {
        num_ = MAX_SYSCALL;
      } else {
        ++num_;
      }
#endif
      // BPF programs only ever operate on unsigned quantities. So, that's how
      // we iterate; we return values from 0..0xFFFFFFFFu. But there are places,
      // where the kernel might interpret system call numbers as signed
      // quantities, so the boundaries between signed and unsigned values are
      // potential problem cases. We want to explicitly return these values from
      // our iterator.
    } else if (num_ < 0x7FFFFFFFu) {
      num_ = 0x7FFFFFFFu;
    } else if (num_ < 0x80000000u) {
      num_ = 0x80000000u;
    } else if (num_ < 0xFFFFFFFFu) {
      num_ = 0xFFFFFFFFu;
    }
  } while (invalid_only_ && IsValid(val));

  done_ |= val == 0xFFFFFFFFu;
  return val;
}

bool SyscallIterator::IsValid(uint32_t num) {
  uint32_t min_syscall = MIN_SYSCALL;
  if (num >= min_syscall && num <= MAX_PUBLIC_SYSCALL) {
    return true;
  }
  if (IsArmPrivate(num)) {
    return true;
  }
  return false;
}

#if defined(__arm__) && (defined(__thumb__) || defined(__ARM_EABI__))
bool SyscallIterator::IsArmPrivate(uint32_t num) {
  return (num >= MIN_PRIVATE_SYSCALL && num <= MAX_PRIVATE_SYSCALL) ||
         (num >= MIN_GHOST_SYSCALL && num <= MAX_SYSCALL);
}
#else
bool SyscallIterator::IsArmPrivate(uint32_t) { return false; }
#endif

}  // namespace sandbox
