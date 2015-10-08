/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxBrokerTypes_h
#define mozilla_SandboxBrokerTypes_h

#include <sys/types.h>

struct iovec;

// This file defines the protocol between the filesystem broker,
// described in SandboxBroker.h, and its client, described in
// ../SandboxBrokerClient.h; and it defines some utility functions
// used by both.
//
// In order to keep the client simple while allowing it to be thread
// safe and async signal safe, the main broker socket is used only for
// requests; responses arrive on a per-request socketpair sent with
// the request.  (This technique is also used by Chromium and Breakpad.)

namespace mozilla {

class SandboxBrokerCommon {
public:
  enum Operation {
    SANDBOX_FILE_OPEN,
    SANDBOX_FILE_ACCESS,
    SANDBOX_FILE_STAT,
  };

  struct Request {
    Operation mOp;
    // For open, flags; for access, "mode"; for stat, O_NOFOLLOW for lstat.
    int mFlags;
    // The rest of the packet is the pathname.
    // SCM_RIGHTS for response socket attached.
  };

  struct Response {
    int mError; // errno, or 0 for no error
    // Followed by struct stat for stat/lstat.
    // SCM_RIGHTS attached for successful open.
  };

  // This doesn't need to be the system's maximum path length, just
  // the largest path that would be allowed by any policy.  (It's used
  // to size a stack-allocated buffer.)
  static const size_t kMaxPathLen = 4096;

  static ssize_t RecvWithFd(int aFd, const iovec* aIO, size_t aNumIO,
                            int* aPassedFdPtr);
  static ssize_t SendWithFd(int aFd, const iovec* aIO, size_t aNumIO,
                            int aPassedFd);
};

} // namespace mozilla

#endif // mozilla_SandboxBrokerTypes_h
