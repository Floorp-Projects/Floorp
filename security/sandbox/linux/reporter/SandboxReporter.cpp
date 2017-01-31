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

#include "mozilla/Assertions.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/PodOperations.h"
#include "nsThreadUtils.h"

namespace mozilla {

StaticAutoPtr<SandboxReporter> SandboxReporter::sSingleton;

SandboxReporter::SandboxReporter()
  : mClientFd(-1)
  , mServerFd(-1)
  , mMutex("SandboxReporter")
  , mBuffer(MakeUnique<SandboxReport[]>(kSandboxReporterBufferSize))
  , mCount(0)
{
}

bool
SandboxReporter::Init()
{
  int fds[2];

  if (0 != socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds)) {
    SANDBOX_LOG_ERROR("SandboxReporter: socketpair failed: %s",
		      strerror(errno));
    return false;
  }
  mClientFd = fds[0];
  mServerFd = fds[1];

  if (!PlatformThread::Create(0, this, &mThread)) {
    SANDBOX_LOG_ERROR("SandboxReporter: thread creation failed: %s",
                      strerror(errno));
    close(mClientFd);
    close(mServerFd);
    mClientFd = mServerFd = -1;
    return false;
  }

  return true;
}

SandboxReporter::~SandboxReporter()
{
  if (mServerFd < 0) {
    return;
  }
  shutdown(mServerFd, SHUT_RD);
  PlatformThread::Join(mThread);
  close(mServerFd);
  close(mClientFd);
}

/* static */ SandboxReporter*
SandboxReporter::Singleton()
{
  static StaticMutex sMutex;
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
    // the only other user is the main-thread-only Troubleshoot.jsm.
    NS_DispatchToMainThread(NS_NewRunnableFunction([] {
      ClearOnShutdown(&sSingleton);
    }));
  }
  return sSingleton.get();
}

void
SandboxReporter::GetClientFileDescriptorMapping(int* aSrcFd, int* aDstFd) const
{
  MOZ_ASSERT(mClientFd >= 0);
  *aSrcFd = mClientFd;
  *aDstFd = kSandboxReporterFileDesc;
}

void
SandboxReporter::AddOne(const SandboxReport& aReport)
{
  // TODO: send a copy to Telemetry
  MutexAutoLock lock(mMutex);
  mBuffer[mCount % kSandboxReporterBufferSize] = aReport;
  ++mCount;
}

void
SandboxReporter::ThreadMain(void)
{
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
      SANDBOX_LOG_ERROR("SandboxReporter: recvmsg: %s", strerror(errno));
    }
    if (recvd <= 0) {
      break;
    }

    if (static_cast<size_t>(recvd) < sizeof(rep)) {
      SANDBOX_LOG_ERROR("SandboxReporter: packet too short (%d < %d)",
			recvd, sizeof(rep));
      continue;
    }
    if (msg.msg_flags & MSG_TRUNC) {
      SANDBOX_LOG_ERROR("SandboxReporter: packet too long");
      continue;
    }

    AddOne(rep);
  }
}

SandboxReporter::Snapshot
SandboxReporter::GetSnapshot()
{
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
  // Named Return Value Optimization would apply here, but C++11
  // doesn't require it; so, instead of possibly copying the entire
  // array contents, invoke the move constructor and copy at most a
  // few words.
  return Move(snapshot);
}

} // namespace mozilla
