// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_SYSCALL_H__
#define SANDBOX_LINUX_SECCOMP_BPF_SYSCALL_H__

#include <stdint.h>

#include "sandbox/linux/sandbox_export.h"

namespace sandbox {

// We have to make sure that we have a single "magic" return address for
// our system calls, which we can check from within a BPF filter. This
// works by writing a little bit of asm() code that a) enters the kernel, and
// that also b) can be invoked in a way that computes this return address.
// Passing "nr" as "-1" computes the "magic" return address. Passing any
// other value invokes the appropriate system call.
SANDBOX_EXPORT intptr_t SandboxSyscall(int nr,
                                       intptr_t p0,
                                       intptr_t p1,
                                       intptr_t p2,
                                       intptr_t p3,
                                       intptr_t p4,
                                       intptr_t p5);

// System calls can take up to six parameters. Traditionally, glibc
// implements this property by using variadic argument lists. This works, but
// confuses modern tools such as valgrind, because we are nominally passing
// uninitialized data whenever we call through this function and pass less
// than the full six arguments.
// So, instead, we use C++'s template system to achieve a very similar
// effect. C++ automatically sets the unused parameters to zero for us, and
// it also does the correct type expansion (e.g. from 32bit to 64bit) where
// necessary.
// We have to use C-style cast operators as we want to be able to accept both
// integer and pointer types.
// We explicitly mark all functions as inline. This is not necessary in
// optimized builds, where the compiler automatically figures out that it
// can inline everything. But it makes stack traces of unoptimized builds
// easier to read as it hides implementation details.
#if __cplusplus >= 201103  // C++11

template <class T0 = intptr_t,
          class T1 = intptr_t,
          class T2 = intptr_t,
          class T3 = intptr_t,
          class T4 = intptr_t,
          class T5 = intptr_t>
SANDBOX_EXPORT inline intptr_t SandboxSyscall(int nr,
                                              T0 p0 = 0,
                                              T1 p1 = 0,
                                              T2 p2 = 0,
                                              T3 p3 = 0,
                                              T4 p4 = 0,
                                              T5 p5 = 0)
    __attribute__((always_inline));

template <class T0, class T1, class T2, class T3, class T4, class T5>
SANDBOX_EXPORT inline intptr_t
SandboxSyscall(int nr, T0 p0, T1 p1, T2 p2, T3 p3, T4 p4, T5 p5) {
  return SandboxSyscall(nr,
                        (intptr_t)p0,
                        (intptr_t)p1,
                        (intptr_t)p2,
                        (intptr_t)p3,
                        (intptr_t)p4,
                        (intptr_t)p5);
}

#else  // Pre-C++11

// TODO(markus): C++11 has a much more concise and readable solution for
//   expressing what we are doing here. Delete the fall-back code for older
//   compilers as soon as we have fully switched to C++11

template <class T0, class T1, class T2, class T3, class T4, class T5>
SANDBOX_EXPORT inline intptr_t
    SandboxSyscall(int nr, T0 p0, T1 p1, T2 p2, T3 p3, T4 p4, T5 p5)
    __attribute__((always_inline));
template <class T0, class T1, class T2, class T3, class T4, class T5>
SANDBOX_EXPORT inline intptr_t
SandboxSyscall(int nr, T0 p0, T1 p1, T2 p2, T3 p3, T4 p4, T5 p5) {
  return SandboxSyscall(nr,
                        (intptr_t)p0,
                        (intptr_t)p1,
                        (intptr_t)p2,
                        (intptr_t)p3,
                        (intptr_t)p4,
                        (intptr_t)p5);
}

template <class T0, class T1, class T2, class T3, class T4>
SANDBOX_EXPORT inline intptr_t
    SandboxSyscall(int nr, T0 p0, T1 p1, T2 p2, T3 p3, T4 p4)
    __attribute__((always_inline));
template <class T0, class T1, class T2, class T3, class T4>
SANDBOX_EXPORT inline intptr_t
SandboxSyscall(int nr, T0 p0, T1 p1, T2 p2, T3 p3, T4 p4) {
  return SandboxSyscall(nr, p0, p1, p2, p3, p4, 0);
}

template <class T0, class T1, class T2, class T3>
SANDBOX_EXPORT inline intptr_t
    SandboxSyscall(int nr, T0 p0, T1 p1, T2 p2, T3 p3)
    __attribute__((always_inline));
template <class T0, class T1, class T2, class T3>
SANDBOX_EXPORT inline intptr_t
SandboxSyscall(int nr, T0 p0, T1 p1, T2 p2, T3 p3) {
  return SandboxSyscall(nr, p0, p1, p2, p3, 0, 0);
}

template <class T0, class T1, class T2>
SANDBOX_EXPORT inline intptr_t SandboxSyscall(int nr, T0 p0, T1 p1, T2 p2)
    __attribute__((always_inline));
template <class T0, class T1, class T2>
SANDBOX_EXPORT inline intptr_t SandboxSyscall(int nr, T0 p0, T1 p1, T2 p2) {
  return SandboxSyscall(nr, p0, p1, p2, 0, 0, 0);
}

template <class T0, class T1>
SANDBOX_EXPORT inline intptr_t SandboxSyscall(int nr, T0 p0, T1 p1)
    __attribute__((always_inline));
template <class T0, class T1>
SANDBOX_EXPORT inline intptr_t SandboxSyscall(int nr, T0 p0, T1 p1) {
  return SandboxSyscall(nr, p0, p1, 0, 0, 0, 0);
}

template <class T0>
SANDBOX_EXPORT inline intptr_t SandboxSyscall(int nr, T0 p0)
    __attribute__((always_inline));
template <class T0>
SANDBOX_EXPORT inline intptr_t SandboxSyscall(int nr, T0 p0) {
  return SandboxSyscall(nr, p0, 0, 0, 0, 0, 0);
}

SANDBOX_EXPORT inline intptr_t SandboxSyscall(int nr)
    __attribute__((always_inline));
SANDBOX_EXPORT inline intptr_t SandboxSyscall(int nr) {
  return SandboxSyscall(nr, 0, 0, 0, 0, 0, 0);
}

#endif  // Pre-C++11

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SECCOMP_BPF_SYSCALL_H__
