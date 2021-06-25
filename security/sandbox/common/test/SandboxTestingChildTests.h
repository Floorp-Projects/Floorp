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
#    include <sys/ioctl.h>
#    include <termios.h>
#    include <sys/resource.h>
#    include <sys/time.h>
#    include <sys/utsname.h>
#    include <sched.h>
#    include <sys/syscall.h>
#    include <sys/un.h>
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

  const struct timespec usec = {0, 1000};
  child->ErrnoTest("nanosleep"_ns, true,
                   [&] { return nanosleep(&usec, nullptr); });

  struct timespec res = {0, 0};
  child->ErrnoTest("clock_getres"_ns, true,
                   [&] { return clock_getres(CLOCK_REALTIME, &res); });

  // An abstract socket that does not starts with '/', so we don't want it to
  // work.
  // Checking ENETUNREACH should be thrown by SandboxBrokerClient::Connect()
  // when it detects it does not starts with a '/'
  child->ErrnoValueTest("connect_abstract_blocked"_ns, false, ENETUNREACH, [&] {
    int sockfd;
    struct sockaddr_un addr;
    char str[] = "\0xyz";  // Abstract socket requires first byte to be NULL
    size_t str_size = 4;

    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    memcpy(&addr.sun_path, str, str_size);

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd == -1) {
      return -1;
    }

    int con_st = connect(sockfd, (struct sockaddr*)&addr,
                         sizeof(sa_family_t) + str_size);
    return con_st;
  });

  // An abstract socket that does starts with /, so we do want it to work.
  // Checking ECONNREFUSED because this is what the broker should get when
  // trying to establish the connect call for us.
  child->ErrnoValueTest("connect_abstract_permit"_ns, false, ECONNREFUSED, [&] {
    int sockfd;
    struct sockaddr_un addr;
    // we re-use actual X path, because this is what is allowed within
    // SandboxBrokerPolicyFactory::InitContentPolicy()
    // We can't just use any random path allowed, but one with CONNECT allowed.

    // Abstract socket requires first byte to be NULL
    char str[] = "\0/tmp/.X11-unix/X";
    size_t str_size = 17;

    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    memcpy(&addr.sun_path, str, str_size);

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd == -1) {
      return -1;
    }

    int con_st = connect(sockfd, (struct sockaddr*)&addr,
                         sizeof(sa_family_t) + str_size);
    return con_st;
  });
#  endif  // XP_LINUX

#else   // XP_UNIX
  child->ReportNoTests();
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
  child->ReportNoTests();
#endif  // XP_UNIX
}

void RunTestsRDD(SandboxTestingChild* child) {
  MOZ_ASSERT(child, "No SandboxTestingChild*?");

#ifdef XP_UNIX
#  ifdef XP_LINUX
  child->ErrnoValueTest("ioctl_tiocsti"_ns, false, ENOSYS, [&] {
    int rv = ioctl(1, TIOCSTI, "x");
    return rv;
  });

  struct rusage res;
  child->ErrnoTest("getrusage"_ns, true, [&] {
    int rv = getrusage(RUSAGE_SELF, &res);
    return rv;
  });
#  endif  // XP_LINUX
#else     // XP_UNIX
  child->ReportNoTests();
#endif
}

void RunTestsGMPlugin(SandboxTestingChild* child) {
  MOZ_ASSERT(child, "No SandboxTestingChild*?");
#ifdef XP_UNIX
#  ifdef XP_LINUX
  struct utsname utsname_res = {};
  child->ErrnoTest("uname"_ns, true, [&] {
    int rv = uname(&utsname_res);

    nsCString expectedSysname("Linux"_ns);
    nsCString sysname(utsname_res.sysname);
    nsCString expectedVersion("3"_ns);
    nsCString version(utsname_res.version);
    if ((sysname != expectedSysname) || (version != expectedVersion)) {
      return -1;
    }

    return rv;
  });

  child->ErrnoTest("getuid"_ns, true, [&] { return getuid(); });
  child->ErrnoTest("getgid"_ns, true, [&] { return getgid(); });
  child->ErrnoTest("geteuid"_ns, true, [&] { return geteuid(); });
  child->ErrnoTest("getegid"_ns, true, [&] { return getegid(); });

  struct sched_param param_pid_0 = {};
  child->ErrnoTest("sched_getparam(0)"_ns, true,
                   [&] { return sched_getparam(0, &param_pid_0); });

  struct sched_param param_pid_tid = {};
  child->ErrnoTest("sched_getparam(tid)"_ns, true, [&] {
    return sched_getparam((pid_t)syscall(__NR_gettid), &param_pid_tid);
  });

  struct sched_param param_pid_Ntid = {};
  child->ErrnoTest("sched_getparam(Ntid)"_ns, false, [&] {
    return sched_getparam((pid_t)(syscall(__NR_gettid) - 1), &param_pid_Ntid);
  });

  std::vector<std::pair<const char*, bool>> open_tests = {
      {"/etc/ld.so.cache", true},
      {"/proc/cpuinfo", true},
      {"/etc/hostname", false}};

  for (const std::pair<const char*, bool>& to_open : open_tests) {
    child->ErrnoTest("open("_ns + nsCString(to_open.first) + ")"_ns,
                     to_open.second, [&] {
                       int fd = open(to_open.first, O_RDONLY);
                       if (to_open.second && fd > 0) {
                         close(fd);
                       }
                       return fd;
                     });
  }
#  endif  // XP_LINUX
#else     // XP_UNIX
  child->ReportNoTests();
#endif
}

}  // namespace mozilla
