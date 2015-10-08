/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SandboxBrokerClient.h"
#include "SandboxInfo.h"
#include "SandboxLogging.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "mozilla/Assertions.h"
#include "mozilla/NullPtr.h"
#include "base/strings/safe_sprintf.h"

namespace mozilla {

SandboxBrokerClient::SandboxBrokerClient(int aFd)
: mFileDesc(aFd)
{
}

SandboxBrokerClient::~SandboxBrokerClient()
{
  close(mFileDesc);
}

int
SandboxBrokerClient::DoCall(const Request* aReq, const char* aPath,
                            struct stat* aStat, bool expectFd)
{
  // Remap /proc/self to the actual pid, so that the broker can open
  // it.  This happens here instead of in the broker to follow the
  // principle of least privilege and keep the broker as simple as
  // possible.  (Note: when pid namespaces happen, this will also need
  // to remap the inner pid to the outer pid.)
  static const char kProcSelf[] = "/proc/self/";
  static const size_t kProcSelfLen = sizeof(kProcSelf) - 1;
  const char* path = aPath;
  // This buffer just needs to be large enough for any such path that
  // the policy would actually allow.  sizeof("/proc/2147483647/") == 18.
  char rewrittenPath[64];
  if (strncmp(aPath, kProcSelf, kProcSelfLen) == 0) {
    ssize_t len =
      base::strings::SafeSPrintf(rewrittenPath, "/proc/%d/%s",
                                 getpid(), aPath + kProcSelfLen);
    if (static_cast<size_t>(len) < sizeof(rewrittenPath)) {
      if (SandboxInfo::Get().Test(SandboxInfo::kVerbose)) {
        SANDBOX_LOG_ERROR("rewriting %s -> %s", aPath, rewrittenPath);
      }
      path = rewrittenPath;
    } else {
      SANDBOX_LOG_ERROR("not rewriting unexpectedly long path %s", aPath);
    }
  }

  struct iovec ios[2];
  int respFds[2];

  // Set up iovecs for request + path.
  ios[0].iov_base = const_cast<Request*>(aReq);
  ios[0].iov_len = sizeof(*aReq);
  ios[1].iov_base = const_cast<char*>(path);
  ios[1].iov_len = strlen(path);
  if (ios[1].iov_len > kMaxPathLen) {
    return -ENAMETOOLONG;
  }

  // Create response socket and send request.
  if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, respFds) < 0) {
    return -errno;
  }
  const ssize_t sent = SendWithFd(mFileDesc, ios, 2, respFds[1]);
  const int sendErrno = errno;
  MOZ_ASSERT(sent < 0 ||
	     static_cast<size_t>(sent) == ios[0].iov_len + ios[1].iov_len);
  close(respFds[1]);
  if (sent < 0) {
    close(respFds[0]);
    return -sendErrno;
  }

  // Set up iovecs for response.
  Response resp;
  ios[0].iov_base = &resp;
  ios[0].iov_len = sizeof(resp);
  if (aStat) {
    ios[1].iov_base = aStat;
    ios[1].iov_len = sizeof(*aStat);
  } else {
    ios[1].iov_base = nullptr;
    ios[1].iov_len = 0;
  }

  // Wait for response and return appropriately.
  int openedFd = -1;
  const ssize_t recvd = RecvWithFd(respFds[0], ios, aStat ? 2 : 1,
                                   expectFd ? &openedFd : nullptr);
  const int recvErrno = errno;
  close(respFds[0]);
  if (recvd < 0) {
    return -recvErrno;
  }
  if (recvd == 0) {
    SANDBOX_LOG_ERROR("Unexpected EOF, op %d flags 0%o path %s",
                      aReq->mOp, aReq->mFlags, path);
    return -EIO;
  }
  if (resp.mError != 0) {
    // If the operation fails, the return payload will be empty;
    // adjust the iov_len for the following assertion.
    ios[1].iov_len = 0;
  }
  MOZ_ASSERT(static_cast<size_t>(recvd) == ios[0].iov_len + ios[1].iov_len);
  if (resp.mError == 0) {
    // Success!
    if (expectFd) {
      MOZ_ASSERT(openedFd >= 0);
      return openedFd;
    }
    return 0;
  }
  if (SandboxInfo::Get().Test(SandboxInfo::kVerbose)) {
    // Keep in mind that "rejected" files can include ones that don't
    // actually exist, if it's something that's optional or part of a
    // search path (e.g., shared libraries).  In those cases, this
    // error message is expected.
    SANDBOX_LOG_ERROR("Rejected errno %d op %d flags 0%o path %s",
                      resp.mError, aReq->mOp, aReq->mFlags, path);
  }
  if (openedFd >= 0) {
    close(openedFd);
  }
  return -resp.mError;
}

int
SandboxBrokerClient::Open(const char* aPath, int aFlags)
{
  Request req = { SANDBOX_FILE_OPEN, aFlags };
  int maybeFd = DoCall(&req, aPath, nullptr, true);
  if (maybeFd >= 0) {
    // NSPR has opinions about file flags.  Fix O_CLOEXEC.
    if ((aFlags & O_CLOEXEC) == 0) {
      fcntl(maybeFd, F_SETFD, 0);
    }
  }
  return maybeFd;
}

int
SandboxBrokerClient::Access(const char* aPath, int aMode)
{
  Request req = { SANDBOX_FILE_ACCESS, aMode };
  return DoCall(&req, aPath, nullptr, false);
}

int
SandboxBrokerClient::Stat(const char* aPath, struct stat* aStat)
{
  Request req = { SANDBOX_FILE_STAT, 0 };
  return DoCall(&req, aPath, aStat, false);
}

int
SandboxBrokerClient::LStat(const char* aPath, struct stat* aStat)
{
  Request req = { SANDBOX_FILE_STAT, O_NOFOLLOW };
  return DoCall(&req, aPath, aStat, false);
}

} // namespace mozilla

