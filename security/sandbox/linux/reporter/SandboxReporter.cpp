/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SandboxReporter.h"
#include "SandboxLogging.h"

#include <algorithm>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>  // for clockid_t

#include "GeckoProfiler.h"
#include "mozilla/Assertions.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/PodOperations.h"
#include "nsThreadUtils.h"
#include "mozilla/Telemetry.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

// Distinguish architectures for the telemetry key.
#if defined(__i386__)
#  define SANDBOX_ARCH_NAME "x86"
#elif defined(__x86_64__)
#  define SANDBOX_ARCH_NAME "amd64"
#elif defined(__arm__)
#  define SANDBOX_ARCH_NAME "arm"
#elif defined(__aarch64__)
#  define SANDBOX_ARCH_NAME "arm64"
#else
#  error "unrecognized architecture"
#endif

namespace mozilla {

StaticAutoPtr<SandboxReporter> SandboxReporter::sSingleton;

SandboxReporter::SandboxReporter()
    : mClientFd(-1),
      mServerFd(-1),
      mMutex("SandboxReporter"),
      mBuffer(MakeUnique<SandboxReport[]>(kSandboxReporterBufferSize)),
      mCount(0) {}

bool SandboxReporter::Init() {
  int fds[2];

  if (0 != socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, fds)) {
    SANDBOX_LOG_ERRNO("SandboxReporter: socketpair failed");
    return false;
  }
  mClientFd = fds[0];
  mServerFd = fds[1];

  if (!PlatformThread::Create(0, this, &mThread)) {
    SANDBOX_LOG_ERRNO("SandboxReporter: thread creation failed");
    close(mClientFd);
    close(mServerFd);
    mClientFd = mServerFd = -1;
    return false;
  }

  return true;
}

SandboxReporter::~SandboxReporter() {
  if (mServerFd < 0) {
    return;
  }
  shutdown(mServerFd, SHUT_RD);
  PlatformThread::Join(mThread);
  close(mServerFd);
  close(mClientFd);
}

/* static */
SandboxReporter* SandboxReporter::Singleton() {
  static StaticMutex sMutex MOZ_UNANNOTATED;
  StaticMutexAutoLock lock(sMutex);

  if (sSingleton == nullptr) {
    sSingleton = new SandboxReporter();
    if (!sSingleton->Init()) {
      // If socketpair or thread creation failed, trying to continue
      // with child process creation is unlikely to succeed; crash
      // instead of trying to handle that case.
      MOZ_CRASH("SandboxRepoter::Singleton: initialization failed");
    }
    // ClearOnShutdown must be called on the main thread and will
    // destroy the object on the main thread.  That *should* be safe;
    // the destructor will shut down the reporter's socket reader
    // thread before freeing anything, IPC should already be shut down
    // by that point (so it won't race by calling Singleton()), all
    // non-main XPCOM threads will also be shut down, and currently
    // the only other user is the main-thread-only Troubleshoot.sys.mjs.
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "SandboxReporter::Singleton", [] { ClearOnShutdown(&sSingleton); }));
  }
  return sSingleton.get();
}

void SandboxReporter::GetClientFileDescriptorMapping(int* aSrcFd,
                                                     int* aDstFd) const {
  MOZ_ASSERT(mClientFd >= 0);
  *aSrcFd = mClientFd;
  *aDstFd = kSandboxReporterFileDesc;
}

// This function is mentioned in Histograms.json; keep that in mind if
// it's renamed or moved to a different file.
static void SubmitToTelemetry(const SandboxReport& aReport) {
  nsAutoCString key;
  // The key contains the process type, something that uniquely
  // identifies the syscall, and in some cases arguments (see below
  // for details).  Arbitrary formatting choice: fields in the key are
  // separated by ':', except that (arch, syscall#) pairs are
  // separated by '/'.
  //
  // Examples:
  // * "content:x86/64"           (bug 1285768)
  // * "content:x86_64/110"       (bug 1285768)
  // * "gmp:madvise:8"            (bug 1303813)
  // * "content:clock_gettime:4"  (bug 1334687)

  switch (aReport.mProcType) {
    case SandboxReport::ProcType::CONTENT:
      key.AppendLiteral("content");
      break;
    case SandboxReport::ProcType::FILE:
      key.AppendLiteral("file");
      break;
    case SandboxReport::ProcType::MEDIA_PLUGIN:
      key.AppendLiteral("gmp");
      break;
    case SandboxReport::ProcType::RDD:
      key.AppendLiteral("rdd");
      break;
    case SandboxReport::ProcType::SOCKET_PROCESS:
      key.AppendLiteral("socket");
      break;
    case SandboxReport::ProcType::UTILITY:
      key.AppendLiteral("utility");
      break;
    default:
      MOZ_ASSERT(false);
  }
  key.Append(':');

  switch (aReport.mSyscall) {
    // Syscalls that are filtered by arguments in one or more of the
    // policies in SandboxFilter.cpp should generally have those
    // arguments included here, but don't include irrelevant
    // information that would cause large numbers of distinct keys for
    // the same issue -- for example, pids or pointers.  When in
    // doubt, include arguments only if they would typically be
    // constants (or asm immediates) in the code making the syscall.
    //
    // Also, keep in mind that this is opt-out data collection and
    // privacy is critical.  While it's unlikely that information in
    // the register values alone could personally identify a user
    // (see also crash reports, where register contents are public),
    // and the guidelines in the previous paragraph should rule out
    // any value that's capable of holding PII, please be careful.
    //
    // When making changes here, please consult with a data steward
    // (https://wiki.mozilla.org/Firefox/Data_Collection) and ask for
    // a review if you are unsure about anything.

    // This macro includes one argument as a decimal number; it should
    // be enough for most cases.
#define ARG_DECIMAL(name, idx)         \
  case __NR_##name:                    \
    key.AppendLiteral(#name ":");      \
    key.AppendInt(aReport.mArgs[idx]); \
    break

    // This may be more convenient if the argument is a set of bit flags.
#define ARG_HEX(name, idx)                 \
  case __NR_##name:                        \
    key.AppendLiteral(#name ":0x");        \
    key.AppendInt(aReport.mArgs[idx], 16); \
    break

    // clockid_t is annoying: there are a small set of fixed timers,
    // but it can also encode a pid/tid (or a fd for a hardware clock
    // device); in this case the value is negative.
#define ARG_CLOCKID(name, idx)                            \
  case __NR_##name:                                       \
    key.AppendLiteral(#name ":");                         \
    if (static_cast<clockid_t>(aReport.mArgs[idx]) < 0) { \
      key.AppendLiteral("dynamic");                       \
    } else {                                              \
      key.AppendInt(aReport.mArgs[idx]);                  \
    }                                                     \
    break

    // The syscalls handled specially:

    ARG_HEX(clone, 0);              // flags
    ARG_DECIMAL(prctl, 0);          // option
    ARG_HEX(ioctl, 1);              // request
    ARG_DECIMAL(fcntl, 1);          // cmd
    ARG_DECIMAL(madvise, 2);        // advice
    ARG_CLOCKID(clock_gettime, 0);  // clk_id

#ifdef __NR_socketcall
    ARG_DECIMAL(socketcall, 0);  // call
#endif
#ifdef __NR_ipc
    ARG_DECIMAL(ipc, 0);  // call
#endif

#undef ARG_DECIMAL
#undef ARG_HEX
#undef ARG_CLOCKID

    default:
      // Otherwise just use the number, with the arch name to disambiguate.
      key.Append(SANDBOX_ARCH_NAME "/");
      key.AppendInt(aReport.mSyscall);
  }

  Telemetry::Accumulate(Telemetry::SANDBOX_REJECTED_SYSCALLS, key);
}

void SandboxReporter::AddOne(const SandboxReport& aReport) {
  SubmitToTelemetry(aReport);

  MutexAutoLock lock(mMutex);
  mBuffer[mCount % kSandboxReporterBufferSize] = aReport;
  ++mCount;
}

void SandboxReporter::ThreadMain(void) {
  // Create a nsThread wrapper for the current platform thread, and register it
  // with the thread manager.
  (void)NS_GetCurrentThread();

  PlatformThread::SetName("SandboxReporter");
  AUTO_PROFILER_REGISTER_THREAD("SandboxReporter");

  for (;;) {
    SandboxReport rep;
    struct iovec iov;
    struct msghdr msg;

    iov.iov_base = &rep;
    iov.iov_len = sizeof(rep);
    PodZero(&msg);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    const auto recvd = recvmsg(mServerFd, &msg, 0);
    if (recvd < 0) {
      if (errno == EINTR) {
        continue;
      }
      SANDBOX_LOG_ERRNO("SandboxReporter: recvmsg");
    }
    if (recvd <= 0) {
      break;
    }

    if (static_cast<size_t>(recvd) < sizeof(rep)) {
      SANDBOX_LOG("SandboxReporter: packet too short (%d < %d)", recvd,
                  sizeof(rep));
      continue;
    }
    if (msg.msg_flags & MSG_TRUNC) {
      SANDBOX_LOG("SandboxReporter: packet too long");
      continue;
    }

    AddOne(rep);
  }
}

SandboxReporter::Snapshot SandboxReporter::GetSnapshot() {
  Snapshot snapshot;
  MutexAutoLock lock(mMutex);

  const uint64_t bufSize = static_cast<uint64_t>(kSandboxReporterBufferSize);
  const uint64_t start = std::max(mCount, bufSize) - bufSize;
  snapshot.mOffset = start;
  snapshot.mReports.Clear();
  snapshot.mReports.SetCapacity(mCount - start);
  for (size_t i = start; i < mCount; ++i) {
    const SandboxReport* rep = &mBuffer[i % kSandboxReporterBufferSize];
    MOZ_ASSERT(rep->IsValid());
    snapshot.mReports.AppendElement(*rep);
  }
  return snapshot;
}

}  // namespace mozilla
