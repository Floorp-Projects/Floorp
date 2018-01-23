/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SandboxReporterClient.h"
#include "SandboxLogging.h"

#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>

#include "mozilla/Assertions.h"
#include "mozilla/PodOperations.h"
#include "prenv.h"
#include "sandbox/linux/bpf_dsl/seccomp_macros.h"
#ifdef ANDROID
#include "sandbox/linux/system_headers/linux_ucontext.h"
#else
#include <ucontext.h>
#endif

namespace mozilla {

SandboxReporterClient::SandboxReporterClient(SandboxReport::ProcType aProcType,
					     int aFd)
  : mProcType(aProcType)
  , mFd(aFd)
{
  // Unfortunately, there isn't a good way to check that the fd is a
  // socket connected to the right thing without attempting some kind
  // of in-band handshake.  However, the crash reporter (which also
  // uses a "magic number" fd) doesn't do any kind of checking either,
  // so it's probably okay to skip it here.
}

SandboxReporterClient::SandboxReporterClient(SandboxReport::ProcType aProcType)
  : SandboxReporterClient(aProcType, kSandboxReporterFileDesc)
{
  MOZ_RELEASE_ASSERT(PR_GetEnv("MOZ_SANDBOXED") != nullptr);
}

SandboxReport
SandboxReporterClient::MakeReport(const void* aContext)
{
  SandboxReport report;
  const auto ctx = static_cast<const ucontext_t*>(aContext);

  // Zero the entire struct; some memory safety analyses care about
  // sending uninitialized alignment padding to another process.
  PodZero(&report);

  clock_gettime(CLOCK_MONOTONIC_COARSE, &report.mTime);
  report.mPid = getpid();
  report.mTid = syscall(__NR_gettid);
  report.mProcType = mProcType;
  report.mSyscall = SECCOMP_SYSCALL(ctx);
  report.mArgs[0] = SECCOMP_PARM1(ctx);
  report.mArgs[1] = SECCOMP_PARM2(ctx);
  report.mArgs[2] = SECCOMP_PARM3(ctx);
  report.mArgs[3] = SECCOMP_PARM4(ctx);
  report.mArgs[4] = SECCOMP_PARM5(ctx);
  report.mArgs[5] = SECCOMP_PARM6(ctx);
  // Named Return Value Optimization allows the compiler to optimize
  // out the copy here (and the one in MakeReportAndSend).
  return report;
}

void
SandboxReporterClient::SendReport(const SandboxReport& aReport)
{
  // The "common" seccomp-bpf policy allows sendmsg but not send(to),
  // so just use sendmsg even though send would suffice for this.
  struct iovec iov;
  struct msghdr msg;

  iov.iov_base = const_cast<void*>(static_cast<const void*>(&aReport));
  iov.iov_len = sizeof(SandboxReport);
  PodZero(&msg);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  const auto sent = sendmsg(mFd, &msg, MSG_NOSIGNAL);

  if (sent != sizeof(SandboxReport)) {
    MOZ_DIAGNOSTIC_ASSERT(sent == -1);
    SANDBOX_LOG_ERROR("Failed to report rejected syscall: %s",
		      strerror(errno));
  }
}

} // namespace mozilla
