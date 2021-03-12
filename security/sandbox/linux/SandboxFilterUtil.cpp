/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SandboxFilterUtil.h"

#ifndef ANDROID
#  include <linux/ipc.h>
#endif
#include <linux/net.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "mozilla/UniquePtr.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"

// Older kernel headers (mostly Android, but also some older desktop
// distributions) are missing some or all of these:
#ifndef SYS_ACCEPT4
#  define SYS_ACCEPT4 18
#endif
#ifndef SYS_RECVMMSG
#  define SYS_RECVMMSG 19
#endif
#ifndef SYS_SENDMMSG
#  define SYS_SENDMMSG 20
#endif

using namespace sandbox::bpf_dsl;
#define CASES SANDBOX_BPF_DSL_CASES

namespace mozilla {

sandbox::bpf_dsl::ResultExpr SandboxPolicyBase::EvaluateSyscall(
    int aSysno) const {
  switch (aSysno) {
#ifdef __NR_socketcall
    case __NR_socketcall: {
      Arg<int> call(0);
      UniquePtr<Caser<int>> acc(new Caser<int>(Switch(call)));
      for (int i = SYS_SOCKET; i <= SYS_SENDMMSG; ++i) {
        auto thisCase = EvaluateSocketCall(i, false);
        // Optimize out cases that are equal to the default.
        if (thisCase) {
          acc.reset(new Caser<int>(acc->Case(i, *thisCase)));
        }
      }
      return acc->Default(InvalidSyscall());
    }
#  ifndef ANDROID
    case __NR_ipc: {
      Arg<int> callAndVersion(0);
      auto call = callAndVersion & 0xFFFF;
      UniquePtr<Caser<int>> acc(new Caser<int>(Switch(call)));
      for (int i = SEMOP; i <= DIPC; ++i) {
        auto thisCase = EvaluateIpcCall(i);
        // Optimize out cases that are equal to the default.
        if (thisCase) {
          acc.reset(new Caser<int>(acc->Case(i, *thisCase)));
        }
      }
      return acc->Default(InvalidSyscall());
    }
#  endif  // ANDROID
#endif    // __NR_socketcall
          // clang-format off
#define DISPATCH_SOCKETCALL(sysnum, socketnum) \
  case sysnum:                                 \
    return EvaluateSocketCall(socketnum, true).valueOr(InvalidSyscall())
#ifdef __NR_socket
      DISPATCH_SOCKETCALL(__NR_socket,      SYS_SOCKET);
      DISPATCH_SOCKETCALL(__NR_bind,        SYS_BIND);
      DISPATCH_SOCKETCALL(__NR_connect,     SYS_CONNECT);
      DISPATCH_SOCKETCALL(__NR_listen,      SYS_LISTEN);
#ifdef __NR_accept
      DISPATCH_SOCKETCALL(__NR_accept,      SYS_ACCEPT);
#endif
      DISPATCH_SOCKETCALL(__NR_getsockname, SYS_GETSOCKNAME);
      DISPATCH_SOCKETCALL(__NR_getpeername, SYS_GETPEERNAME);
      DISPATCH_SOCKETCALL(__NR_socketpair,  SYS_SOCKETPAIR);
#ifdef __NR_send
      DISPATCH_SOCKETCALL(__NR_send,        SYS_SEND);
      DISPATCH_SOCKETCALL(__NR_recv,        SYS_RECV);
#endif  // __NR_send
      DISPATCH_SOCKETCALL(__NR_sendto,      SYS_SENDTO);
      DISPATCH_SOCKETCALL(__NR_recvfrom,    SYS_RECVFROM);
      DISPATCH_SOCKETCALL(__NR_shutdown,    SYS_SHUTDOWN);
      DISPATCH_SOCKETCALL(__NR_setsockopt,  SYS_SETSOCKOPT);
      DISPATCH_SOCKETCALL(__NR_getsockopt,  SYS_GETSOCKOPT);
      DISPATCH_SOCKETCALL(__NR_sendmsg,     SYS_SENDMSG);
      DISPATCH_SOCKETCALL(__NR_recvmsg,     SYS_RECVMSG);
      DISPATCH_SOCKETCALL(__NR_accept4,     SYS_ACCEPT4);
      DISPATCH_SOCKETCALL(__NR_recvmmsg,    SYS_RECVMMSG);
      DISPATCH_SOCKETCALL(__NR_sendmmsg,    SYS_SENDMMSG);
#endif  // __NR_socket
#undef DISPATCH_SOCKETCALL
#ifndef __NR_socketcall
#ifndef ANDROID
#define DISPATCH_SYSVCALL(sysnum, ipcnum) \
  case sysnum:                            \
    return EvaluateIpcCall(ipcnum).valueOr(InvalidSyscall())
      DISPATCH_SYSVCALL(__NR_semop,       SEMOP);
      DISPATCH_SYSVCALL(__NR_semget,      SEMGET);
      DISPATCH_SYSVCALL(__NR_semctl,      SEMCTL);
      DISPATCH_SYSVCALL(__NR_semtimedop,  SEMTIMEDOP);
      DISPATCH_SYSVCALL(__NR_msgsnd,      MSGSND);
      DISPATCH_SYSVCALL(__NR_msgrcv,      MSGRCV);
      DISPATCH_SYSVCALL(__NR_msgget,      MSGGET);
      DISPATCH_SYSVCALL(__NR_msgctl,      MSGCTL);
      DISPATCH_SYSVCALL(__NR_shmat,       SHMAT);
      DISPATCH_SYSVCALL(__NR_shmdt,       SHMDT);
      DISPATCH_SYSVCALL(__NR_shmget,      SHMGET);
      DISPATCH_SYSVCALL(__NR_shmctl,      SHMCTL);
#undef DISPATCH_SYSVCALL
#endif  // ANDROID
#endif  // __NR_socketcall
          // clang-format on
    default:
      return InvalidSyscall();
  }
}

/* static */ bool SandboxPolicyBase::HasSeparateSocketCalls() {
#ifdef __NR_socket
  // If there's no socketcall, then obviously there are separate syscalls.
#  ifdef __NR_socketcall
  // This could be memoized....
  int fd = syscall(__NR_socket, AF_LOCAL, SOCK_STREAM, 0);
  if (fd < 0) {
    MOZ_DIAGNOSTIC_ASSERT(errno == ENOSYS);
    return false;
  }
  close(fd);
#  endif  // __NR_socketcall
  return true;
#else   // ifndef __NR_socket
  return false;
#endif  // __NR_socket
}

}  // namespace mozilla
