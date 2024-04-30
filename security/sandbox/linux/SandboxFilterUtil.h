/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxFilterUtil_h
#define mozilla_SandboxFilterUtil_h

// This header file exists to hold helper code for SandboxFilter.cpp,
// to make that file easier to read for anyone trying to understand
// the filter policy.  It's mostly about smoothing out differences
// between different Linux architectures.

#include "mozilla/Maybe.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

namespace mozilla {

// This class handles syscalls for BSD socket and SysV IPC operations.
// On 32-bit x86 they're multiplexed via socketcall(2) and ipc(2),
// respectively; on most other architectures they're individual system
// calls. It translates the syscalls into socketcall/ipc selector
// values, because those are defined (even if not used) for all
// architectures.  (As of kernel 4.2.0, x86 also has regular system
// calls, but userland will typically still use socketcall.)
//
// This EvaluateSyscall() routine always returns InvalidSyscall() for
// everything else.  It's assumed that subclasses will be implementing
// a whitelist policy, so they can handle what they're whitelisting
// and then defer to this class in the default case.
class SandboxPolicyBase : public sandbox::bpf_dsl::Policy {
 public:
  using ResultExpr = sandbox::bpf_dsl::ResultExpr;

  virtual ResultExpr EvaluateSyscall(int aSysno) const override;

  // aHasArgs is true if this is a normal syscall, where the arguments
  // can be inspected by seccomp-bpf, rather than a case of socketcall().
  virtual Maybe<ResultExpr> EvaluateSocketCall(int aCall, bool aHasArgs) const {
    return Nothing();
  }

  // Android doesn't use SysV IPC (and doesn't define the selector
  // constants in its headers), so this isn't implemented there.
#ifndef ANDROID
  // aArgShift is the offset to add the argument index when
  // constructing `Arg` objects: it's 0 for separate syscalls and 1
  // for ipc().
  virtual Maybe<ResultExpr> EvaluateIpcCall(int aCall, int aArgShift) const {
    return Nothing();
  }
#endif

  // Returns true if the running kernel supports separate syscalls for
  // socket operations, or false if it supports only socketcall(2).
  static bool HasSeparateSocketCalls();
};

}  // namespace mozilla

// "Machine independent" pseudo-syscall numbers, to deal with arch
// dependencies.  (Most 32-bit archs started with 32-bit off_t; older
// archs started with 16-bit uid_t/gid_t; 32-bit registers can't hold
// a 64-bit offset for mmap; and so on.)
//
// For some of these, the "old" syscalls are also in use in some
// cases; see, e.g., the handling of RT vs. non-RT signal syscalls.

#ifdef __NR_mmap2
#  define CASES_FOR_mmap case __NR_mmap2
#else
#  define CASES_FOR_mmap case __NR_mmap
#endif

#ifdef __NR_fchown32
#  define CASES_FOR_fchown \
    case __NR_fchown32:    \
    case __NR_fchown
#else
#  define CASES_FOR_fchown case __NR_fchown
#endif

#ifdef __NR_getuid32
#  define CASES_FOR_getuid case __NR_getuid32
#  define CASES_FOR_getgid case __NR_getgid32
#  define CASES_FOR_geteuid case __NR_geteuid32
#  define CASES_FOR_getegid case __NR_getegid32
#  define CASES_FOR_getresuid \
    case __NR_getresuid32:    \
    case __NR_getresuid
#  define CASES_FOR_getresgid \
    case __NR_getresgid32:    \
    case __NR_getresgid
// The set*id syscalls are omitted; we'll probably never need to allow them.
#else
#  define CASES_FOR_getuid case __NR_getuid
#  define CASES_FOR_getgid case __NR_getgid
#  define CASES_FOR_geteuid case __NR_geteuid
#  define CASES_FOR_getegid case __NR_getegid
#  define CASES_FOR_getresuid case __NR_getresuid
#  define CASES_FOR_getresgid case __NR_getresgid
#endif

#ifdef __NR_stat64
#  define CASES_FOR_stat case __NR_stat64
#  define CASES_FOR_lstat case __NR_lstat64
#  define CASES_FOR_fstat case __NR_fstat64
#  define CASES_FOR_fstatat case __NR_fstatat64
#  define CASES_FOR_statfs \
    case __NR_statfs64:    \
    case __NR_statfs
#  define CASES_FOR_fstatfs \
    case __NR_fstatfs64:    \
    case __NR_fstatfs
#  define CASES_FOR_fcntl case __NR_fcntl64
// FIXME: we might not need the compat cases for these on non-Android:
#  define CASES_FOR_lseek \
    case __NR_lseek:      \
    case __NR__llseek
#  define CASES_FOR_ftruncate \
    case __NR_ftruncate:      \
    case __NR_ftruncate64
#else
#  define CASES_FOR_stat case __NR_stat
#  define CASES_FOR_lstat case __NR_lstat
#  define CASES_FOR_fstatat case __NR_newfstatat
#  define CASES_FOR_fstat case __NR_fstat
#  define CASES_FOR_fstatfs case __NR_fstatfs
#  define CASES_FOR_statfs case __NR_statfs
#  define CASES_FOR_fcntl case __NR_fcntl
#  define CASES_FOR_lseek case __NR_lseek
#  define CASES_FOR_ftruncate case __NR_ftruncate
#endif

// getdents is not like the other FS-related syscalls with a "64" variant
#ifdef __NR_getdents
#  define CASES_FOR_getdents \
    case __NR_getdents64:    \
    case __NR_getdents
#else
#  define CASES_FOR_getdents case __NR_getdents64
#endif

#ifdef __NR_sigprocmask
#  define CASES_FOR_sigprocmask \
    case __NR_sigprocmask:      \
    case __NR_rt_sigprocmask
#  define CASES_FOR_sigaction \
    case __NR_sigaction:      \
    case __NR_rt_sigaction
#  define CASES_FOR_sigreturn \
    case __NR_sigreturn:      \
    case __NR_rt_sigreturn
#else
#  define CASES_FOR_sigprocmask case __NR_rt_sigprocmask
#  define CASES_FOR_sigaction case __NR_rt_sigaction
#  define CASES_FOR_sigreturn case __NR_rt_sigreturn
#endif

#ifdef __NR_clock_gettime64
#  define CASES_FOR_clock_gettime \
    case __NR_clock_gettime:      \
    case __NR_clock_gettime64
#  define CASES_FOR_clock_getres \
    case __NR_clock_getres:      \
    case __NR_clock_getres_time64
#  define CASES_FOR_clock_nanosleep \
    case __NR_clock_nanosleep:      \
    case __NR_clock_nanosleep_time64
#  define CASES_FOR_pselect6 \
    case __NR_pselect6:      \
    case __NR_pselect6_time64
#  define CASES_FOR_ppoll \
    case __NR_ppoll:      \
    case __NR_ppoll_time64
#  define CASES_FOR_futex \
    case __NR_futex:      \
    case __NR_futex_time64
#else
#  define CASES_FOR_clock_gettime case __NR_clock_gettime
#  define CASES_FOR_clock_getres case __NR_clock_getres
#  define CASES_FOR_clock_nanosleep case __NR_clock_nanosleep
#  define CASES_FOR_pselect6 case __NR_pselect6
#  define CASES_FOR_ppoll case __NR_ppoll
#  define CASES_FOR_futex case __NR_futex
#endif

#if defined(__NR__newselect)
#  define CASES_FOR_select \
    case __NR__newselect:  \
      CASES_FOR_pselect6
#elif defined(__NR_select)
#  define CASES_FOR_select \
    case __NR_select:      \
      CASES_FOR_pselect6
#else
#  define CASES_FOR_select CASES_FOR_pselect6
#endif

#ifdef __NR_poll
#  define CASES_FOR_poll \
    case __NR_poll:      \
      CASES_FOR_ppoll
#else
#  define CASES_FOR_poll CASES_FOR_ppoll
#endif

#ifdef __NR_epoll_create
#  define CASES_FOR_epoll_create \
    case __NR_epoll_create:      \
    case __NR_epoll_create1
#else
#  define CASES_FOR_epoll_create case __NR_epoll_create1
#endif

#ifdef __NR_epoll_wait
#  define CASES_FOR_epoll_wait \
    case __NR_epoll_wait:      \
    case __NR_epoll_pwait:     \
    case __NR_epoll_pwait2
#else
#  define CASES_FOR_epoll_wait \
    case __NR_epoll_pwait:     \
    case __NR_epoll_pwait2
#endif

#ifdef __NR_pipe
#  define CASES_FOR_pipe \
    case __NR_pipe:      \
    case __NR_pipe2
#else
#  define CASES_FOR_pipe case __NR_pipe2
#endif

#ifdef __NR_dup2
#  define CASES_FOR_dup2 \
    case __NR_dup2:      \
    case __NR_dup3
#else
#  define CASES_FOR_dup2 case __NR_dup3
#endif

#ifdef __NR_ugetrlimit
#  define CASES_FOR_getrlimit case __NR_ugetrlimit
#else
#  define CASES_FOR_getrlimit case __NR_getrlimit
#endif

#endif  // mozilla_SandboxFilterUtil_h
