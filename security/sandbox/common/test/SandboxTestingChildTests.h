/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "SandboxTestingChild.h"

#include "nsXULAppAPI.h"

#ifdef XP_UNIX
#  include <fcntl.h>
#  include <netdb.h>
#  ifdef XP_LINUX
#    include <sys/prctl.h>
#  endif  // XP_LINUX
#  include <sys/socket.h>
#  include <sys/stat.h>
#  include <sys/types.h>
#  include <time.h>
#  include <unistd.h>
#endif

namespace mozilla {

void RunTestsContent(SandboxTestingChild* child) {
  MOZ_ASSERT(child, "No SandboxTestingChild*?");

#ifdef XP_UNIX
  struct stat st;
  static const char kAllowedPath[] = "/usr/lib";

  child->ErrnoTest("fstatat_as_stat"_ns, true,
                   [&] { return fstatat(AT_FDCWD, kAllowedPath, &st, 0); });
  child->ErrnoTest("fstatat_as_lstat"_ns, true, [&] {
    return fstatat(AT_FDCWD, kAllowedPath, &st, AT_SYMLINK_NOFOLLOW);
  });

#  ifdef XP_LINUX
  child->ErrnoTest("fstatat_as_fstat"_ns, true,
                   [&] { return fstatat(0, "", &st, AT_EMPTY_PATH); });
#  endif  // XP_LINUX

  const struct timespec usec = {0, 1000};
  child->ErrnoTest("nanosleep"_ns, true,
                   [&] { return nanosleep(&usec, nullptr); });

  struct timespec res = {0, 0};
  child->ErrnoTest("clock_getres"_ns, true,
                   [&] { return clock_getres(CLOCK_REALTIME, &res); });

#else   // XP_UNIX
  child->SendReportTestResults(
      "dummy_test"_ns,
      /* shouldSucceed */ true,
      /* didSucceed */ true,
      "The test framework fails if there are no cases."_ns);
#endif  // XP_UNIX
}

void RunTestsSocket(SandboxTestingChild* child) {
  MOZ_ASSERT(child, "No SandboxTestingChild*?");

#ifdef XP_UNIX
  child->ErrnoTest("getaddrinfo"_ns, true, [&] {
    struct addrinfo* res;
    int rv = getaddrinfo("localhost", nullptr, nullptr, &res);
    if (res != nullptr) {
      freeaddrinfo(res);
    }
    return rv;
  });

#  ifdef XP_LINUX
  child->ErrnoTest("prctl_allowed"_ns, true, [&] {
    int rv = prctl(PR_SET_DUMPABLE, 0, 0, 0, 0);
    return rv;
  });

  child->ErrnoTest("prctl_blocked"_ns, false, [&] {
    int rv = prctl(PR_GET_SECCOMP, 0, 0, 0, 0);
    return rv;
  });
#  endif  // XP_LINUX

#else   // XP_UNIX
  child->SendReportTestResults(
      "dummy_test"_ns,
      /* shouldSucceed */ true,
      /* didSucceed */ true,
      "The test framework fails if there are no cases."_ns);
#endif  // XP_UNIX
}

}  // namespace mozilla
