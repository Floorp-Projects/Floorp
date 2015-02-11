// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/syscall_iterator.h"

#include "base/logging.h"
#include "base/macros.h"
#include "sandbox/linux/seccomp-bpf/linux_seccomp.h"

namespace sandbox {

namespace {

#if defined(__mips__) && (_MIPS_SIM == _MIPS_SIM_ABI32)
// This is true for Mips O32 ABI.
COMPILE_ASSERT(MIN_SYSCALL == __NR_Linux, min_syscall_should_be_4000);
#else
// This true for supported architectures (Intel and ARM EABI).
COMPILE_ASSERT(MIN_SYSCALL == 0u, min_syscall_should_always_be_zero);
#endif

// SyscallRange represents an inclusive range of system call numbers.
struct SyscallRange {
  uint32_t first;
  uint32_t last;
};

const SyscallRange kValidSyscallRanges[] = {
    // First we iterate up to MAX_PUBLIC_SYSCALL, which is equal to MAX_SYSCALL
    // on Intel architectures, but leaves room for private syscalls on ARM.
    {MIN_SYSCALL, MAX_PUBLIC_SYSCALL},
#if defined(__arm__)
    // ARM EABI includes "ARM private" system calls starting at
    // MIN_PRIVATE_SYSCALL, and a "ghost syscall private to the kernel" at
    // MIN_GHOST_SYSCALL.
    {MIN_PRIVATE_SYSCALL, MAX_PRIVATE_SYSCALL},
    {MIN_GHOST_SYSCALL, MAX_SYSCALL},
#endif
};

}  // namespace

SyscallSet::Iterator SyscallSet::begin() const {
  return Iterator(set_, false);
}

SyscallSet::Iterator SyscallSet::end() const {
  return Iterator(set_, true);
}

bool SyscallSet::IsValid(uint32_t num) {
  for (const SyscallRange& range : kValidSyscallRanges) {
    if (num >= range.first && num <= range.last) {
      return true;
    }
  }
  return false;
}

bool operator==(const SyscallSet& lhs, const SyscallSet& rhs) {
  return (lhs.set_ == rhs.set_);
}

SyscallSet::Iterator::Iterator(Set set, bool done)
    : set_(set), done_(done), num_(0) {
  // If the set doesn't contain 0, we need to skip to the next element.
  if (!done && set_ == (IsValid(num_) ? Set::INVALID_ONLY : Set::VALID_ONLY)) {
    ++*this;
  }
}

uint32_t SyscallSet::Iterator::operator*() const {
  DCHECK(!done_);
  return num_;
}

SyscallSet::Iterator& SyscallSet::Iterator::operator++() {
  DCHECK(!done_);

  num_ = NextSyscall();
  if (num_ == 0) {
    done_ = true;
  }

  return *this;
}

// NextSyscall returns the next system call in the iterated system
// call set after |num_|, or 0 if no such system call exists.
uint32_t SyscallSet::Iterator::NextSyscall() const {
  const bool want_valid = (set_ != Set::INVALID_ONLY);
  const bool want_invalid = (set_ != Set::VALID_ONLY);

  for (const SyscallRange& range : kValidSyscallRanges) {
    if (want_invalid && range.first > 0 && num_ < range.first - 1) {
      // Even when iterating invalid syscalls, we only include the end points;
      // so skip directly to just before the next (valid) range.
      return range.first - 1;
    }
    if (want_valid && num_ < range.first) {
      return range.first;
    }
    if (want_valid && num_ < range.last) {
      return num_ + 1;
    }
    if (want_invalid && num_ <= range.last) {
      return range.last + 1;
    }
  }

  if (want_invalid) {
    // BPF programs only ever operate on unsigned quantities. So,
    // that's how we iterate; we return values from
    // 0..0xFFFFFFFFu. But there are places, where the kernel might
    // interpret system call numbers as signed quantities, so the
    // boundaries between signed and unsigned values are potential
    // problem cases. We want to explicitly return these values from
    // our iterator.
    if (num_ < 0x7FFFFFFFu)
      return 0x7FFFFFFFu;
    if (num_ < 0x80000000u)
      return 0x80000000u;

    if (num_ < 0xFFFFFFFFu)
      return 0xFFFFFFFFu;
  }

  return 0;
}

bool operator==(const SyscallSet::Iterator& lhs,
                const SyscallSet::Iterator& rhs) {
  DCHECK(lhs.set_ == rhs.set_);
  return (lhs.done_ == rhs.done_) && (lhs.num_ == rhs.num_);
}

bool operator!=(const SyscallSet::Iterator& lhs,
                const SyscallSet::Iterator& rhs) {
  return !(lhs == rhs);
}

}  // namespace sandbox
