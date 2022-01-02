/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SandboxInfo.h"
#include "SandboxLogging.h"
#include "LinuxSched.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"
#include "mozilla/Assertions.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/SandboxSettings.h"
#include "sandbox/linux/system_headers/linux_seccomp.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

#ifdef MOZ_VALGRIND
#  include <valgrind/valgrind.h>
#endif

// A note about assertions: in general, the worst thing this module
// should be able to do is disable sandboxing features, so release
// asserts or MOZ_CRASH should be avoided, even for seeming
// impossibilities like an unimplemented syscall returning success
// (which has happened: https://crbug.com/439795 ).
//
// MOZ_DIAGNOSTIC_ASSERT (debug builds, plus Nightly/Aurora non-debug)
// is probably the best choice for conditions that shouldn't be able
// to fail without the help of bugs in the kernel or system libraries.
//
// Regardless of assertion type, whatever condition caused it to fail
// should generally also disable the corresponding feature on builds
// that omit the assertion.

namespace mozilla {

static bool HasSeccompBPF() {
  // Allow simulating the absence of seccomp-bpf support, for testing.
  if (getenv("MOZ_FAKE_NO_SANDBOX")) {
    return false;
  }

  // Valgrind and the sandbox don't interact well, probably because Valgrind
  // does various system calls which aren't allowed, even if Firefox itself
  // is playing by the rules.
#if defined(MOZ_VALGRIND)
  if (RUNNING_ON_VALGRIND) {
    return false;
  }
#endif

  // Determine whether seccomp-bpf is supported by trying to
  // enable it with an invalid pointer for the filter.  This will
  // fail with EFAULT if supported and EINVAL if not, without
  // changing the process's state.

  int rv = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, nullptr);
  MOZ_DIAGNOSTIC_ASSERT(rv == -1,
                        "prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER,"
                        " nullptr) didn't fail");
  MOZ_DIAGNOSTIC_ASSERT(errno == EFAULT || errno == EINVAL);
  return rv == -1 && errno == EFAULT;
}

static bool HasSeccompTSync() {
  // Similar to above, but for thread-sync mode.  See also Chromium's
  // sandbox::SandboxBPF::SupportsSeccompThreadFilterSynchronization
  if (getenv("MOZ_FAKE_NO_SECCOMP_TSYNC")) {
    return false;
  }
  int rv = syscall(__NR_seccomp, SECCOMP_SET_MODE_FILTER,
                   SECCOMP_FILTER_FLAG_TSYNC, nullptr);
  MOZ_DIAGNOSTIC_ASSERT(rv == -1,
                        "seccomp(..., SECCOMP_FILTER_FLAG_TSYNC,"
                        " nullptr) didn't fail");
  MOZ_DIAGNOSTIC_ASSERT(errno == EFAULT || errno == EINVAL || errno == ENOSYS);
  return rv == -1 && errno == EFAULT;
}

static bool HasUserNamespaceSupport() {
  // Note: the /proc/<pid>/ns/* files track setns(2) support, which in
  // some cases (e.g., pid) significantly postdates kernel support for
  // the namespace type, so in general this type of check could be a
  // false negative.  However, for user namespaces, any kernel new
  // enough for the feature to be usable for us has setns support
  // (v3.8), so this is okay.
  //
  // The non-user namespaces all default to "y" in init/Kconfig, but
  // check them explicitly in case someone has a weird custom config.
  static const char* const paths[] = {
      "/proc/self/ns/user",
      "/proc/self/ns/pid",
      "/proc/self/ns/net",
      "/proc/self/ns/ipc",
  };
  for (size_t i = 0; i < ArrayLength(paths); ++i) {
    if (access(paths[i], F_OK) == -1) {
      MOZ_ASSERT(errno == ENOENT);
      return false;
    }
  }
  return true;
}

static bool CanCreateUserNamespace() {
  // Unfortunately, the only way to verify that this process can
  // create a new user namespace is to actually create one; because
  // this process's namespaces shouldn't be side-effected (yet), it's
  // necessary to clone (and collect) a child process.  See also
  // Chromium's sandbox::Credentials::SupportsNewUserNS.
  //
  // This is somewhat more expensive than the other tests, so it's
  // cached in the environment to prevent child processes from having
  // to re-run the test.
  //
  // This is run at static initializer time, while single-threaded, so
  // locking isn't needed to access the environment.
  static const char kCacheEnvName[] = "MOZ_ASSUME_USER_NS";
  const char* cached = getenv(kCacheEnvName);
  if (cached) {
    return cached[0] > '0';
  }

  // Bug 1434528: In addition to CLONE_NEWUSER, do something that uses
  // the new capabilities (in this case, cloning another namespace) to
  // detect AppArmor policies that allow CLONE_NEWUSER but don't allow
  // doing anything useful with it.
  pid_t pid = syscall(__NR_clone, SIGCHLD | CLONE_NEWUSER | CLONE_NEWPID,
                      nullptr, nullptr, nullptr, nullptr);
  if (pid == 0) {
    // In the child.  Do as little as possible.
    _exit(0);
  }
  if (pid == -1) {
    // Failure.
    MOZ_ASSERT(errno == EINVAL ||  // unsupported
               errno == EPERM ||   // root-only, or we're already chrooted
               errno == EUSERS);   // already at user namespace nesting limit
    setenv(kCacheEnvName, "0", 1);
    return false;
  }
  // Otherwise, in the parent and successful.
  bool waitpid_ok = HANDLE_EINTR(waitpid(pid, nullptr, 0)) == pid;
  MOZ_ASSERT(waitpid_ok);
  if (!waitpid_ok) {
    return false;
  }
  setenv(kCacheEnvName, "1", 1);
  return true;
}

/* static */
const SandboxInfo SandboxInfo::sSingleton = SandboxInfo();

SandboxInfo::SandboxInfo() {
  int flags = 0;
  static_assert(sizeof(flags) >= sizeof(Flags), "enum Flags fits in an int");

  if (HasSeccompBPF()) {
    flags |= kHasSeccompBPF;
    if (HasSeccompTSync()) {
      flags |= kHasSeccompTSync;
    }
  }

  if (HasUserNamespaceSupport()) {
    flags |= kHasPrivilegedUserNamespaces;
    if (CanCreateUserNamespace()) {
      flags |= kHasUserNamespaces;
    }
  }

  // We can't use mozilla::IsContentSandboxEnabled() here because a)
  // libmozsandbox can't depend on libxul, and b) this is called in a static
  // initializer before the prefences service is ready.
  if (!getenv("MOZ_DISABLE_CONTENT_SANDBOX")) {
    flags |= kEnabledForContent;
  }
  if (getenv("MOZ_PERMISSIVE_CONTENT_SANDBOX")) {
    flags |= kPermissive;
  }
  if (!getenv("MOZ_DISABLE_GMP_SANDBOX")) {
    flags |= kEnabledForMedia;
  }
  if (getenv("MOZ_SANDBOX_LOGGING")) {
    flags |= kVerbose;
  }

  mFlags = static_cast<Flags>(flags);
}

}  // namespace mozilla
