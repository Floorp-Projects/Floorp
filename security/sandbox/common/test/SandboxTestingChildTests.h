/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "SandboxTestingChild.h"

#include "mozilla/StaticPrefs_security.h"
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
#    include "mozilla/ProcInfo_linux.h"
#  endif  // XP_LINUX
#  include <sys/socket.h>
#  include <sys/stat.h>
#  include <sys/types.h>
#  include <time.h>
#  include <unistd.h>
#endif

#ifdef XP_MACOSX
#  include <CoreGraphics/CoreGraphics.h>
#endif

#ifdef XP_WIN
#  include <stdio.h>
#endif

namespace mozilla {

#ifdef XP_LINUX
static void RunTestsSched(SandboxTestingChild* child) {
  struct sched_param param_pid_0 = {};
  child->ErrnoTest("sched_getparam(0)"_ns, true,
                   [&] { return sched_getparam(0, &param_pid_0); });

  struct sched_param param_pid_tid = {};
  child->ErrnoTest("sched_getparam(tid)"_ns, true, [&] {
    return sched_getparam((pid_t)syscall(__NR_gettid), &param_pid_tid);
  });

  struct sched_param param_pid_Ntid = {};
  child->ErrnoValueTest("sched_getparam(Ntid)"_ns, EPERM, [&] {
    return sched_getparam((pid_t)(syscall(__NR_gettid) - 1), &param_pid_Ntid);
  });
}
#endif  // XP_LINUX

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

  // same process is allowed
  struct timespec tproc = {0, 0};
  clockid_t same_process = MAKE_PROCESS_CPUCLOCK(getpid(), CPUCLOCK_SCHED);
  child->ErrnoTest("clock_gettime_same_process"_ns, true,
                   [&] { return clock_gettime(same_process, &tproc); });

  // different process is blocked by sandbox (SIGSYS, kernel would return
  // EINVAL)
  struct timespec tprocd = {0, 0};
  clockid_t diff_process = MAKE_PROCESS_CPUCLOCK(1, CPUCLOCK_SCHED);
  child->ErrnoValueTest("clock_gettime_diff_process"_ns, ENOSYS,
                        [&] { return clock_gettime(diff_process, &tprocd); });

  // thread is allowed
  struct timespec tthread = {0, 0};
  clockid_t thread =
      MAKE_THREAD_CPUCLOCK((pid_t)syscall(__NR_gettid), CPUCLOCK_SCHED);
  child->ErrnoTest("clock_gettime_thread"_ns, true,
                   [&] { return clock_gettime(thread, &tthread); });

  // An abstract socket that does not starts with '/', so we don't want it to
  // work.
  // Checking ENETUNREACH should be thrown by SandboxBrokerClient::Connect()
  // when it detects it does not starts with a '/'
  child->ErrnoValueTest("connect_abstract_blocked"_ns, ENETUNREACH, [&] {
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
  // Checking ECONNREFUSED because this is what the broker should get
  // when trying to establish the connect call for us if it's allowed;
  // otherwise we get EACCES, meaning that it was passed to the broker
  // (unlike the previous test) but rejected.
  const int errorForX =
      StaticPrefs::security_sandbox_content_headless_AtStartup() ? EACCES
                                                                 : ECONNREFUSED;
  child->ErrnoValueTest("connect_abstract_permit"_ns, errorForX, [&] {
    int sockfd;
    struct sockaddr_un addr;
    // we re-use actual X path, because this is what is allowed within
    // SandboxBrokerPolicyFactory::InitContentPolicy()
    // We can't just use any random path allowed, but one with CONNECT allowed.

    // (Note that the real X11 sockets have names like `X0` for
    // display `:0`; there shouldn't be anything named just `X`.)

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

  // Testing FIPS-relevant files, which need to be accessible
  std::vector<std::pair<const char*, bool>> open_tests = {
      {"/dev/random", true}};
  // Not all systems have that file, so we only test access, if it exists
  // in the first place
  if (stat("/proc/sys/crypto/fips_enabled", &st) == 0) {
    open_tests.push_back({"/proc/sys/crypto/fips_enabled", true});
  }

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

#  ifdef XP_MACOSX
  // Test that content processes can not connect to the macOS window server.
  // CGSessionCopyCurrentDictionary() returns NULL when a connection to the
  // window server is not available.
  CFDictionaryRef windowServerDict = CGSessionCopyCurrentDictionary();
  bool gotWindowServerDetails = (windowServerDict != nullptr);
  child->SendReportTestResults(
      "CGSessionCopyCurrentDictionary"_ns, !gotWindowServerDetails,
      gotWindowServerDetails ? "Failed: dictionary unexpectedly returned"_ns
                             : "Succeeded: no dictionary returned"_ns);
  if (windowServerDict != nullptr) {
    CFRelease(windowServerDict);
  }
#  endif

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

  // Testing FIPS-relevant files, which need to be accessible
  std::vector<std::pair<const char*, bool>> open_tests = {
      {"/dev/random", true}};
  // Not all systems have that file, so we only test access, if it exists
  // in the first place
  struct stat st;
  if (stat("/proc/sys/crypto/fips_enabled", &st) == 0) {
    open_tests.push_back({"/proc/sys/crypto/fips_enabled", true});
  }

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

#else   // XP_UNIX
  child->ReportNoTests();
#endif  // XP_UNIX
}

void RunTestsRDD(SandboxTestingChild* child) {
  MOZ_ASSERT(child, "No SandboxTestingChild*?");

#ifdef XP_UNIX
#  ifdef XP_LINUX
  child->ErrnoValueTest("ioctl_tiocsti"_ns, ENOSYS, [&] {
    int rv = ioctl(1, TIOCSTI, "x");
    return rv;
  });

  struct rusage res = {};
  child->ErrnoTest("getrusage"_ns, true, [&] {
    int rv = getrusage(RUSAGE_SELF, &res);
    return rv;
  });

  child->ErrnoValueTest("unlink"_ns, ENOENT, [&] {
    int rv = unlink("");
    return rv;
  });

  child->ErrnoValueTest("unlinkat"_ns, ENOENT, [&] {
    int rv = unlinkat(AT_FDCWD, "", 0);
    return rv;
  });

  RunTestsSched(child);

  child->ErrnoTest("socket"_ns, false,
                   [] { return socket(AF_UNIX, SOCK_STREAM, 0); });

  child->ErrnoTest("uname"_ns, true, [] {
    struct utsname uts;
    return uname(&uts);
  });

  child->ErrnoValueTest("ioctl_dma_buf"_ns, ENOTTY, [] {
    // Apply the ioctl to the wrong kind of fd; it should fail with
    // ENOTTY (rather than ENOSYS if it were blocked).
    return ioctl(0, _IOW('b', 0, uint64_t), nullptr);
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
  child->ErrnoTest("uname_restricted"_ns, true, [&] {
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

  RunTestsSched(child);

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

void RunTestsUtility(SandboxTestingChild* child) {
  MOZ_ASSERT(child, "No SandboxTestingChild*?");

#ifdef XP_UNIX
#  ifdef XP_LINUX
  child->ErrnoValueTest("ioctl_tiocsti"_ns, ENOSYS, [&] {
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
#  ifdef XP_WIN
  child->ErrnoValueTest("write_only"_ns, EACCES, [&] {
    FILE* rv = fopen("test_sandbox.txt", "w");
    if (rv != nullptr) {
      fclose(rv);
      return 0;
    }
    return -1;
  });
#  else   // XP_WIN
  child->ReportNoTests();
#  endif  // XP_WIN
#endif
}

}  // namespace mozilla
