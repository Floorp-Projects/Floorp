/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SandboxLogging.h"

#ifdef ANDROID
#  include <android/log.h>
#endif
#include <algorithm>
#include <stdio.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"

#ifdef __GLIBC_PREREQ
#  if __GLIBC_PREREQ(2, 32)
#    define HAVE_STRERRORNAME_NP 1
#  endif
#endif

namespace mozilla {

// Alters an iovec array to remove the first `toDrop` bytes.  This
// complexity is necessary because writev can return a short write
// (e.g., if stderr is a pipe and the buffer is almost full).
static void IOVecDrop(struct iovec* iov, int iovcnt, size_t toDrop) {
  while (toDrop > 0 && iovcnt > 0) {
    size_t toDropHere = std::min(toDrop, iov->iov_len);
    iov->iov_base = static_cast<char*>(iov->iov_base) + toDropHere;
    iov->iov_len -= toDropHere;
    toDrop -= toDropHere;
    ++iov;
    --iovcnt;
  }
}

void SandboxLogError(const char* message) {
#ifdef ANDROID
  // This uses writev internally and appears to be async signal safe.
  __android_log_write(ANDROID_LOG_ERROR, "Sandbox", message);
#endif
  static const char logPrefix[] = "Sandbox: ", logSuffix[] = "\n";
  struct iovec iovs[3] = {
      {const_cast<char*>(logPrefix), sizeof(logPrefix) - 1},
      {const_cast<char*>(message), strlen(message)},
      {const_cast<char*>(logSuffix), sizeof(logSuffix) - 1},
  };
  while (iovs[2].iov_len > 0) {
    ssize_t written = HANDLE_EINTR(writev(STDERR_FILENO, iovs, 3));
    if (written <= 0) {
      break;
    }
    IOVecDrop(iovs, 3, static_cast<size_t>(written));
  }
}

ssize_t GetLibcErrorName(char* aBuf, size_t aSize, int aErr) {
  const char* errStr;

#ifdef HAVE_STRERRORNAME_NP
  // This is the function we'd like to have, but it's a glibc
  // extension and present only in newer versions.
  errStr = strerrorname_np(aErr);
#else
  // Otherwise, handle most of the basic / common errors with a big
  // switch statement.
  switch (aErr) {

#  define HANDLE_ERR(x) \
    case x:             \
      errStr = #x;      \
      break

    // errno-base.h
    HANDLE_ERR(EPERM);
    HANDLE_ERR(ENOENT);
    HANDLE_ERR(ESRCH);
    HANDLE_ERR(EINTR);
    HANDLE_ERR(EIO);
    HANDLE_ERR(ENXIO);
    HANDLE_ERR(E2BIG);
    HANDLE_ERR(ENOEXEC);
    HANDLE_ERR(EBADF);
    HANDLE_ERR(ECHILD);
    HANDLE_ERR(EAGAIN);
    HANDLE_ERR(ENOMEM);
    HANDLE_ERR(EACCES);
    HANDLE_ERR(EFAULT);
    HANDLE_ERR(ENOTBLK);
    HANDLE_ERR(EBUSY);
    HANDLE_ERR(EEXIST);
    HANDLE_ERR(EXDEV);
    HANDLE_ERR(ENODEV);
    HANDLE_ERR(ENOTDIR);
    HANDLE_ERR(EISDIR);
    HANDLE_ERR(EINVAL);
    HANDLE_ERR(ENFILE);
    HANDLE_ERR(EMFILE);
    HANDLE_ERR(ENOTTY);
    HANDLE_ERR(ETXTBSY);
    HANDLE_ERR(EFBIG);
    HANDLE_ERR(ENOSPC);
    HANDLE_ERR(ESPIPE);
    HANDLE_ERR(EROFS);
    HANDLE_ERR(EMLINK);
    HANDLE_ERR(EPIPE);
    HANDLE_ERR(EDOM);
    HANDLE_ERR(ERANGE);

    // selected other errors
    HANDLE_ERR(ENAMETOOLONG);
    HANDLE_ERR(ENOSYS);
    HANDLE_ERR(ENOTEMPTY);
    HANDLE_ERR(ELOOP);
    HANDLE_ERR(ENOTSOCK);
    HANDLE_ERR(EMSGSIZE);
    HANDLE_ERR(ECONNRESET);
    HANDLE_ERR(ECONNREFUSED);
    HANDLE_ERR(EHOSTUNREACH);
    HANDLE_ERR(ESTALE);

#  undef HANDLE_ERR

    default:
      errStr = nullptr;
  }
#endif  // no strerrorname_np

  if (errStr) {
    return base::strings::SafeSNPrintf(aBuf, aSize, "%s", errStr);
  }

  return base::strings::SafeSNPrintf(aBuf, aSize, "error %d", aErr);
}

}  // namespace mozilla
