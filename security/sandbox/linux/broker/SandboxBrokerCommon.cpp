/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SandboxBrokerCommon.h"

#include "mozilla/Assertions.h"

// This file is built both within libxul and as a separate libmozsandbox
// library. We can only use profiler annotations within libxul.
#ifdef MOZILLA_INTERNAL_API
#  include "mozilla/ProfilerThreadSleep.h"
#else
#  define AUTO_PROFILER_THREAD_SLEEP
#endif

#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#ifndef MSG_CMSG_CLOEXEC
#  ifdef XP_LINUX
// As always, Android's kernel headers are somewhat old.
#    define MSG_CMSG_CLOEXEC 0x40000000
#  else
// Most of this code can support other POSIX OSes, but being able to
// receive fds and atomically make them close-on-exec is important,
// because this is running in a multithreaded process that can fork.
// In the future, if the broker becomes a dedicated executable, this
// can change.
#    error "No MSG_CMSG_CLOEXEC?"
#  endif  // XP_LINUX
#endif    // MSG_CMSG_CLOEXEC

namespace mozilla {

const char* SandboxBrokerCommon::OperationDescription[] = {
    "open",
    "access",
    "stat",
    "chmod",
    "link",
    "symlink",
    "mkdir",
    "rename",
    "rmdir",
    "unlink",
    "readlink",
    "connect",
    "connect-abstract",
};

/* static */
ssize_t SandboxBrokerCommon::RecvWithFd(int aFd, const iovec* aIO,
                                        size_t aNumIO, int* aPassedFdPtr) {
  struct msghdr msg = {};
  msg.msg_iov = const_cast<iovec*>(aIO);
  msg.msg_iovlen = aNumIO;

  char cmsg_buf[CMSG_SPACE(sizeof(int))];
  if (aPassedFdPtr) {
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);
    *aPassedFdPtr = -1;
  }

  ssize_t rv;
  do {
    // MSG_CMSG_CLOEXEC is needed to prevent the parent process from
    // accidentally leaking a copy of the child's response socket to a
    // new child process.  (The child won't be able to exec, so this
    // doesn't matter as much for that direction.)
    AUTO_PROFILER_THREAD_SLEEP;
    rv = recvmsg(aFd, &msg, MSG_CMSG_CLOEXEC);
  } while (rv < 0 && errno == EINTR);

  if (rv <= 0) {
    return rv;
  }
  if (msg.msg_controllen > 0) {
    MOZ_ASSERT(aPassedFdPtr);
    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
      int* fds = reinterpret_cast<int*>(CMSG_DATA(cmsg));
      if (cmsg->cmsg_len != CMSG_LEN(sizeof(int))) {
        // A client could, for example, send an extra 32-bit int if
        // CMSG_SPACE pads to 64-bit size_t alignment.  If so, treat
        // it as an error, but also don't leak the fds.
        for (size_t i = 0; CMSG_LEN(sizeof(int) * i) < cmsg->cmsg_len; ++i) {
          close(fds[i]);
        }
        // In theory, the kernel should delete the message instead of
        // giving us an empty one, if errors prevent transferring the
        // fd.
        MOZ_DIAGNOSTIC_ASSERT(cmsg->cmsg_len != 0);
        errno = EPROTO;
        return -1;
      }
      *aPassedFdPtr = fds[0];
    } else {
      errno = EPROTO;
      return -1;
    }
  }
  if (msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) {
    if (aPassedFdPtr && *aPassedFdPtr >= 0) {
      close(*aPassedFdPtr);
      *aPassedFdPtr = -1;
    }
    // MSG_CTRUNC usually means the attached fd was dropped due to fd
    // exhaustion in the receiving process, so map that to `EMFILE`.
    // (It could also happen if the other process maliciously sends
    // too many fds.)
    //
    // MSG_TRUNC (truncation of the data part) shouldn't ever happen.
    // However, it has happened in the past, due to accidentally
    // sending more data than the receiver was expecting.  We assert
    // that that doesn't happen (and, if it does, try to map it to a
    // vaguely sensible error code).
    MOZ_DIAGNOSTIC_ASSERT((msg.msg_flags & MSG_TRUNC) == 0);
    errno = (msg.msg_flags & MSG_CTRUNC) ? EMFILE : EPROTO;
    return -1;
  }

  return rv;
}

/* static */
ssize_t SandboxBrokerCommon::SendWithFd(int aFd, const iovec* aIO,
                                        size_t aNumIO, int aPassedFd) {
  struct msghdr msg = {};
  msg.msg_iov = const_cast<iovec*>(aIO);
  msg.msg_iovlen = aNumIO;

  char cmsg_buf[CMSG_SPACE(sizeof(int))];
  memset(cmsg_buf, 0, sizeof(cmsg_buf));
  if (aPassedFd != -1) {
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);
    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    *reinterpret_cast<int*>(CMSG_DATA(cmsg)) = aPassedFd;
  }

  ssize_t rv;
  do {
    rv = sendmsg(aFd, &msg, MSG_NOSIGNAL);
  } while (rv < 0 && errno == EINTR);

  return rv;
}

}  // namespace mozilla
