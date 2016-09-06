/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SandboxLogging.h"

#ifdef ANDROID
#include <android/log.h>
#endif
#include <algorithm>
#include <stdio.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"

namespace mozilla {

// Alters an iovec array to remove the first `toDrop` bytes.  This
// complexity is necessary because writev can return a short write
// (e.g., if stderr is a pipe and the buffer is almost full).
static void
IOVecDrop(struct iovec* iov, int iovcnt, size_t toDrop)
{
  while (toDrop > 0 && iovcnt > 0) {
    size_t toDropHere = std::min(toDrop, iov->iov_len);
    iov->iov_base = static_cast<char*>(iov->iov_base) + toDropHere;
    iov->iov_len -= toDropHere;
    toDrop -= toDropHere;
    ++iov;
    --iovcnt;
  }
}

void
SandboxLogError(const char* message)
{
#ifdef ANDROID
  // This uses writev internally and appears to be async signal safe.
  __android_log_write(ANDROID_LOG_ERROR, "Sandbox", message);
#endif
  static const char logPrefix[] = "Sandbox: ", logSuffix[] = "\n";
  struct iovec iovs[3] = {
    { const_cast<char*>(logPrefix), sizeof(logPrefix) - 1 },
    { const_cast<char*>(message), strlen(message) },
    { const_cast<char*>(logSuffix), sizeof(logSuffix) - 1 },
  };
  while (iovs[2].iov_len > 0) {
    ssize_t written = HANDLE_EINTR(writev(STDERR_FILENO, iovs, 3));
    if (written <= 0) {
      break;
    }
    IOVecDrop(iovs, 3, static_cast<size_t>(written));
  }
}

}
