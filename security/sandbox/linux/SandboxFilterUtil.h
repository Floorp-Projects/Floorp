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
// architectures.
//
// This EvaluateSyscall() routine always returns InvalidSyscall() for
// everything else.  It's assumed that subclasses will be implementing
// a whitelist policy, so they can handle what they're whitelisting
// and then defer to this class in the default case.
class SandboxPolicyBase : public sandbox::bpf_dsl::Policy
{
public:
  using ResultExpr = sandbox::bpf_dsl::ResultExpr;

  virtual ResultExpr EvaluateSyscall(int aSysno) const override;
  virtual Maybe<ResultExpr> EvaluateSocketCall(int aCall) const {
    return Nothing();
  }
#ifndef ANDROID
  // Android doesn't use SysV IPC (and doesn't define the selector
  // constants in its headers), so this isn't implemented there.
  virtual Maybe<ResultExpr> EvaluateIpcCall(int aCall) const {
    return Nothing();
  }
#endif

#ifdef __NR_socketcall
  // socketcall(2) takes the actual call's arguments via a pointer, so
  // seccomp-bpf can't inspect them; ipc(2) takes them at different indices.
  static const bool kSocketCallHasArgs = false;
  static const bool kIpcCallNormalArgs = false;
#else
  // Otherwise, the bpf_dsl Arg<> class can be used normally.
  static const bool kSocketCallHasArgs = true;
  static const bool kIpcCallNormalArgs = true;
#endif
};

} // namespace mozilla

// "Machine independent" pseudo-syscall numbers, to deal with arch
// dependencies.  (Most 32-bit archs started with 32-bit off_t; older
// archs started with 16-bit uid_t/gid_t; 32-bit registers can't hold
// a 64-bit offset for mmap; and so on.)
//
// For some of these, the "old" syscalls are also in use in some
// cases; see, e.g., the handling of RT vs. non-RT signal syscalls.

#ifdef __NR_mmap2
#define CASES_FOR_mmap   case __NR_mmap2
#else
#define CASES_FOR_mmap   case __NR_mmap
#endif

#ifdef __NR_fchown32
#define CASES_FOR_fchown   case __NR_fchown32: case __NR_fchown
#else
#define CASES_FOR_fchown   case __NR_fchown
#endif

#ifdef __NR_getuid32
#define CASES_FOR_getuid   case __NR_getuid32
#define CASES_FOR_getgid   case __NR_getgid32
#define CASES_FOR_geteuid   case __NR_geteuid32
#define CASES_FOR_getegid   case __NR_getegid32
#define CASES_FOR_getresuid   case __NR_getresuid32: case __NR_getresuid
#define CASES_FOR_getresgid   case __NR_getresgid32: case __NR_getresgid
// The set*id syscalls are omitted; we'll probably never need to allow them.
#else
#define CASES_FOR_getuid   case __NR_getuid
#define CASES_FOR_getgid   case __NR_getgid
#define CASES_FOR_geteuid   case __NR_geteuid
#define CASES_FOR_getegid   case __NR_getegid
#define CASES_FOR_getresuid   case __NR_getresuid
#define CASES_FOR_getresgid   case __NR_getresgid
#endif

#ifdef __NR_stat64
#define CASES_FOR_stat   case __NR_stat64
#define CASES_FOR_lstat   case __NR_lstat64
#define CASES_FOR_fstat   case __NR_fstat64
#define CASES_FOR_fstatat   case __NR_fstatat64
#define CASES_FOR_statfs   case __NR_statfs64: case __NR_statfs
#define CASES_FOR_fstatfs   case __NR_fstatfs64: case __NR_fstatfs
#define CASES_FOR_fcntl   case __NR_fcntl64
// We're using the 32-bit version on 32-bit desktop for some reason.
#define CASES_FOR_getdents   case __NR_getdents64: case __NR_getdents
// FIXME: we might not need the compat cases for these on non-Android:
#define CASES_FOR_lseek   case __NR_lseek: case __NR__llseek
#define CASES_FOR_ftruncate   case __NR_ftruncate: case __NR_ftruncate64
#else
#define CASES_FOR_stat   case __NR_stat
#define CASES_FOR_lstat   case __NR_lstat
#define CASES_FOR_fstatat   case __NR_newfstatat
#define CASES_FOR_fstat   case __NR_fstat
#define CASES_FOR_fstatfs   case __NR_fstatfs
#define CASES_FOR_statfs   case __NR_statfs
#define CASES_FOR_fcntl   case __NR_fcntl
#define CASES_FOR_getdents   case __NR_getdents
#define CASES_FOR_lseek   case __NR_lseek
#define CASES_FOR_ftruncate   case __NR_ftruncate
#endif

#ifdef __NR_sigprocmask
#define CASES_FOR_sigprocmask   case __NR_sigprocmask: case __NR_rt_sigprocmask
#define CASES_FOR_sigaction   case __NR_sigaction: case __NR_rt_sigaction
#define CASES_FOR_sigreturn   case __NR_sigreturn: case __NR_rt_sigreturn
#else
#define CASES_FOR_sigprocmask   case __NR_rt_sigprocmask
#define CASES_FOR_sigaction   case __NR_rt_sigaction
#define CASES_FOR_sigreturn   case __NR_rt_sigreturn
#endif

#ifdef __NR__newselect
#define CASES_FOR_select   case __NR__newselect
#else
#define CASES_FOR_select   case __NR_select
#endif

#ifdef __NR_ugetrlimit
#define CASES_FOR_getrlimit   case __NR_ugetrlimit
#else
#define CASES_FOR_getrlimit   case __NR_getrlimit
#endif

#endif // mozilla_SandboxFilterUtil_h
