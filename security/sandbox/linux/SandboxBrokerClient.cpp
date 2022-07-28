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
#include <sys/un.h>
#include <unistd.h>

#include "mozilla/Assertions.h"
#include "base/strings/safe_sprintf.h"

namespace mozilla {

SandboxBrokerClient::SandboxBrokerClient(int aFd) : mFileDesc(aFd) {}

SandboxBrokerClient::~SandboxBrokerClient() { close(mFileDesc); }

int SandboxBrokerClient::DoCall(const Request* aReq, const char* aPath,
                                const char* aPath2, void* aResponseBuff,
                                bool expectFd) {
  // Remap /proc/self to the actual pid, so that the broker can open
  // it.  This happens here instead of in the broker to follow the
  // principle of least privilege and keep the broker as simple as
  // possible.  (Note: when pid namespaces happen, this will also need
  // to remap the inner pid to the outer pid.)
  // We only remap the first path.
  static const char kProcSelf[] = "/proc/self/";
  static const size_t kProcSelfLen = sizeof(kProcSelf) - 1;
  const char* path = aPath;
  // This buffer just needs to be large enough for any such path that
  // the policy would actually allow.  sizeof("/proc/2147483647/") == 18.
  char rewrittenPath[64];
  if (strncmp(aPath, kProcSelf, kProcSelfLen) == 0) {
    ssize_t len = base::strings::SafeSPrintf(rewrittenPath, "/proc/%d/%s",
                                             getpid(), aPath + kProcSelfLen);
    if (static_cast<size_t>(len) < sizeof(rewrittenPath)) {
      if (SandboxInfo::Get().Test(SandboxInfo::kVerbose)) {
        SANDBOX_LOG("rewriting %s -> %s", aPath, rewrittenPath);
      }
      path = rewrittenPath;
    } else {
      SANDBOX_LOG("not rewriting unexpectedly long path %s", aPath);
    }
  }

  struct iovec ios[3];
  int respFds[2];

  // Set up iovecs for request + path.
  ios[0].iov_base = const_cast<Request*>(aReq);
  ios[0].iov_len = sizeof(*aReq);
  ios[1].iov_base = const_cast<char*>(path);
  ios[1].iov_len = strlen(path) + 1;
  if (aPath2 != nullptr) {
    ios[2].iov_base = const_cast<char*>(aPath2);
    ios[2].iov_len = strlen(aPath2) + 1;
  } else {
    ios[2].iov_base = nullptr;
    ios[2].iov_len = 0;
  }
  if (ios[1].iov_len > kMaxPathLen) {
    return -ENAMETOOLONG;
  }
  if (ios[2].iov_len > kMaxPathLen) {
    return -ENAMETOOLONG;
  }

  // Create response socket and send request.
  if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, respFds) < 0) {
    return -errno;
  }
  const ssize_t sent = SendWithFd(mFileDesc, ios, 3, respFds[1]);
  const int sendErrno = errno;
  MOZ_ASSERT(sent < 0 || static_cast<size_t>(sent) ==
                             ios[0].iov_len + ios[1].iov_len + ios[2].iov_len);
  close(respFds[1]);
  if (sent < 0) {
    close(respFds[0]);
    return -sendErrno;
  }

  // Set up iovecs for response.
  Response resp;
  ios[0].iov_base = &resp;
  ios[0].iov_len = sizeof(resp);
  if (aResponseBuff) {
    ios[1].iov_base = aResponseBuff;
    ios[1].iov_len = aReq->mBufSize;
  } else {
    ios[1].iov_base = nullptr;
    ios[1].iov_len = 0;
  }

  // Wait for response and return appropriately.
  int openedFd = -1;
  const ssize_t recvd = RecvWithFd(respFds[0], ios, aResponseBuff ? 2 : 1,
                                   expectFd ? &openedFd : nullptr);
  const int recvErrno = errno;
  close(respFds[0]);
  if (recvd < 0) {
    return -recvErrno;
  }
  if (recvd == 0) {
    SANDBOX_LOG("Unexpected EOF, op %d flags 0%o path %s", aReq->mOp,
                aReq->mFlags, path);
    return -EIO;
  }
  MOZ_ASSERT(static_cast<size_t>(recvd) <= ios[0].iov_len + ios[1].iov_len);
  // Some calls such as readlink return a size if successful
  if (resp.mError >= 0) {
    // Success!
    if (expectFd) {
      MOZ_ASSERT(openedFd >= 0);
      return openedFd;
    }
    return resp.mError;
  }
  if (SandboxInfo::Get().Test(SandboxInfo::kVerbose)) {
    // Keep in mind that "rejected" files can include ones that don't
    // actually exist, if it's something that's optional or part of a
    // search path (e.g., shared libraries).  In those cases, this
    // error message is expected.
    SANDBOX_LOG("Failed errno %d op %s flags 0%o path %s", resp.mError,
                OperationDescription[aReq->mOp], aReq->mFlags, path);
  }
  if (openedFd >= 0) {
    close(openedFd);
  }
  return resp.mError;
}

int SandboxBrokerClient::Open(const char* aPath, int aFlags) {
  Request req = {SANDBOX_FILE_OPEN, aFlags, 0};
  int maybeFd = DoCall(&req, aPath, nullptr, nullptr, true);
  if (maybeFd >= 0) {
    // NSPR has opinions about file flags.  Fix O_CLOEXEC.
    if ((aFlags & O_CLOEXEC) == 0) {
      fcntl(maybeFd, F_SETFD, 0);
    }
  }
  return maybeFd;
}

int SandboxBrokerClient::Access(const char* aPath, int aMode) {
  Request req = {SANDBOX_FILE_ACCESS, aMode, 0};
  return DoCall(&req, aPath, nullptr, nullptr, false);
}

int SandboxBrokerClient::Stat(const char* aPath, statstruct* aStat) {
  if (!aPath || !aStat) {
    return -EFAULT;
  }

  Request req = {SANDBOX_FILE_STAT, 0, sizeof(statstruct)};
  return DoCall(&req, aPath, nullptr, (void*)aStat, false);
}

int SandboxBrokerClient::LStat(const char* aPath, statstruct* aStat) {
  if (!aPath || !aStat) {
    return -EFAULT;
  }

  Request req = {SANDBOX_FILE_STAT, O_NOFOLLOW, sizeof(statstruct)};
  return DoCall(&req, aPath, nullptr, (void*)aStat, false);
}

int SandboxBrokerClient::Chmod(const char* aPath, int aMode) {
  Request req = {SANDBOX_FILE_CHMOD, aMode, 0};
  return DoCall(&req, aPath, nullptr, nullptr, false);
}

int SandboxBrokerClient::Link(const char* aOldPath, const char* aNewPath) {
  Request req = {SANDBOX_FILE_LINK, 0, 0};
  return DoCall(&req, aOldPath, aNewPath, nullptr, false);
}

int SandboxBrokerClient::Symlink(const char* aOldPath, const char* aNewPath) {
  Request req = {SANDBOX_FILE_SYMLINK, 0, 0};
  return DoCall(&req, aOldPath, aNewPath, nullptr, false);
}

int SandboxBrokerClient::Rename(const char* aOldPath, const char* aNewPath) {
  Request req = {SANDBOX_FILE_RENAME, 0, 0};
  return DoCall(&req, aOldPath, aNewPath, nullptr, false);
}

int SandboxBrokerClient::Mkdir(const char* aPath, int aMode) {
  Request req = {SANDBOX_FILE_MKDIR, aMode, 0};
  return DoCall(&req, aPath, nullptr, nullptr, false);
}

int SandboxBrokerClient::Unlink(const char* aPath) {
  Request req = {SANDBOX_FILE_UNLINK, 0, 0};
  return DoCall(&req, aPath, nullptr, nullptr, false);
}

int SandboxBrokerClient::Rmdir(const char* aPath) {
  Request req = {SANDBOX_FILE_RMDIR, 0, 0};
  return DoCall(&req, aPath, nullptr, nullptr, false);
}

int SandboxBrokerClient::Readlink(const char* aPath, void* aBuff,
                                  size_t aSize) {
  Request req = {SANDBOX_FILE_READLINK, 0, aSize};
  return DoCall(&req, aPath, nullptr, aBuff, false);
}

int SandboxBrokerClient::Connect(const sockaddr_un* aAddr, size_t aLen,
                                 int aType) {
  static constexpr size_t maxLen = sizeof(aAddr->sun_path);
  const char* path = aAddr->sun_path;
  const auto addrEnd = reinterpret_cast<const char*>(aAddr) + aLen;
  // Ensure that the length isn't impossibly small.
  if (addrEnd <= path) {
    return -EINVAL;
  }
  // Unix domain only
  if (aAddr->sun_family != AF_UNIX) {
    return -EAFNOSUPPORT;
  }
  // How much of sun_path may be accessed?
  auto bufLen = static_cast<size_t>(addrEnd - path);
  if (bufLen > maxLen) {
    bufLen = maxLen;
  }

  // Try to handle abstract addresses where the address (the part
  // after the leading null byte) resembles a pathname: a leading
  // slash and no embedded nulls.
  //
  // `DoCall` expects null-terminated strings, but in this case the
  // "path" is terminated by the sockaddr length (without a null), so
  // we need to make a copy.
  if (bufLen >= 2 && path[0] == '\0' && path[1] == '/' &&
      !memchr(path + 1, '\0', bufLen - 1)) {
    char tmpBuf[maxLen];
    MOZ_RELEASE_ASSERT(bufLen - 1 < maxLen);
    memcpy(tmpBuf, path + 1, bufLen - 1);
    tmpBuf[bufLen - 1] = '\0';

    const Request req = {SANDBOX_SOCKET_CONNECT_ABSTRACT, aType, 0};
    return DoCall(&req, tmpBuf, nullptr, nullptr, true);
  }

  // Require null-termination.  (Linux doesn't require it, but
  // applications usually null-terminate for portability, and not
  // handling unterminated strings means we don't have to copy the path.)
  const size_t pathLen = strnlen(path, bufLen);
  if (pathLen == bufLen) {
    return -ENAMETOOLONG;
  }

  // Abstract addresses are handled only in some specific case, error in others
  if (pathLen == 0) {
    return -ENETUNREACH;
  }

  const Request req = {SANDBOX_SOCKET_CONNECT, aType, 0};
  return DoCall(&req, path, nullptr, nullptr, true);
}

}  // namespace mozilla
