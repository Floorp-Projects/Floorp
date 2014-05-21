// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_SYSCALL_ITERATOR_H__
#define SANDBOX_LINUX_SECCOMP_BPF_SYSCALL_ITERATOR_H__

#include <stdint.h>

#include "base/basictypes.h"
#include "sandbox/linux/sandbox_export.h"

namespace sandbox {

// Iterates over the entire system call range from 0..0xFFFFFFFFu. This
// iterator is aware of how system calls look like and will skip quickly
// over ranges that can't contain system calls. It iterates more slowly
// whenever it reaches a range that is potentially problematic, returning
// the last invalid value before a valid range of system calls, and the
// first invalid value after a valid range of syscalls. It iterates over
// individual values whenever it is in the normal range for system calls
// (typically MIN_SYSCALL..MAX_SYSCALL).
// If |invalid_only| is true, this iterator will only return invalid
// syscall numbers, but will still skip quickly over invalid ranges,
// returning the first invalid value in the range and then skipping
// to the last invalid value in the range.
//
// Example usage:
//   for (SyscallIterator iter(false); !iter.Done(); ) {
//     uint32_t sysnum = iter.Next();
//     // Do something with sysnum.
//   }
//
// TODO(markus): Make this a classic C++ iterator.
class SANDBOX_EXPORT SyscallIterator {
 public:
  explicit SyscallIterator(bool invalid_only)
      : invalid_only_(invalid_only), done_(false), num_(0) {}

  bool Done() const { return done_; }
  uint32_t Next();
  static bool IsValid(uint32_t num);

 private:
  static bool IsArmPrivate(uint32_t num);

  bool invalid_only_;
  bool done_;
  uint32_t num_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SyscallIterator);
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SECCOMP_BPF_SYSCALL_ITERATOR_H__
