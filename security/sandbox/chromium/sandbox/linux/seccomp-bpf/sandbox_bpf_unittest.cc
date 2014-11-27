// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#if defined(ANDROID)
// Work-around for buggy headers in Android's NDK
#define __user
#endif
#include <linux/futex.h>

#include <ostream>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "sandbox/linux/seccomp-bpf/bpf_tests.h"
#include "sandbox/linux/seccomp-bpf/syscall.h"
#include "sandbox/linux/seccomp-bpf/trap.h"
#include "sandbox/linux/seccomp-bpf/verifier.h"
#include "sandbox/linux/services/broker_process.h"
#include "sandbox/linux/services/linux_syscalls.h"
#include "sandbox/linux/tests/scoped_temporary_file.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

// Workaround for Android's prctl.h file.
#ifndef PR_GET_ENDIAN
#define PR_GET_ENDIAN 19
#endif
#ifndef PR_CAPBSET_READ
#define PR_CAPBSET_READ 23
#define PR_CAPBSET_DROP 24
#endif

namespace sandbox {

namespace {

const int kExpectedReturnValue = 42;
const char kSandboxDebuggingEnv[] = "CHROME_SANDBOX_DEBUGGING";

// Set the global environment to allow the use of UnsafeTrap() policies.
void EnableUnsafeTraps() {
  // The use of UnsafeTrap() causes us to print a warning message. This is
  // generally desirable, but it results in the unittest failing, as it doesn't
  // expect any messages on "stderr". So, temporarily disable messages. The
  // BPF_TEST() is guaranteed to turn messages back on, after the policy
  // function has completed.
  setenv(kSandboxDebuggingEnv, "t", 0);
  Die::SuppressInfoMessages(true);
}

// This test should execute no matter whether we have kernel support. So,
// we make it a TEST() instead of a BPF_TEST().
TEST(SandboxBPF, DISABLE_ON_TSAN(CallSupports)) {
  // We check that we don't crash, but it's ok if the kernel doesn't
  // support it.
  bool seccomp_bpf_supported =
      SandboxBPF::SupportsSeccompSandbox(-1) == SandboxBPF::STATUS_AVAILABLE;
  // We want to log whether or not seccomp BPF is actually supported
  // since actual test coverage depends on it.
  RecordProperty("SeccompBPFSupported",
                 seccomp_bpf_supported ? "true." : "false.");
  std::cout << "Seccomp BPF supported: "
            << (seccomp_bpf_supported ? "true." : "false.") << "\n";
  RecordProperty("PointerSize", sizeof(void*));
  std::cout << "Pointer size: " << sizeof(void*) << "\n";
}

SANDBOX_TEST(SandboxBPF, DISABLE_ON_TSAN(CallSupportsTwice)) {
  SandboxBPF::SupportsSeccompSandbox(-1);
  SandboxBPF::SupportsSeccompSandbox(-1);
}

// BPF_TEST does a lot of the boiler-plate code around setting up a
// policy and optional passing data between the caller, the policy and
// any Trap() handlers. This is great for writing short and concise tests,
// and it helps us accidentally forgetting any of the crucial steps in
// setting up the sandbox. But it wouldn't hurt to have at least one test
// that explicitly walks through all these steps.

intptr_t IncreaseCounter(const struct arch_seccomp_data& args, void* aux) {
  BPF_ASSERT(aux);
  int* counter = static_cast<int*>(aux);
  return (*counter)++;
}

class VerboseAPITestingPolicy : public SandboxBPFPolicy {
 public:
  VerboseAPITestingPolicy(int* counter_ptr) : counter_ptr_(counter_ptr) {}

  virtual ErrorCode EvaluateSyscall(SandboxBPF* sandbox,
                                    int sysno) const OVERRIDE {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
    if (sysno == __NR_uname) {
      return sandbox->Trap(IncreaseCounter, counter_ptr_);
    }
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  }

 private:
  int* counter_ptr_;
  DISALLOW_COPY_AND_ASSIGN(VerboseAPITestingPolicy);
};

SANDBOX_TEST(SandboxBPF, DISABLE_ON_TSAN(VerboseAPITesting)) {
  if (SandboxBPF::SupportsSeccompSandbox(-1) ==
      sandbox::SandboxBPF::STATUS_AVAILABLE) {
    static int counter = 0;

    SandboxBPF sandbox;
    sandbox.SetSandboxPolicy(new VerboseAPITestingPolicy(&counter));
    BPF_ASSERT(sandbox.StartSandbox(SandboxBPF::PROCESS_SINGLE_THREADED));

    BPF_ASSERT_EQ(0, counter);
    BPF_ASSERT_EQ(0, syscall(__NR_uname, 0));
    BPF_ASSERT_EQ(1, counter);
    BPF_ASSERT_EQ(1, syscall(__NR_uname, 0));
    BPF_ASSERT_EQ(2, counter);
  }
}

// A simple blacklist test

class BlacklistNanosleepPolicy : public SandboxBPFPolicy {
 public:
  BlacklistNanosleepPolicy() {}
  virtual ErrorCode EvaluateSyscall(SandboxBPF*, int sysno) const OVERRIDE {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
    switch (sysno) {
      case __NR_nanosleep:
        return ErrorCode(EACCES);
      default:
        return ErrorCode(ErrorCode::ERR_ALLOWED);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BlacklistNanosleepPolicy);
};

BPF_TEST_C(SandboxBPF, ApplyBasicBlacklistPolicy, BlacklistNanosleepPolicy) {
  // nanosleep() should be denied
  const struct timespec ts = {0, 0};
  errno = 0;
  BPF_ASSERT(syscall(__NR_nanosleep, &ts, NULL) == -1);
  BPF_ASSERT(errno == EACCES);
}

// Now do a simple whitelist test

class WhitelistGetpidPolicy : public SandboxBPFPolicy {
 public:
  WhitelistGetpidPolicy() {}
  virtual ErrorCode EvaluateSyscall(SandboxBPF*, int sysno) const OVERRIDE {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
    switch (sysno) {
      case __NR_getpid:
      case __NR_exit_group:
        return ErrorCode(ErrorCode::ERR_ALLOWED);
      default:
        return ErrorCode(ENOMEM);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WhitelistGetpidPolicy);
};

BPF_TEST_C(SandboxBPF, ApplyBasicWhitelistPolicy, WhitelistGetpidPolicy) {
  // getpid() should be allowed
  errno = 0;
  BPF_ASSERT(syscall(__NR_getpid) > 0);
  BPF_ASSERT(errno == 0);

  // getpgid() should be denied
  BPF_ASSERT(getpgid(0) == -1);
  BPF_ASSERT(errno == ENOMEM);
}

// A simple blacklist policy, with a SIGSYS handler
intptr_t EnomemHandler(const struct arch_seccomp_data& args, void* aux) {
  // We also check that the auxiliary data is correct
  SANDBOX_ASSERT(aux);
  *(static_cast<int*>(aux)) = kExpectedReturnValue;
  return -ENOMEM;
}

ErrorCode BlacklistNanosleepPolicySigsys(SandboxBPF* sandbox,
                                         int sysno,
                                         int* aux) {
  DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
  switch (sysno) {
    case __NR_nanosleep:
      return sandbox->Trap(EnomemHandler, aux);
    default:
      return ErrorCode(ErrorCode::ERR_ALLOWED);
  }
}

BPF_TEST(SandboxBPF,
         BasicBlacklistWithSigsys,
         BlacklistNanosleepPolicySigsys,
         int /* (*BPF_AUX) */) {
  // getpid() should work properly
  errno = 0;
  BPF_ASSERT(syscall(__NR_getpid) > 0);
  BPF_ASSERT(errno == 0);

  // Our Auxiliary Data, should be reset by the signal handler
  *BPF_AUX = -1;
  const struct timespec ts = {0, 0};
  BPF_ASSERT(syscall(__NR_nanosleep, &ts, NULL) == -1);
  BPF_ASSERT(errno == ENOMEM);

  // We expect the signal handler to modify AuxData
  BPF_ASSERT(*BPF_AUX == kExpectedReturnValue);
}

// A simple test that verifies we can return arbitrary errno values.

class ErrnoTestPolicy : public SandboxBPFPolicy {
 public:
  ErrnoTestPolicy() {}
  virtual ErrorCode EvaluateSyscall(SandboxBPF*, int sysno) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ErrnoTestPolicy);
};

ErrorCode ErrnoTestPolicy::EvaluateSyscall(SandboxBPF*, int sysno) const {
  DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
  switch (sysno) {
#if defined(ANDROID)
    case __NR_dup3:    // dup2 is a wrapper of dup3 in android
#else
    case __NR_dup2:
#endif
      // Pretend that dup2() worked, but don't actually do anything.
      return ErrorCode(0);
    case __NR_setuid:
#if defined(__NR_setuid32)
    case __NR_setuid32:
#endif
      // Return errno = 1.
      return ErrorCode(1);
    case __NR_setgid:
#if defined(__NR_setgid32)
    case __NR_setgid32:
#endif
      // Return maximum errno value (typically 4095).
      return ErrorCode(ErrorCode::ERR_MAX_ERRNO);
    case __NR_uname:
      // Return errno = 42;
      return ErrorCode(42);
    default:
      return ErrorCode(ErrorCode::ERR_ALLOWED);
  }
}

BPF_TEST_C(SandboxBPF, ErrnoTest, ErrnoTestPolicy) {
  // Verify that dup2() returns success, but doesn't actually run.
  int fds[4];
  BPF_ASSERT(pipe(fds) == 0);
  BPF_ASSERT(pipe(fds + 2) == 0);
  BPF_ASSERT(dup2(fds[2], fds[0]) == 0);
  char buf[1] = {};
  BPF_ASSERT(write(fds[1], "\x55", 1) == 1);
  BPF_ASSERT(write(fds[3], "\xAA", 1) == 1);
  BPF_ASSERT(read(fds[0], buf, 1) == 1);

  // If dup2() executed, we will read \xAA, but it dup2() has been turned
  // into a no-op by our policy, then we will read \x55.
  BPF_ASSERT(buf[0] == '\x55');

  // Verify that we can return the minimum and maximum errno values.
  errno = 0;
  BPF_ASSERT(setuid(0) == -1);
  BPF_ASSERT(errno == 1);

  // On Android, errno is only supported up to 255, otherwise errno
  // processing is skipped.
  // We work around this (crbug.com/181647).
  if (sandbox::IsAndroid() && setgid(0) != -1) {
    errno = 0;
    BPF_ASSERT(setgid(0) == -ErrorCode::ERR_MAX_ERRNO);
    BPF_ASSERT(errno == 0);
  } else {
    errno = 0;
    BPF_ASSERT(setgid(0) == -1);
    BPF_ASSERT(errno == ErrorCode::ERR_MAX_ERRNO);
  }

  // Finally, test an errno in between the minimum and maximum.
  errno = 0;
  struct utsname uts_buf;
  BPF_ASSERT(uname(&uts_buf) == -1);
  BPF_ASSERT(errno == 42);
}

// Testing the stacking of two sandboxes

class StackingPolicyPartOne : public SandboxBPFPolicy {
 public:
  StackingPolicyPartOne() {}
  virtual ErrorCode EvaluateSyscall(SandboxBPF* sandbox,
                                    int sysno) const OVERRIDE {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
    switch (sysno) {
      case __NR_getppid:
        return sandbox->Cond(0,
                             ErrorCode::TP_32BIT,
                             ErrorCode::OP_EQUAL,
                             0,
                             ErrorCode(ErrorCode::ERR_ALLOWED),
                             ErrorCode(EPERM));
      default:
        return ErrorCode(ErrorCode::ERR_ALLOWED);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StackingPolicyPartOne);
};

class StackingPolicyPartTwo : public SandboxBPFPolicy {
 public:
  StackingPolicyPartTwo() {}
  virtual ErrorCode EvaluateSyscall(SandboxBPF* sandbox,
                                    int sysno) const OVERRIDE {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
    switch (sysno) {
      case __NR_getppid:
        return sandbox->Cond(0,
                             ErrorCode::TP_32BIT,
                             ErrorCode::OP_EQUAL,
                             0,
                             ErrorCode(EINVAL),
                             ErrorCode(ErrorCode::ERR_ALLOWED));
      default:
        return ErrorCode(ErrorCode::ERR_ALLOWED);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StackingPolicyPartTwo);
};

BPF_TEST_C(SandboxBPF, StackingPolicy, StackingPolicyPartOne) {
  errno = 0;
  BPF_ASSERT(syscall(__NR_getppid, 0) > 0);
  BPF_ASSERT(errno == 0);

  BPF_ASSERT(syscall(__NR_getppid, 1) == -1);
  BPF_ASSERT(errno == EPERM);

  // Stack a second sandbox with its own policy. Verify that we can further
  // restrict filters, but we cannot relax existing filters.
  SandboxBPF sandbox;
  sandbox.SetSandboxPolicy(new StackingPolicyPartTwo());
  BPF_ASSERT(sandbox.StartSandbox(SandboxBPF::PROCESS_SINGLE_THREADED));

  errno = 0;
  BPF_ASSERT(syscall(__NR_getppid, 0) == -1);
  BPF_ASSERT(errno == EINVAL);

  BPF_ASSERT(syscall(__NR_getppid, 1) == -1);
  BPF_ASSERT(errno == EPERM);
}

// A more complex, but synthetic policy. This tests the correctness of the BPF
// program by iterating through all syscalls and checking for an errno that
// depends on the syscall number. Unlike the Verifier, this exercises the BPF
// interpreter in the kernel.

// We try to make sure we exercise optimizations in the BPF compiler. We make
// sure that the compiler can have an opportunity to coalesce syscalls with
// contiguous numbers and we also make sure that disjoint sets can return the
// same errno.
int SysnoToRandomErrno(int sysno) {
  // Small contiguous sets of 3 system calls return an errno equal to the
  // index of that set + 1 (so that we never return a NUL errno).
  return ((sysno & ~3) >> 2) % 29 + 1;
}

class SyntheticPolicy : public SandboxBPFPolicy {
 public:
  SyntheticPolicy() {}
  virtual ErrorCode EvaluateSyscall(SandboxBPF*, int sysno) const OVERRIDE {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
    if (sysno == __NR_exit_group || sysno == __NR_write) {
      // exit_group() is special, we really need it to work.
      // write() is needed for BPF_ASSERT() to report a useful error message.
      return ErrorCode(ErrorCode::ERR_ALLOWED);
    }
    return ErrorCode(SysnoToRandomErrno(sysno));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SyntheticPolicy);
};

BPF_TEST_C(SandboxBPF, SyntheticPolicy, SyntheticPolicy) {
  // Ensure that that kExpectedReturnValue + syscallnumber + 1 does not int
  // overflow.
  BPF_ASSERT(std::numeric_limits<int>::max() - kExpectedReturnValue - 1 >=
             static_cast<int>(MAX_PUBLIC_SYSCALL));

  for (int syscall_number = static_cast<int>(MIN_SYSCALL);
       syscall_number <= static_cast<int>(MAX_PUBLIC_SYSCALL);
       ++syscall_number) {
    if (syscall_number == __NR_exit_group || syscall_number == __NR_write) {
      // exit_group() is special
      continue;
    }
    errno = 0;
    BPF_ASSERT(syscall(syscall_number) == -1);
    BPF_ASSERT(errno == SysnoToRandomErrno(syscall_number));
  }
}

#if defined(__arm__)
// A simple policy that tests whether ARM private system calls are supported
// by our BPF compiler and by the BPF interpreter in the kernel.

// For ARM private system calls, return an errno equal to their offset from
// MIN_PRIVATE_SYSCALL plus 1 (to avoid NUL errno).
int ArmPrivateSysnoToErrno(int sysno) {
  if (sysno >= static_cast<int>(MIN_PRIVATE_SYSCALL) &&
      sysno <= static_cast<int>(MAX_PRIVATE_SYSCALL)) {
    return (sysno - MIN_PRIVATE_SYSCALL) + 1;
  } else {
    return ENOSYS;
  }
}

class ArmPrivatePolicy : public SandboxBPFPolicy {
 public:
  ArmPrivatePolicy() {}
  virtual ErrorCode EvaluateSyscall(SandboxBPF*, int sysno) const OVERRIDE {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
    // Start from |__ARM_NR_set_tls + 1| so as not to mess with actual
    // ARM private system calls.
    if (sysno >= static_cast<int>(__ARM_NR_set_tls + 1) &&
        sysno <= static_cast<int>(MAX_PRIVATE_SYSCALL)) {
      return ErrorCode(ArmPrivateSysnoToErrno(sysno));
    }
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArmPrivatePolicy);
};

BPF_TEST_C(SandboxBPF, ArmPrivatePolicy, ArmPrivatePolicy) {
  for (int syscall_number = static_cast<int>(__ARM_NR_set_tls + 1);
       syscall_number <= static_cast<int>(MAX_PRIVATE_SYSCALL);
       ++syscall_number) {
    errno = 0;
    BPF_ASSERT(syscall(syscall_number) == -1);
    BPF_ASSERT(errno == ArmPrivateSysnoToErrno(syscall_number));
  }
}
#endif  // defined(__arm__)

intptr_t CountSyscalls(const struct arch_seccomp_data& args, void* aux) {
  // Count all invocations of our callback function.
  ++*reinterpret_cast<int*>(aux);

  // Verify that within the callback function all filtering is temporarily
  // disabled.
  BPF_ASSERT(syscall(__NR_getpid) > 1);

  // Verify that we can now call the underlying system call without causing
  // infinite recursion.
  return SandboxBPF::ForwardSyscall(args);
}

ErrorCode GreyListedPolicy(SandboxBPF* sandbox, int sysno, int* aux) {
  // Set the global environment for unsafe traps once.
  if (sysno == MIN_SYSCALL) {
    EnableUnsafeTraps();
  }

  // Some system calls must always be allowed, if our policy wants to make
  // use of UnsafeTrap()
  if (sysno == __NR_rt_sigprocmask || sysno == __NR_rt_sigreturn
#if defined(__NR_sigprocmask)
      ||
      sysno == __NR_sigprocmask
#endif
#if defined(__NR_sigreturn)
      ||
      sysno == __NR_sigreturn
#endif
      ) {
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  } else if (sysno == __NR_getpid) {
    // Disallow getpid()
    return ErrorCode(EPERM);
  } else if (SandboxBPF::IsValidSyscallNumber(sysno)) {
    // Allow (and count) all other system calls.
    return sandbox->UnsafeTrap(CountSyscalls, aux);
  } else {
    return ErrorCode(ENOSYS);
  }
}

BPF_TEST(SandboxBPF, GreyListedPolicy, GreyListedPolicy, int /* (*BPF_AUX) */) {
  BPF_ASSERT(syscall(__NR_getpid) == -1);
  BPF_ASSERT(errno == EPERM);
  BPF_ASSERT(*BPF_AUX == 0);
  BPF_ASSERT(syscall(__NR_geteuid) == syscall(__NR_getuid));
  BPF_ASSERT(*BPF_AUX == 2);
  char name[17] = {};
  BPF_ASSERT(!syscall(__NR_prctl,
                      PR_GET_NAME,
                      name,
                      (void*)NULL,
                      (void*)NULL,
                      (void*)NULL));
  BPF_ASSERT(*BPF_AUX == 3);
  BPF_ASSERT(*name);
}

SANDBOX_TEST(SandboxBPF, EnableUnsafeTrapsInSigSysHandler) {
  // Disabling warning messages that could confuse our test framework.
  setenv(kSandboxDebuggingEnv, "t", 0);
  Die::SuppressInfoMessages(true);

  unsetenv(kSandboxDebuggingEnv);
  SANDBOX_ASSERT(Trap::EnableUnsafeTrapsInSigSysHandler() == false);
  setenv(kSandboxDebuggingEnv, "", 1);
  SANDBOX_ASSERT(Trap::EnableUnsafeTrapsInSigSysHandler() == false);
  setenv(kSandboxDebuggingEnv, "t", 1);
  SANDBOX_ASSERT(Trap::EnableUnsafeTrapsInSigSysHandler() == true);
}

intptr_t PrctlHandler(const struct arch_seccomp_data& args, void*) {
  if (args.args[0] == PR_CAPBSET_DROP && static_cast<int>(args.args[1]) == -1) {
    // prctl(PR_CAPBSET_DROP, -1) is never valid. The kernel will always
    // return an error. But our handler allows this call.
    return 0;
  } else {
    return SandboxBPF::ForwardSyscall(args);
  }
}

class PrctlPolicy : public SandboxBPFPolicy {
 public:
  PrctlPolicy() {}
  virtual ErrorCode EvaluateSyscall(SandboxBPF* sandbox,
                                    int sysno) const OVERRIDE {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
    setenv(kSandboxDebuggingEnv, "t", 0);
    Die::SuppressInfoMessages(true);

    if (sysno == __NR_prctl) {
      // Handle prctl() inside an UnsafeTrap()
      return sandbox->UnsafeTrap(PrctlHandler, NULL);
    }

    // Allow all other system calls.
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PrctlPolicy);
};

BPF_TEST_C(SandboxBPF, ForwardSyscall, PrctlPolicy) {
  // This call should never be allowed. But our policy will intercept it and
  // let it pass successfully.
  BPF_ASSERT(
      !prctl(PR_CAPBSET_DROP, -1, (void*)NULL, (void*)NULL, (void*)NULL));

  // Verify that the call will fail, if it makes it all the way to the kernel.
  BPF_ASSERT(
      prctl(PR_CAPBSET_DROP, -2, (void*)NULL, (void*)NULL, (void*)NULL) == -1);

  // And verify that other uses of prctl() work just fine.
  char name[17] = {};
  BPF_ASSERT(!syscall(__NR_prctl,
                      PR_GET_NAME,
                      name,
                      (void*)NULL,
                      (void*)NULL,
                      (void*)NULL));
  BPF_ASSERT(*name);

  // Finally, verify that system calls other than prctl() are completely
  // unaffected by our policy.
  struct utsname uts = {};
  BPF_ASSERT(!uname(&uts));
  BPF_ASSERT(!strcmp(uts.sysname, "Linux"));
}

intptr_t AllowRedirectedSyscall(const struct arch_seccomp_data& args, void*) {
  return SandboxBPF::ForwardSyscall(args);
}

class RedirectAllSyscallsPolicy : public SandboxBPFPolicy {
 public:
  RedirectAllSyscallsPolicy() {}
  virtual ErrorCode EvaluateSyscall(SandboxBPF* sandbox,
                                    int sysno) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(RedirectAllSyscallsPolicy);
};

ErrorCode RedirectAllSyscallsPolicy::EvaluateSyscall(SandboxBPF* sandbox,
                                                     int sysno) const {
  DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
  setenv(kSandboxDebuggingEnv, "t", 0);
  Die::SuppressInfoMessages(true);

  // Some system calls must always be allowed, if our policy wants to make
  // use of UnsafeTrap()
  if (sysno == __NR_rt_sigprocmask || sysno == __NR_rt_sigreturn
#if defined(__NR_sigprocmask)
      ||
      sysno == __NR_sigprocmask
#endif
#if defined(__NR_sigreturn)
      ||
      sysno == __NR_sigreturn
#endif
      ) {
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  }
  return sandbox->UnsafeTrap(AllowRedirectedSyscall, NULL);
}

int bus_handler_fd_ = -1;

void SigBusHandler(int, siginfo_t* info, void* void_context) {
  BPF_ASSERT(write(bus_handler_fd_, "\x55", 1) == 1);
}

BPF_TEST_C(SandboxBPF, SigBus, RedirectAllSyscallsPolicy) {
  // We use the SIGBUS bit in the signal mask as a thread-local boolean
  // value in the implementation of UnsafeTrap(). This is obviously a bit
  // of a hack that could conceivably interfere with code that uses SIGBUS
  // in more traditional ways. This test verifies that basic functionality
  // of SIGBUS is not impacted, but it is certainly possibly to construe
  // more complex uses of signals where our use of the SIGBUS mask is not
  // 100% transparent. This is expected behavior.
  int fds[2];
  BPF_ASSERT(pipe(fds) == 0);
  bus_handler_fd_ = fds[1];
  struct sigaction sa = {};
  sa.sa_sigaction = SigBusHandler;
  sa.sa_flags = SA_SIGINFO;
  BPF_ASSERT(sigaction(SIGBUS, &sa, NULL) == 0);
  raise(SIGBUS);
  char c = '\000';
  BPF_ASSERT(read(fds[0], &c, 1) == 1);
  BPF_ASSERT(close(fds[0]) == 0);
  BPF_ASSERT(close(fds[1]) == 0);
  BPF_ASSERT(c == 0x55);
}

BPF_TEST_C(SandboxBPF, SigMask, RedirectAllSyscallsPolicy) {
  // Signal masks are potentially tricky to handle. For instance, if we
  // ever tried to update them from inside a Trap() or UnsafeTrap() handler,
  // the call to sigreturn() at the end of the signal handler would undo
  // all of our efforts. So, it makes sense to test that sigprocmask()
  // works, even if we have a policy in place that makes use of UnsafeTrap().
  // In practice, this works because we force sigprocmask() to be handled
  // entirely in the kernel.
  sigset_t mask0, mask1, mask2;

  // Call sigprocmask() to verify that SIGUSR2 wasn't blocked, if we didn't
  // change the mask (it shouldn't have been, as it isn't blocked by default
  // in POSIX).
  //
  // Use SIGUSR2 because Android seems to use SIGUSR1 for some purpose.
  sigemptyset(&mask0);
  BPF_ASSERT(!sigprocmask(SIG_BLOCK, &mask0, &mask1));
  BPF_ASSERT(!sigismember(&mask1, SIGUSR2));

  // Try again, and this time we verify that we can block it. This
  // requires a second call to sigprocmask().
  sigaddset(&mask0, SIGUSR2);
  BPF_ASSERT(!sigprocmask(SIG_BLOCK, &mask0, NULL));
  BPF_ASSERT(!sigprocmask(SIG_BLOCK, NULL, &mask2));
  BPF_ASSERT(sigismember(&mask2, SIGUSR2));
}

BPF_TEST_C(SandboxBPF, UnsafeTrapWithErrno, RedirectAllSyscallsPolicy) {
  // An UnsafeTrap() (or for that matter, a Trap()) has to report error
  // conditions by returning an exit code in the range -1..-4096. This
  // should happen automatically if using ForwardSyscall(). If the TrapFnc()
  // uses some other method to make system calls, then it is responsible
  // for computing the correct return code.
  // This test verifies that ForwardSyscall() does the correct thing.

  // The glibc system wrapper will ultimately set errno for us. So, from normal
  // userspace, all of this should be completely transparent.
  errno = 0;
  BPF_ASSERT(close(-1) == -1);
  BPF_ASSERT(errno == EBADF);

  // Explicitly avoid the glibc wrapper. This is not normally the way anybody
  // would make system calls, but it allows us to verify that we don't
  // accidentally mess with errno, when we shouldn't.
  errno = 0;
  struct arch_seccomp_data args = {};
  args.nr = __NR_close;
  args.args[0] = -1;
  BPF_ASSERT(SandboxBPF::ForwardSyscall(args) == -EBADF);
  BPF_ASSERT(errno == 0);
}

bool NoOpCallback() { return true; }

// Test a trap handler that makes use of a broker process to open().

class InitializedOpenBroker {
 public:
  InitializedOpenBroker() : initialized_(false) {
    std::vector<std::string> allowed_files;
    allowed_files.push_back("/proc/allowed");
    allowed_files.push_back("/proc/cpuinfo");

    broker_process_.reset(
        new BrokerProcess(EPERM, allowed_files, std::vector<std::string>()));
    BPF_ASSERT(broker_process() != NULL);
    BPF_ASSERT(broker_process_->Init(base::Bind(&NoOpCallback)));

    initialized_ = true;
  }
  bool initialized() { return initialized_; }
  class BrokerProcess* broker_process() { return broker_process_.get(); }

 private:
  bool initialized_;
  scoped_ptr<class BrokerProcess> broker_process_;
  DISALLOW_COPY_AND_ASSIGN(InitializedOpenBroker);
};

intptr_t BrokerOpenTrapHandler(const struct arch_seccomp_data& args,
                               void* aux) {
  BPF_ASSERT(aux);
  BrokerProcess* broker_process = static_cast<BrokerProcess*>(aux);
  switch (args.nr) {
#if defined(ANDROID)
    case __NR_faccessat:    // access is a wrapper of faccessat in android
      return broker_process->Access(reinterpret_cast<const char*>(args.args[1]),
                                    static_cast<int>(args.args[2]));
#else
    case __NR_access:
      return broker_process->Access(reinterpret_cast<const char*>(args.args[0]),
                                    static_cast<int>(args.args[1]));
#endif
    case __NR_open:
      return broker_process->Open(reinterpret_cast<const char*>(args.args[0]),
                                  static_cast<int>(args.args[1]));
    case __NR_openat:
      // We only call open() so if we arrive here, it's because glibc uses
      // the openat() system call.
      BPF_ASSERT(static_cast<int>(args.args[0]) == AT_FDCWD);
      return broker_process->Open(reinterpret_cast<const char*>(args.args[1]),
                                  static_cast<int>(args.args[2]));
    default:
      BPF_ASSERT(false);
      return -ENOSYS;
  }
}

ErrorCode DenyOpenPolicy(SandboxBPF* sandbox,
                         int sysno,
                         InitializedOpenBroker* iob) {
  if (!SandboxBPF::IsValidSyscallNumber(sysno)) {
    return ErrorCode(ENOSYS);
  }

  switch (sysno) {
#if defined(ANDROID)
    case __NR_faccessat:
#else
    case __NR_access:
#endif
    case __NR_open:
    case __NR_openat:
      // We get a InitializedOpenBroker class, but our trap handler wants
      // the BrokerProcess object.
      return ErrorCode(
          sandbox->Trap(BrokerOpenTrapHandler, iob->broker_process()));
    default:
      return ErrorCode(ErrorCode::ERR_ALLOWED);
  }
}

// We use a InitializedOpenBroker class, so that we can run unsandboxed
// code in its constructor, which is the only way to do so in a BPF_TEST.
BPF_TEST(SandboxBPF,
         UseOpenBroker,
         DenyOpenPolicy,
         InitializedOpenBroker /* (*BPF_AUX) */) {
  BPF_ASSERT(BPF_AUX->initialized());
  BrokerProcess* broker_process = BPF_AUX->broker_process();
  BPF_ASSERT(broker_process != NULL);

  // First, use the broker "manually"
  BPF_ASSERT(broker_process->Open("/proc/denied", O_RDONLY) == -EPERM);
  BPF_ASSERT(broker_process->Access("/proc/denied", R_OK) == -EPERM);
  BPF_ASSERT(broker_process->Open("/proc/allowed", O_RDONLY) == -ENOENT);
  BPF_ASSERT(broker_process->Access("/proc/allowed", R_OK) == -ENOENT);

  // Now use glibc's open() as an external library would.
  BPF_ASSERT(open("/proc/denied", O_RDONLY) == -1);
  BPF_ASSERT(errno == EPERM);

  BPF_ASSERT(open("/proc/allowed", O_RDONLY) == -1);
  BPF_ASSERT(errno == ENOENT);

  // Also test glibc's openat(), some versions of libc use it transparently
  // instead of open().
  BPF_ASSERT(openat(AT_FDCWD, "/proc/denied", O_RDONLY) == -1);
  BPF_ASSERT(errno == EPERM);

  BPF_ASSERT(openat(AT_FDCWD, "/proc/allowed", O_RDONLY) == -1);
  BPF_ASSERT(errno == ENOENT);

  // And test glibc's access().
  BPF_ASSERT(access("/proc/denied", R_OK) == -1);
  BPF_ASSERT(errno == EPERM);

  BPF_ASSERT(access("/proc/allowed", R_OK) == -1);
  BPF_ASSERT(errno == ENOENT);

  // This is also white listed and does exist.
  int cpu_info_access = access("/proc/cpuinfo", R_OK);
  BPF_ASSERT(cpu_info_access == 0);
  int cpu_info_fd = open("/proc/cpuinfo", O_RDONLY);
  BPF_ASSERT(cpu_info_fd >= 0);
  char buf[1024];
  BPF_ASSERT(read(cpu_info_fd, buf, sizeof(buf)) > 0);
}

// Simple test demonstrating how to use SandboxBPF::Cond()

class SimpleCondTestPolicy : public SandboxBPFPolicy {
 public:
  SimpleCondTestPolicy() {}
  virtual ErrorCode EvaluateSyscall(SandboxBPF* sandbox,
                                    int sysno) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SimpleCondTestPolicy);
};

ErrorCode SimpleCondTestPolicy::EvaluateSyscall(SandboxBPF* sandbox,
                                                int sysno) const {
  DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));

  // We deliberately return unusual errno values upon failure, so that we
  // can uniquely test for these values. In a "real" policy, you would want
  // to return more traditional values.
  switch (sysno) {
#if defined(ANDROID)
    case __NR_openat:    // open is a wrapper of openat in android
      // Allow opening files for reading, but don't allow writing.
      COMPILE_ASSERT(O_RDONLY == 0, O_RDONLY_must_be_all_zero_bits);
      return sandbox->Cond(2,
                           ErrorCode::TP_32BIT,
                           ErrorCode::OP_HAS_ANY_BITS,
                           O_ACCMODE /* 0x3 */,
                           ErrorCode(EROFS),
                           ErrorCode(ErrorCode::ERR_ALLOWED));
#else
    case __NR_open:
      // Allow opening files for reading, but don't allow writing.
      COMPILE_ASSERT(O_RDONLY == 0, O_RDONLY_must_be_all_zero_bits);
      return sandbox->Cond(1,
                           ErrorCode::TP_32BIT,
                           ErrorCode::OP_HAS_ANY_BITS,
                           O_ACCMODE /* 0x3 */,
                           ErrorCode(EROFS),
                           ErrorCode(ErrorCode::ERR_ALLOWED));
#endif
    case __NR_prctl:
      // Allow prctl(PR_SET_DUMPABLE) and prctl(PR_GET_DUMPABLE), but
      // disallow everything else.
      return sandbox->Cond(0,
                           ErrorCode::TP_32BIT,
                           ErrorCode::OP_EQUAL,
                           PR_SET_DUMPABLE,
                           ErrorCode(ErrorCode::ERR_ALLOWED),
                           sandbox->Cond(0,
                                         ErrorCode::TP_32BIT,
                                         ErrorCode::OP_EQUAL,
                                         PR_GET_DUMPABLE,
                                         ErrorCode(ErrorCode::ERR_ALLOWED),
                                         ErrorCode(ENOMEM)));
    default:
      return ErrorCode(ErrorCode::ERR_ALLOWED);
  }
}

BPF_TEST_C(SandboxBPF, SimpleCondTest, SimpleCondTestPolicy) {
  int fd;
  BPF_ASSERT((fd = open("/proc/self/comm", O_RDWR)) == -1);
  BPF_ASSERT(errno == EROFS);
  BPF_ASSERT((fd = open("/proc/self/comm", O_RDONLY)) >= 0);
  close(fd);

  int ret;
  BPF_ASSERT((ret = prctl(PR_GET_DUMPABLE)) >= 0);
  BPF_ASSERT(prctl(PR_SET_DUMPABLE, 1 - ret) == 0);
  BPF_ASSERT(prctl(PR_GET_ENDIAN, &ret) == -1);
  BPF_ASSERT(errno == ENOMEM);
}

// This test exercises the SandboxBPF::Cond() method by building a complex
// tree of conditional equality operations. It then makes system calls and
// verifies that they return the values that we expected from our BPF
// program.
class EqualityStressTest {
 public:
  EqualityStressTest() {
    // We want a deterministic test
    srand(0);

    // Iterates over system call numbers and builds a random tree of
    // equality tests.
    // We are actually constructing a graph of ArgValue objects. This
    // graph will later be used to a) compute our sandbox policy, and
    // b) drive the code that verifies the output from the BPF program.
    COMPILE_ASSERT(
        kNumTestCases < (int)(MAX_PUBLIC_SYSCALL - MIN_SYSCALL - 10),
        num_test_cases_must_be_significantly_smaller_than_num_system_calls);
    for (int sysno = MIN_SYSCALL, end = kNumTestCases; sysno < end; ++sysno) {
      if (IsReservedSyscall(sysno)) {
        // Skip reserved system calls. This ensures that our test frame
        // work isn't impacted by the fact that we are overriding
        // a lot of different system calls.
        ++end;
        arg_values_.push_back(NULL);
      } else {
        arg_values_.push_back(
            RandomArgValue(rand() % kMaxArgs, 0, rand() % kMaxArgs));
      }
    }
  }

  ~EqualityStressTest() {
    for (std::vector<ArgValue*>::iterator iter = arg_values_.begin();
         iter != arg_values_.end();
         ++iter) {
      DeleteArgValue(*iter);
    }
  }

  ErrorCode Policy(SandboxBPF* sandbox, int sysno) {
    if (!SandboxBPF::IsValidSyscallNumber(sysno)) {
      // FIXME: we should really not have to do that in a trivial policy
      return ErrorCode(ENOSYS);
    } else if (sysno < 0 || sysno >= (int)arg_values_.size() ||
               IsReservedSyscall(sysno)) {
      // We only return ErrorCode values for the system calls that
      // are part of our test data. Every other system call remains
      // allowed.
      return ErrorCode(ErrorCode::ERR_ALLOWED);
    } else {
      // ToErrorCode() turns an ArgValue object into an ErrorCode that is
      // suitable for use by a sandbox policy.
      return ToErrorCode(sandbox, arg_values_[sysno]);
    }
  }

  void VerifyFilter() {
    // Iterate over all system calls. Skip the system calls that have
    // previously been determined as being reserved.
    for (int sysno = 0; sysno < (int)arg_values_.size(); ++sysno) {
      if (!arg_values_[sysno]) {
        // Skip reserved system calls.
        continue;
      }
      // Verify that system calls return the values that we expect them to
      // return. This involves passing different combinations of system call
      // parameters in order to exercise all possible code paths through the
      // BPF filter program.
      // We arbitrarily start by setting all six system call arguments to
      // zero. And we then recursive traverse our tree of ArgValues to
      // determine the necessary combinations of parameters.
      intptr_t args[6] = {};
      Verify(sysno, args, *arg_values_[sysno]);
    }
  }

 private:
  struct ArgValue {
    int argno;  // Argument number to inspect.
    int size;   // Number of test cases (must be > 0).
    struct Tests {
      uint32_t k_value;            // Value to compare syscall arg against.
      int err;                     // If non-zero, errno value to return.
      struct ArgValue* arg_value;  // Otherwise, more args needs inspecting.
    }* tests;
    int err;                     // If none of the tests passed, this is what
    struct ArgValue* arg_value;  // we'll return (this is the "else" branch).
  };

  bool IsReservedSyscall(int sysno) {
    // There are a handful of system calls that we should never use in our
    // test cases. These system calls are needed to allow the test framework
    // to run properly.
    // If we wanted to write fully generic code, there are more system calls
    // that could be listed here, and it is quite difficult to come up with a
    // truly comprehensive list. After all, we are deliberately making system
    // calls unavailable. In practice, we have a pretty good idea of the system
    // calls that will be made by this particular test. So, this small list is
    // sufficient. But if anybody copy'n'pasted this code for other uses, they
    // would have to review that the list.
    return sysno == __NR_read || sysno == __NR_write || sysno == __NR_exit ||
           sysno == __NR_exit_group || sysno == __NR_restart_syscall;
  }

  ArgValue* RandomArgValue(int argno, int args_mask, int remaining_args) {
    // Create a new ArgValue and fill it with random data. We use as bit mask
    // to keep track of the system call parameters that have previously been
    // set; this ensures that we won't accidentally define a contradictory
    // set of equality tests.
    struct ArgValue* arg_value = new ArgValue();
    args_mask |= 1 << argno;
    arg_value->argno = argno;

    // Apply some restrictions on just how complex our tests can be.
    // Otherwise, we end up with a BPF program that is too complicated for
    // the kernel to load.
    int fan_out = kMaxFanOut;
    if (remaining_args > 3) {
      fan_out = 1;
    } else if (remaining_args > 2) {
      fan_out = 2;
    }

    // Create a couple of different test cases with randomized values that
    // we want to use when comparing system call parameter number "argno".
    arg_value->size = rand() % fan_out + 1;
    arg_value->tests = new ArgValue::Tests[arg_value->size];

    uint32_t k_value = rand();
    for (int n = 0; n < arg_value->size; ++n) {
      // Ensure that we have unique values
      k_value += rand() % (RAND_MAX / (kMaxFanOut + 1)) + 1;

      // There are two possible types of nodes. Either this is a leaf node;
      // in that case, we have completed all the equality tests that we
      // wanted to perform, and we can now compute a random "errno" value that
      // we should return. Or this is part of a more complex boolean
      // expression; in that case, we have to recursively add tests for some
      // of system call parameters that we have not yet included in our
      // tests.
      arg_value->tests[n].k_value = k_value;
      if (!remaining_args || (rand() & 1)) {
        arg_value->tests[n].err = (rand() % 1000) + 1;
        arg_value->tests[n].arg_value = NULL;
      } else {
        arg_value->tests[n].err = 0;
        arg_value->tests[n].arg_value =
            RandomArgValue(RandomArg(args_mask), args_mask, remaining_args - 1);
      }
    }
    // Finally, we have to define what we should return if none of the
    // previous equality tests pass. Again, we can either deal with a leaf
    // node, or we can randomly add another couple of tests.
    if (!remaining_args || (rand() & 1)) {
      arg_value->err = (rand() % 1000) + 1;
      arg_value->arg_value = NULL;
    } else {
      arg_value->err = 0;
      arg_value->arg_value =
          RandomArgValue(RandomArg(args_mask), args_mask, remaining_args - 1);
    }
    // We have now built a new (sub-)tree of ArgValues defining a set of
    // boolean expressions for testing random system call arguments against
    // random values. Return this tree to our caller.
    return arg_value;
  }

  int RandomArg(int args_mask) {
    // Compute a random system call parameter number.
    int argno = rand() % kMaxArgs;

    // Make sure that this same parameter number has not previously been
    // used. Otherwise, we could end up with a test that is impossible to
    // satisfy (e.g. args[0] == 1 && args[0] == 2).
    while (args_mask & (1 << argno)) {
      argno = (argno + 1) % kMaxArgs;
    }
    return argno;
  }

  void DeleteArgValue(ArgValue* arg_value) {
    // Delete an ArgValue and all of its child nodes. This requires
    // recursively descending into the tree.
    if (arg_value) {
      if (arg_value->size) {
        for (int n = 0; n < arg_value->size; ++n) {
          if (!arg_value->tests[n].err) {
            DeleteArgValue(arg_value->tests[n].arg_value);
          }
        }
        delete[] arg_value->tests;
      }
      if (!arg_value->err) {
        DeleteArgValue(arg_value->arg_value);
      }
      delete arg_value;
    }
  }

  ErrorCode ToErrorCode(SandboxBPF* sandbox, ArgValue* arg_value) {
    // Compute the ErrorCode that should be returned, if none of our
    // tests succeed (i.e. the system call parameter doesn't match any
    // of the values in arg_value->tests[].k_value).
    ErrorCode err;
    if (arg_value->err) {
      // If this was a leaf node, return the errno value that we expect to
      // return from the BPF filter program.
      err = ErrorCode(arg_value->err);
    } else {
      // If this wasn't a leaf node yet, recursively descend into the rest
      // of the tree. This will end up adding a few more SandboxBPF::Cond()
      // tests to our ErrorCode.
      err = ToErrorCode(sandbox, arg_value->arg_value);
    }

    // Now, iterate over all the test cases that we want to compare against.
    // This builds a chain of SandboxBPF::Cond() tests
    // (aka "if ... elif ... elif ... elif ... fi")
    for (int n = arg_value->size; n-- > 0;) {
      ErrorCode matched;
      // Again, we distinguish between leaf nodes and subtrees.
      if (arg_value->tests[n].err) {
        matched = ErrorCode(arg_value->tests[n].err);
      } else {
        matched = ToErrorCode(sandbox, arg_value->tests[n].arg_value);
      }
      // For now, all of our tests are limited to 32bit.
      // We have separate tests that check the behavior of 32bit vs. 64bit
      // conditional expressions.
      err = sandbox->Cond(arg_value->argno,
                          ErrorCode::TP_32BIT,
                          ErrorCode::OP_EQUAL,
                          arg_value->tests[n].k_value,
                          matched,
                          err);
    }
    return err;
  }

  void Verify(int sysno, intptr_t* args, const ArgValue& arg_value) {
    uint32_t mismatched = 0;
    // Iterate over all the k_values in arg_value.tests[] and verify that
    // we see the expected return values from system calls, when we pass
    // the k_value as a parameter in a system call.
    for (int n = arg_value.size; n-- > 0;) {
      mismatched += arg_value.tests[n].k_value;
      args[arg_value.argno] = arg_value.tests[n].k_value;
      if (arg_value.tests[n].err) {
        VerifyErrno(sysno, args, arg_value.tests[n].err);
      } else {
        Verify(sysno, args, *arg_value.tests[n].arg_value);
      }
    }
  // Find a k_value that doesn't match any of the k_values in
  // arg_value.tests[]. In most cases, the current value of "mismatched"
  // would fit this requirement. But on the off-chance that it happens
  // to collide, we double-check.
  try_again:
    for (int n = arg_value.size; n-- > 0;) {
      if (mismatched == arg_value.tests[n].k_value) {
        ++mismatched;
        goto try_again;
      }
    }
    // Now verify that we see the expected return value from system calls,
    // if we pass a value that doesn't match any of the conditions (i.e. this
    // is testing the "else" clause of the conditions).
    args[arg_value.argno] = mismatched;
    if (arg_value.err) {
      VerifyErrno(sysno, args, arg_value.err);
    } else {
      Verify(sysno, args, *arg_value.arg_value);
    }
    // Reset args[arg_value.argno]. This is not technically needed, but it
    // makes it easier to reason about the correctness of our tests.
    args[arg_value.argno] = 0;
  }

  void VerifyErrno(int sysno, intptr_t* args, int err) {
    // We installed BPF filters that return different errno values
    // based on the system call number and the parameters that we decided
    // to pass in. Verify that this condition holds true.
    BPF_ASSERT(
        Syscall::Call(
            sysno, args[0], args[1], args[2], args[3], args[4], args[5]) ==
        -err);
  }

  // Vector of ArgValue trees. These trees define all the possible boolean
  // expressions that we want to turn into a BPF filter program.
  std::vector<ArgValue*> arg_values_;

  // Don't increase these values. We are pushing the limits of the maximum
  // BPF program that the kernel will allow us to load. If the values are
  // increased too much, the test will start failing.
  static const int kNumTestCases = 40;
  static const int kMaxFanOut = 3;
  static const int kMaxArgs = 6;
};

ErrorCode EqualityStressTestPolicy(SandboxBPF* sandbox,
                                   int sysno,
                                   EqualityStressTest* aux) {
  DCHECK(aux);
  return aux->Policy(sandbox, sysno);
}

BPF_TEST(SandboxBPF,
         EqualityTests,
         EqualityStressTestPolicy,
         EqualityStressTest /* (*BPF_AUX) */) {
  BPF_AUX->VerifyFilter();
}

class EqualityArgumentWidthPolicy : public SandboxBPFPolicy {
 public:
  EqualityArgumentWidthPolicy() {}
  virtual ErrorCode EvaluateSyscall(SandboxBPF* sandbox,
                                    int sysno) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(EqualityArgumentWidthPolicy);
};

ErrorCode EqualityArgumentWidthPolicy::EvaluateSyscall(SandboxBPF* sandbox,
                                                       int sysno) const {
  DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
  if (sysno == __NR_uname) {
    return sandbox->Cond(
        0,
        ErrorCode::TP_32BIT,
        ErrorCode::OP_EQUAL,
        0,
        sandbox->Cond(1,
                      ErrorCode::TP_32BIT,
                      ErrorCode::OP_EQUAL,
                      0x55555555,
                      ErrorCode(1),
                      ErrorCode(2)),
        // The BPF compiler and the BPF interpreter in the kernel are
        // (mostly) agnostic of the host platform's word size. The compiler
        // will happily generate code that tests a 64bit value, and the
        // interpreter will happily perform this test.
        // But unless there is a kernel bug, there is no way for us to pass
        // in a 64bit quantity on a 32bit platform. The upper 32bits should
        // always be zero. So, this test should always evaluate as false on
        // 32bit systems.
        sandbox->Cond(1,
                      ErrorCode::TP_64BIT,
                      ErrorCode::OP_EQUAL,
                      0x55555555AAAAAAAAULL,
                      ErrorCode(1),
                      ErrorCode(2)));
  }
  return ErrorCode(ErrorCode::ERR_ALLOWED);
}

BPF_TEST_C(SandboxBPF, EqualityArgumentWidth, EqualityArgumentWidthPolicy) {
  BPF_ASSERT(Syscall::Call(__NR_uname, 0, 0x55555555) == -1);
  BPF_ASSERT(Syscall::Call(__NR_uname, 0, 0xAAAAAAAA) == -2);
#if __SIZEOF_POINTER__ > 4
  // On 32bit machines, there is no way to pass a 64bit argument through the
  // syscall interface. So, we have to skip the part of the test that requires
  // 64bit arguments.
  BPF_ASSERT(Syscall::Call(__NR_uname, 1, 0x55555555AAAAAAAAULL) == -1);
  BPF_ASSERT(Syscall::Call(__NR_uname, 1, 0x5555555500000000ULL) == -2);
  BPF_ASSERT(Syscall::Call(__NR_uname, 1, 0x5555555511111111ULL) == -2);
  BPF_ASSERT(Syscall::Call(__NR_uname, 1, 0x11111111AAAAAAAAULL) == -2);
#else
  BPF_ASSERT(Syscall::Call(__NR_uname, 1, 0x55555555) == -2);
#endif
}

#if __SIZEOF_POINTER__ > 4
// On 32bit machines, there is no way to pass a 64bit argument through the
// syscall interface. So, we have to skip the part of the test that requires
// 64bit arguments.
BPF_DEATH_TEST_C(SandboxBPF,
                 EqualityArgumentUnallowed64bit,
                 DEATH_MESSAGE("Unexpected 64bit argument detected"),
                 EqualityArgumentWidthPolicy) {
  Syscall::Call(__NR_uname, 0, 0x5555555555555555ULL);
}
#endif

class EqualityWithNegativeArgumentsPolicy : public SandboxBPFPolicy {
 public:
  EqualityWithNegativeArgumentsPolicy() {}
  virtual ErrorCode EvaluateSyscall(SandboxBPF* sandbox,
                                    int sysno) const OVERRIDE {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
    if (sysno == __NR_uname) {
      return sandbox->Cond(0,
                           ErrorCode::TP_32BIT,
                           ErrorCode::OP_EQUAL,
                           0xFFFFFFFF,
                           ErrorCode(1),
                           ErrorCode(2));
    }
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EqualityWithNegativeArgumentsPolicy);
};

BPF_TEST_C(SandboxBPF,
           EqualityWithNegativeArguments,
           EqualityWithNegativeArgumentsPolicy) {
  BPF_ASSERT(Syscall::Call(__NR_uname, 0xFFFFFFFF) == -1);
  BPF_ASSERT(Syscall::Call(__NR_uname, -1) == -1);
  BPF_ASSERT(Syscall::Call(__NR_uname, -1LL) == -1);
}

#if __SIZEOF_POINTER__ > 4
BPF_DEATH_TEST_C(SandboxBPF,
                 EqualityWithNegative64bitArguments,
                 DEATH_MESSAGE("Unexpected 64bit argument detected"),
                 EqualityWithNegativeArgumentsPolicy) {
  // When expecting a 32bit system call argument, we look at the MSB of the
  // 64bit value and allow both "0" and "-1". But the latter is allowed only
  // iff the LSB was negative. So, this death test should error out.
  BPF_ASSERT(Syscall::Call(__NR_uname, 0xFFFFFFFF00000000LL) == -1);
}
#endif
class AllBitTestPolicy : public SandboxBPFPolicy {
 public:
  AllBitTestPolicy() {}
  virtual ErrorCode EvaluateSyscall(SandboxBPF* sandbox,
                                    int sysno) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AllBitTestPolicy);
};

ErrorCode AllBitTestPolicy::EvaluateSyscall(SandboxBPF* sandbox,
                                            int sysno) const {
  DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
  // Test the OP_HAS_ALL_BITS conditional test operator with a couple of
  // different bitmasks. We try to find bitmasks that could conceivably
  // touch corner cases.
  // For all of these tests, we override the uname(). We can make use with
  // a single system call number, as we use the first system call argument to
  // select the different bit masks that we want to test against.
  if (sysno == __NR_uname) {
    return sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, 0,
           sandbox->Cond(1, ErrorCode::TP_32BIT, ErrorCode::OP_HAS_ALL_BITS,
                         0x0,
                         ErrorCode(1), ErrorCode(0)),

           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, 1,
           sandbox->Cond(1, ErrorCode::TP_32BIT, ErrorCode::OP_HAS_ALL_BITS,
                         0x1,
                         ErrorCode(1), ErrorCode(0)),

           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, 2,
           sandbox->Cond(1, ErrorCode::TP_32BIT, ErrorCode::OP_HAS_ALL_BITS,
                         0x3,
                         ErrorCode(1), ErrorCode(0)),

           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, 3,
           sandbox->Cond(1, ErrorCode::TP_32BIT, ErrorCode::OP_HAS_ALL_BITS,
                         0x80000000,
                         ErrorCode(1), ErrorCode(0)),
           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, 4,
           sandbox->Cond(1, ErrorCode::TP_64BIT, ErrorCode::OP_HAS_ALL_BITS,
                         0x0,
                         ErrorCode(1), ErrorCode(0)),

           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, 5,
           sandbox->Cond(1, ErrorCode::TP_64BIT, ErrorCode::OP_HAS_ALL_BITS,
                         0x1,
                         ErrorCode(1), ErrorCode(0)),

           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, 6,
           sandbox->Cond(1, ErrorCode::TP_64BIT, ErrorCode::OP_HAS_ALL_BITS,
                         0x3,
                         ErrorCode(1), ErrorCode(0)),

           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, 7,
           sandbox->Cond(1, ErrorCode::TP_64BIT, ErrorCode::OP_HAS_ALL_BITS,
                         0x80000000,
                         ErrorCode(1), ErrorCode(0)),

           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, 8,
           sandbox->Cond(1, ErrorCode::TP_64BIT, ErrorCode::OP_HAS_ALL_BITS,
                         0x100000000ULL,
                         ErrorCode(1), ErrorCode(0)),

           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, 9,
           sandbox->Cond(1, ErrorCode::TP_64BIT, ErrorCode::OP_HAS_ALL_BITS,
                         0x300000000ULL,
                         ErrorCode(1), ErrorCode(0)),

           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, 10,
           sandbox->Cond(1, ErrorCode::TP_64BIT, ErrorCode::OP_HAS_ALL_BITS,
                         0x100000001ULL,
                         ErrorCode(1), ErrorCode(0)),

                         sandbox->Kill("Invalid test case number"))))))))))));
  }
  return ErrorCode(ErrorCode::ERR_ALLOWED);
}

// Define a macro that performs tests using our test policy.
// NOTE: Not all of the arguments in this macro are actually used!
//       They are here just to serve as documentation of the conditions
//       implemented in the test policy.
//       Most notably, "op" and "mask" are unused by the macro. If you want
//       to make changes to these values, you will have to edit the
//       test policy instead.
#define BITMASK_TEST(testcase, arg, op, mask, expected_value) \
  BPF_ASSERT(Syscall::Call(__NR_uname, (testcase), (arg)) == (expected_value))

// Our uname() system call returns ErrorCode(1) for success and
// ErrorCode(0) for failure. Syscall::Call() turns this into an
// exit code of -1 or 0.
#define EXPECT_FAILURE 0
#define EXPECT_SUCCESS -1

// A couple of our tests behave differently on 32bit and 64bit systems, as
// there is no way for a 32bit system call to pass in a 64bit system call
// argument "arg".
// We expect these tests to succeed on 64bit systems, but to tail on 32bit
// systems.
#define EXPT64_SUCCESS (sizeof(void*) > 4 ? EXPECT_SUCCESS : EXPECT_FAILURE)
BPF_TEST_C(SandboxBPF, AllBitTests, AllBitTestPolicy) {
  // 32bit test: all of 0x0 (should always be true)
  BITMASK_TEST( 0,                   0, ALLBITS32,          0, EXPECT_SUCCESS);
  BITMASK_TEST( 0,                   1, ALLBITS32,          0, EXPECT_SUCCESS);
  BITMASK_TEST( 0,                   3, ALLBITS32,          0, EXPECT_SUCCESS);
  BITMASK_TEST( 0,         0xFFFFFFFFU, ALLBITS32,          0, EXPECT_SUCCESS);
  BITMASK_TEST( 0,                -1LL, ALLBITS32,          0, EXPECT_SUCCESS);

  // 32bit test: all of 0x1
  BITMASK_TEST( 1,                   0, ALLBITS32,        0x1, EXPECT_FAILURE);
  BITMASK_TEST( 1,                   1, ALLBITS32,        0x1, EXPECT_SUCCESS);
  BITMASK_TEST( 1,                   2, ALLBITS32,        0x1, EXPECT_FAILURE);
  BITMASK_TEST( 1,                   3, ALLBITS32,        0x1, EXPECT_SUCCESS);

  // 32bit test: all of 0x3
  BITMASK_TEST( 2,                   0, ALLBITS32,        0x3, EXPECT_FAILURE);
  BITMASK_TEST( 2,                   1, ALLBITS32,        0x3, EXPECT_FAILURE);
  BITMASK_TEST( 2,                   2, ALLBITS32,        0x3, EXPECT_FAILURE);
  BITMASK_TEST( 2,                   3, ALLBITS32,        0x3, EXPECT_SUCCESS);
  BITMASK_TEST( 2,                   7, ALLBITS32,        0x3, EXPECT_SUCCESS);

  // 32bit test: all of 0x80000000
  BITMASK_TEST( 3,                   0, ALLBITS32, 0x80000000, EXPECT_FAILURE);
  BITMASK_TEST( 3,         0x40000000U, ALLBITS32, 0x80000000, EXPECT_FAILURE);
  BITMASK_TEST( 3,         0x80000000U, ALLBITS32, 0x80000000, EXPECT_SUCCESS);
  BITMASK_TEST( 3,         0xC0000000U, ALLBITS32, 0x80000000, EXPECT_SUCCESS);
  BITMASK_TEST( 3,       -0x80000000LL, ALLBITS32, 0x80000000, EXPECT_SUCCESS);

  // 64bit test: all of 0x0 (should always be true)
  BITMASK_TEST( 4,                   0, ALLBITS64,          0, EXPECT_SUCCESS);
  BITMASK_TEST( 4,                   1, ALLBITS64,          0, EXPECT_SUCCESS);
  BITMASK_TEST( 4,                   3, ALLBITS64,          0, EXPECT_SUCCESS);
  BITMASK_TEST( 4,         0xFFFFFFFFU, ALLBITS64,          0, EXPECT_SUCCESS);
  BITMASK_TEST( 4,       0x100000000LL, ALLBITS64,          0, EXPECT_SUCCESS);
  BITMASK_TEST( 4,       0x300000000LL, ALLBITS64,          0, EXPECT_SUCCESS);
  BITMASK_TEST( 4,0x8000000000000000LL, ALLBITS64,          0, EXPECT_SUCCESS);
  BITMASK_TEST( 4,                -1LL, ALLBITS64,          0, EXPECT_SUCCESS);

  // 64bit test: all of 0x1
  BITMASK_TEST( 5,                   0, ALLBITS64,          1, EXPECT_FAILURE);
  BITMASK_TEST( 5,                   1, ALLBITS64,          1, EXPECT_SUCCESS);
  BITMASK_TEST( 5,                   2, ALLBITS64,          1, EXPECT_FAILURE);
  BITMASK_TEST( 5,                   3, ALLBITS64,          1, EXPECT_SUCCESS);
  BITMASK_TEST( 5,       0x100000000LL, ALLBITS64,          1, EXPECT_FAILURE);
  BITMASK_TEST( 5,       0x100000001LL, ALLBITS64,          1, EXPECT_SUCCESS);
  BITMASK_TEST( 5,       0x100000002LL, ALLBITS64,          1, EXPECT_FAILURE);
  BITMASK_TEST( 5,       0x100000003LL, ALLBITS64,          1, EXPECT_SUCCESS);

  // 64bit test: all of 0x3
  BITMASK_TEST( 6,                   0, ALLBITS64,          3, EXPECT_FAILURE);
  BITMASK_TEST( 6,                   1, ALLBITS64,          3, EXPECT_FAILURE);
  BITMASK_TEST( 6,                   2, ALLBITS64,          3, EXPECT_FAILURE);
  BITMASK_TEST( 6,                   3, ALLBITS64,          3, EXPECT_SUCCESS);
  BITMASK_TEST( 6,                   7, ALLBITS64,          3, EXPECT_SUCCESS);
  BITMASK_TEST( 6,       0x100000000LL, ALLBITS64,          3, EXPECT_FAILURE);
  BITMASK_TEST( 6,       0x100000001LL, ALLBITS64,          3, EXPECT_FAILURE);
  BITMASK_TEST( 6,       0x100000002LL, ALLBITS64,          3, EXPECT_FAILURE);
  BITMASK_TEST( 6,       0x100000003LL, ALLBITS64,          3, EXPECT_SUCCESS);
  BITMASK_TEST( 6,       0x100000007LL, ALLBITS64,          3, EXPECT_SUCCESS);

  // 64bit test: all of 0x80000000
  BITMASK_TEST( 7,                   0, ALLBITS64, 0x80000000, EXPECT_FAILURE);
  BITMASK_TEST( 7,         0x40000000U, ALLBITS64, 0x80000000, EXPECT_FAILURE);
  BITMASK_TEST( 7,         0x80000000U, ALLBITS64, 0x80000000, EXPECT_SUCCESS);
  BITMASK_TEST( 7,         0xC0000000U, ALLBITS64, 0x80000000, EXPECT_SUCCESS);
  BITMASK_TEST( 7,       -0x80000000LL, ALLBITS64, 0x80000000, EXPECT_SUCCESS);
  BITMASK_TEST( 7,       0x100000000LL, ALLBITS64, 0x80000000, EXPECT_FAILURE);
  BITMASK_TEST( 7,       0x140000000LL, ALLBITS64, 0x80000000, EXPECT_FAILURE);
  BITMASK_TEST( 7,       0x180000000LL, ALLBITS64, 0x80000000, EXPECT_SUCCESS);
  BITMASK_TEST( 7,       0x1C0000000LL, ALLBITS64, 0x80000000, EXPECT_SUCCESS);
  BITMASK_TEST( 7,      -0x180000000LL, ALLBITS64, 0x80000000, EXPECT_SUCCESS);

  // 64bit test: all of 0x100000000
  BITMASK_TEST( 8,       0x000000000LL, ALLBITS64,0x100000000, EXPECT_FAILURE);
  BITMASK_TEST( 8,       0x100000000LL, ALLBITS64,0x100000000, EXPT64_SUCCESS);
  BITMASK_TEST( 8,       0x200000000LL, ALLBITS64,0x100000000, EXPECT_FAILURE);
  BITMASK_TEST( 8,       0x300000000LL, ALLBITS64,0x100000000, EXPT64_SUCCESS);
  BITMASK_TEST( 8,       0x000000001LL, ALLBITS64,0x100000000, EXPECT_FAILURE);
  BITMASK_TEST( 8,       0x100000001LL, ALLBITS64,0x100000000, EXPT64_SUCCESS);
  BITMASK_TEST( 8,       0x200000001LL, ALLBITS64,0x100000000, EXPECT_FAILURE);
  BITMASK_TEST( 8,       0x300000001LL, ALLBITS64,0x100000000, EXPT64_SUCCESS);

  // 64bit test: all of 0x300000000
  BITMASK_TEST( 9,       0x000000000LL, ALLBITS64,0x300000000, EXPECT_FAILURE);
  BITMASK_TEST( 9,       0x100000000LL, ALLBITS64,0x300000000, EXPECT_FAILURE);
  BITMASK_TEST( 9,       0x200000000LL, ALLBITS64,0x300000000, EXPECT_FAILURE);
  BITMASK_TEST( 9,       0x300000000LL, ALLBITS64,0x300000000, EXPT64_SUCCESS);
  BITMASK_TEST( 9,       0x700000000LL, ALLBITS64,0x300000000, EXPT64_SUCCESS);
  BITMASK_TEST( 9,       0x000000001LL, ALLBITS64,0x300000000, EXPECT_FAILURE);
  BITMASK_TEST( 9,       0x100000001LL, ALLBITS64,0x300000000, EXPECT_FAILURE);
  BITMASK_TEST( 9,       0x200000001LL, ALLBITS64,0x300000000, EXPECT_FAILURE);
  BITMASK_TEST( 9,       0x300000001LL, ALLBITS64,0x300000000, EXPT64_SUCCESS);
  BITMASK_TEST( 9,       0x700000001LL, ALLBITS64,0x300000000, EXPT64_SUCCESS);

  // 64bit test: all of 0x100000001
  BITMASK_TEST(10,       0x000000000LL, ALLBITS64,0x100000001, EXPECT_FAILURE);
  BITMASK_TEST(10,       0x000000001LL, ALLBITS64,0x100000001, EXPECT_FAILURE);
  BITMASK_TEST(10,       0x100000000LL, ALLBITS64,0x100000001, EXPECT_FAILURE);
  BITMASK_TEST(10,       0x100000001LL, ALLBITS64,0x100000001, EXPT64_SUCCESS);
  BITMASK_TEST(10,         0xFFFFFFFFU, ALLBITS64,0x100000001, EXPECT_FAILURE);
  BITMASK_TEST(10,                 -1L, ALLBITS64,0x100000001, EXPT64_SUCCESS);
}

class AnyBitTestPolicy : public SandboxBPFPolicy {
 public:
  AnyBitTestPolicy() {}
  virtual ErrorCode EvaluateSyscall(SandboxBPF* sandbox,
                                    int sysno) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AnyBitTestPolicy);
};

ErrorCode AnyBitTestPolicy::EvaluateSyscall(SandboxBPF* sandbox,
                                            int sysno) const {
  DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
  // Test the OP_HAS_ANY_BITS conditional test operator with a couple of
  // different bitmasks. We try to find bitmasks that could conceivably
  // touch corner cases.
  // For all of these tests, we override the uname(). We can make use with
  // a single system call number, as we use the first system call argument to
  // select the different bit masks that we want to test against.
  if (sysno == __NR_uname) {
    return sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, 0,
           sandbox->Cond(1, ErrorCode::TP_32BIT, ErrorCode::OP_HAS_ANY_BITS,
                         0x0,
                         ErrorCode(1), ErrorCode(0)),

           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, 1,
           sandbox->Cond(1, ErrorCode::TP_32BIT, ErrorCode::OP_HAS_ANY_BITS,
                         0x1,
                         ErrorCode(1), ErrorCode(0)),

           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, 2,
           sandbox->Cond(1, ErrorCode::TP_32BIT, ErrorCode::OP_HAS_ANY_BITS,
                         0x3,
                         ErrorCode(1), ErrorCode(0)),

           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, 3,
           sandbox->Cond(1, ErrorCode::TP_32BIT, ErrorCode::OP_HAS_ANY_BITS,
                         0x80000000,
                         ErrorCode(1), ErrorCode(0)),

           // All the following tests don't really make much sense on 32bit
           // systems. They will always evaluate as false.
           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, 4,
           sandbox->Cond(1, ErrorCode::TP_64BIT, ErrorCode::OP_HAS_ANY_BITS,
                         0x0,
                         ErrorCode(1), ErrorCode(0)),

           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, 5,
           sandbox->Cond(1, ErrorCode::TP_64BIT, ErrorCode::OP_HAS_ANY_BITS,
                         0x1,
                         ErrorCode(1), ErrorCode(0)),

           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, 6,
           sandbox->Cond(1, ErrorCode::TP_64BIT, ErrorCode::OP_HAS_ANY_BITS,
                         0x3,
                         ErrorCode(1), ErrorCode(0)),

           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, 7,
           sandbox->Cond(1, ErrorCode::TP_64BIT, ErrorCode::OP_HAS_ANY_BITS,
                         0x80000000,
                         ErrorCode(1), ErrorCode(0)),

           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, 8,
           sandbox->Cond(1, ErrorCode::TP_64BIT, ErrorCode::OP_HAS_ANY_BITS,
                         0x100000000ULL,
                         ErrorCode(1), ErrorCode(0)),

           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, 9,
           sandbox->Cond(1, ErrorCode::TP_64BIT, ErrorCode::OP_HAS_ANY_BITS,
                         0x300000000ULL,
                         ErrorCode(1), ErrorCode(0)),

           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, 10,
           sandbox->Cond(1, ErrorCode::TP_64BIT, ErrorCode::OP_HAS_ANY_BITS,
                         0x100000001ULL,
                         ErrorCode(1), ErrorCode(0)),

                         sandbox->Kill("Invalid test case number"))))))))))));
  }
  return ErrorCode(ErrorCode::ERR_ALLOWED);
}

BPF_TEST_C(SandboxBPF, AnyBitTests, AnyBitTestPolicy) {
  // 32bit test: any of 0x0 (should always be false)
  BITMASK_TEST( 0,                   0, ANYBITS32,        0x0, EXPECT_FAILURE);
  BITMASK_TEST( 0,                   1, ANYBITS32,        0x0, EXPECT_FAILURE);
  BITMASK_TEST( 0,                   3, ANYBITS32,        0x0, EXPECT_FAILURE);
  BITMASK_TEST( 0,         0xFFFFFFFFU, ANYBITS32,        0x0, EXPECT_FAILURE);
  BITMASK_TEST( 0,                -1LL, ANYBITS32,        0x0, EXPECT_FAILURE);

  // 32bit test: any of 0x1
  BITMASK_TEST( 1,                   0, ANYBITS32,        0x1, EXPECT_FAILURE);
  BITMASK_TEST( 1,                   1, ANYBITS32,        0x1, EXPECT_SUCCESS);
  BITMASK_TEST( 1,                   2, ANYBITS32,        0x1, EXPECT_FAILURE);
  BITMASK_TEST( 1,                   3, ANYBITS32,        0x1, EXPECT_SUCCESS);

  // 32bit test: any of 0x3
  BITMASK_TEST( 2,                   0, ANYBITS32,        0x3, EXPECT_FAILURE);
  BITMASK_TEST( 2,                   1, ANYBITS32,        0x3, EXPECT_SUCCESS);
  BITMASK_TEST( 2,                   2, ANYBITS32,        0x3, EXPECT_SUCCESS);
  BITMASK_TEST( 2,                   3, ANYBITS32,        0x3, EXPECT_SUCCESS);
  BITMASK_TEST( 2,                   7, ANYBITS32,        0x3, EXPECT_SUCCESS);

  // 32bit test: any of 0x80000000
  BITMASK_TEST( 3,                   0, ANYBITS32, 0x80000000, EXPECT_FAILURE);
  BITMASK_TEST( 3,         0x40000000U, ANYBITS32, 0x80000000, EXPECT_FAILURE);
  BITMASK_TEST( 3,         0x80000000U, ANYBITS32, 0x80000000, EXPECT_SUCCESS);
  BITMASK_TEST( 3,         0xC0000000U, ANYBITS32, 0x80000000, EXPECT_SUCCESS);
  BITMASK_TEST( 3,       -0x80000000LL, ANYBITS32, 0x80000000, EXPECT_SUCCESS);

  // 64bit test: any of 0x0 (should always be false)
  BITMASK_TEST( 4,                   0, ANYBITS64,        0x0, EXPECT_FAILURE);
  BITMASK_TEST( 4,                   1, ANYBITS64,        0x0, EXPECT_FAILURE);
  BITMASK_TEST( 4,                   3, ANYBITS64,        0x0, EXPECT_FAILURE);
  BITMASK_TEST( 4,         0xFFFFFFFFU, ANYBITS64,        0x0, EXPECT_FAILURE);
  BITMASK_TEST( 4,       0x100000000LL, ANYBITS64,        0x0, EXPECT_FAILURE);
  BITMASK_TEST( 4,       0x300000000LL, ANYBITS64,        0x0, EXPECT_FAILURE);
  BITMASK_TEST( 4,0x8000000000000000LL, ANYBITS64,        0x0, EXPECT_FAILURE);
  BITMASK_TEST( 4,                -1LL, ANYBITS64,        0x0, EXPECT_FAILURE);

  // 64bit test: any of 0x1
  BITMASK_TEST( 5,                   0, ANYBITS64,        0x1, EXPECT_FAILURE);
  BITMASK_TEST( 5,                   1, ANYBITS64,        0x1, EXPECT_SUCCESS);
  BITMASK_TEST( 5,                   2, ANYBITS64,        0x1, EXPECT_FAILURE);
  BITMASK_TEST( 5,                   3, ANYBITS64,        0x1, EXPECT_SUCCESS);
  BITMASK_TEST( 5,       0x100000001LL, ANYBITS64,        0x1, EXPECT_SUCCESS);
  BITMASK_TEST( 5,       0x100000000LL, ANYBITS64,        0x1, EXPECT_FAILURE);
  BITMASK_TEST( 5,       0x100000002LL, ANYBITS64,        0x1, EXPECT_FAILURE);
  BITMASK_TEST( 5,       0x100000003LL, ANYBITS64,        0x1, EXPECT_SUCCESS);

  // 64bit test: any of 0x3
  BITMASK_TEST( 6,                   0, ANYBITS64,        0x3, EXPECT_FAILURE);
  BITMASK_TEST( 6,                   1, ANYBITS64,        0x3, EXPECT_SUCCESS);
  BITMASK_TEST( 6,                   2, ANYBITS64,        0x3, EXPECT_SUCCESS);
  BITMASK_TEST( 6,                   3, ANYBITS64,        0x3, EXPECT_SUCCESS);
  BITMASK_TEST( 6,                   7, ANYBITS64,        0x3, EXPECT_SUCCESS);
  BITMASK_TEST( 6,       0x100000000LL, ANYBITS64,        0x3, EXPECT_FAILURE);
  BITMASK_TEST( 6,       0x100000001LL, ANYBITS64,        0x3, EXPECT_SUCCESS);
  BITMASK_TEST( 6,       0x100000002LL, ANYBITS64,        0x3, EXPECT_SUCCESS);
  BITMASK_TEST( 6,       0x100000003LL, ANYBITS64,        0x3, EXPECT_SUCCESS);
  BITMASK_TEST( 6,       0x100000007LL, ANYBITS64,        0x3, EXPECT_SUCCESS);

  // 64bit test: any of 0x80000000
  BITMASK_TEST( 7,                   0, ANYBITS64, 0x80000000, EXPECT_FAILURE);
  BITMASK_TEST( 7,         0x40000000U, ANYBITS64, 0x80000000, EXPECT_FAILURE);
  BITMASK_TEST( 7,         0x80000000U, ANYBITS64, 0x80000000, EXPECT_SUCCESS);
  BITMASK_TEST( 7,         0xC0000000U, ANYBITS64, 0x80000000, EXPECT_SUCCESS);
  BITMASK_TEST( 7,       -0x80000000LL, ANYBITS64, 0x80000000, EXPECT_SUCCESS);
  BITMASK_TEST( 7,       0x100000000LL, ANYBITS64, 0x80000000, EXPECT_FAILURE);
  BITMASK_TEST( 7,       0x140000000LL, ANYBITS64, 0x80000000, EXPECT_FAILURE);
  BITMASK_TEST( 7,       0x180000000LL, ANYBITS64, 0x80000000, EXPECT_SUCCESS);
  BITMASK_TEST( 7,       0x1C0000000LL, ANYBITS64, 0x80000000, EXPECT_SUCCESS);
  BITMASK_TEST( 7,      -0x180000000LL, ANYBITS64, 0x80000000, EXPECT_SUCCESS);

  // 64bit test: any of 0x100000000
  BITMASK_TEST( 8,       0x000000000LL, ANYBITS64,0x100000000, EXPECT_FAILURE);
  BITMASK_TEST( 8,       0x100000000LL, ANYBITS64,0x100000000, EXPT64_SUCCESS);
  BITMASK_TEST( 8,       0x200000000LL, ANYBITS64,0x100000000, EXPECT_FAILURE);
  BITMASK_TEST( 8,       0x300000000LL, ANYBITS64,0x100000000, EXPT64_SUCCESS);
  BITMASK_TEST( 8,       0x000000001LL, ANYBITS64,0x100000000, EXPECT_FAILURE);
  BITMASK_TEST( 8,       0x100000001LL, ANYBITS64,0x100000000, EXPT64_SUCCESS);
  BITMASK_TEST( 8,       0x200000001LL, ANYBITS64,0x100000000, EXPECT_FAILURE);
  BITMASK_TEST( 8,       0x300000001LL, ANYBITS64,0x100000000, EXPT64_SUCCESS);

  // 64bit test: any of 0x300000000
  BITMASK_TEST( 9,       0x000000000LL, ANYBITS64,0x300000000, EXPECT_FAILURE);
  BITMASK_TEST( 9,       0x100000000LL, ANYBITS64,0x300000000, EXPT64_SUCCESS);
  BITMASK_TEST( 9,       0x200000000LL, ANYBITS64,0x300000000, EXPT64_SUCCESS);
  BITMASK_TEST( 9,       0x300000000LL, ANYBITS64,0x300000000, EXPT64_SUCCESS);
  BITMASK_TEST( 9,       0x700000000LL, ANYBITS64,0x300000000, EXPT64_SUCCESS);
  BITMASK_TEST( 9,       0x000000001LL, ANYBITS64,0x300000000, EXPECT_FAILURE);
  BITMASK_TEST( 9,       0x100000001LL, ANYBITS64,0x300000000, EXPT64_SUCCESS);
  BITMASK_TEST( 9,       0x200000001LL, ANYBITS64,0x300000000, EXPT64_SUCCESS);
  BITMASK_TEST( 9,       0x300000001LL, ANYBITS64,0x300000000, EXPT64_SUCCESS);
  BITMASK_TEST( 9,       0x700000001LL, ANYBITS64,0x300000000, EXPT64_SUCCESS);

  // 64bit test: any of 0x100000001
  BITMASK_TEST( 10,      0x000000000LL, ANYBITS64,0x100000001, EXPECT_FAILURE);
  BITMASK_TEST( 10,      0x000000001LL, ANYBITS64,0x100000001, EXPECT_SUCCESS);
  BITMASK_TEST( 10,      0x100000000LL, ANYBITS64,0x100000001, EXPT64_SUCCESS);
  BITMASK_TEST( 10,      0x100000001LL, ANYBITS64,0x100000001, EXPECT_SUCCESS);
  BITMASK_TEST( 10,        0xFFFFFFFFU, ANYBITS64,0x100000001, EXPECT_SUCCESS);
  BITMASK_TEST( 10,                -1L, ANYBITS64,0x100000001, EXPECT_SUCCESS);
}

intptr_t PthreadTrapHandler(const struct arch_seccomp_data& args, void* aux) {
  if (args.args[0] != (CLONE_CHILD_CLEARTID | CLONE_CHILD_SETTID | SIGCHLD)) {
    // We expect to get called for an attempt to fork(). No need to log that
    // call. But if we ever get called for anything else, we want to verbosely
    // print as much information as possible.
    const char* msg = (const char*)aux;
    printf(
        "Clone() was called with unexpected arguments\n"
        "  nr: %d\n"
        "  1: 0x%llX\n"
        "  2: 0x%llX\n"
        "  3: 0x%llX\n"
        "  4: 0x%llX\n"
        "  5: 0x%llX\n"
        "  6: 0x%llX\n"
        "%s\n",
        args.nr,
        (long long)args.args[0],
        (long long)args.args[1],
        (long long)args.args[2],
        (long long)args.args[3],
        (long long)args.args[4],
        (long long)args.args[5],
        msg);
  }
  return -EPERM;
}

class PthreadPolicyEquality : public SandboxBPFPolicy {
 public:
  PthreadPolicyEquality() {}
  virtual ErrorCode EvaluateSyscall(SandboxBPF* sandbox,
                                    int sysno) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(PthreadPolicyEquality);
};

ErrorCode PthreadPolicyEquality::EvaluateSyscall(SandboxBPF* sandbox,
                                                 int sysno) const {
  DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
  // This policy allows creating threads with pthread_create(). But it
  // doesn't allow any other uses of clone(). Most notably, it does not
  // allow callers to implement fork() or vfork() by passing suitable flags
  // to the clone() system call.
  if (sysno == __NR_clone) {
    // We have seen two different valid combinations of flags. Glibc
    // uses the more modern flags, sets the TLS from the call to clone(), and
    // uses futexes to monitor threads. Android's C run-time library, doesn't
    // do any of this, but it sets the obsolete (and no-op) CLONE_DETACHED.
    // More recent versions of Android don't set CLONE_DETACHED anymore, so
    // the last case accounts for that.
    // The following policy is very strict. It only allows the exact masks
    // that we have seen in known implementations. It is probably somewhat
    // stricter than what we would want to do.
    const uint64_t kGlibcCloneMask =
        CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND |
        CLONE_THREAD | CLONE_SYSVSEM | CLONE_SETTLS |
        CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID;
    const uint64_t kBaseAndroidCloneMask =
        CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND |
        CLONE_THREAD | CLONE_SYSVSEM;
    return sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                         kGlibcCloneMask,
                         ErrorCode(ErrorCode::ERR_ALLOWED),
           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                         kBaseAndroidCloneMask | CLONE_DETACHED,
                         ErrorCode(ErrorCode::ERR_ALLOWED),
           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                         kBaseAndroidCloneMask,
                         ErrorCode(ErrorCode::ERR_ALLOWED),
                         sandbox->Trap(PthreadTrapHandler, "Unknown mask"))));
  }
  return ErrorCode(ErrorCode::ERR_ALLOWED);
}

class PthreadPolicyBitMask : public SandboxBPFPolicy {
 public:
  PthreadPolicyBitMask() {}
  virtual ErrorCode EvaluateSyscall(SandboxBPF* sandbox,
                                    int sysno) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(PthreadPolicyBitMask);
};

ErrorCode PthreadPolicyBitMask::EvaluateSyscall(SandboxBPF* sandbox,
                                                int sysno) const {
  DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
  // This policy allows creating threads with pthread_create(). But it
  // doesn't allow any other uses of clone(). Most notably, it does not
  // allow callers to implement fork() or vfork() by passing suitable flags
  // to the clone() system call.
  if (sysno == __NR_clone) {
    // We have seen two different valid combinations of flags. Glibc
    // uses the more modern flags, sets the TLS from the call to clone(), and
    // uses futexes to monitor threads. Android's C run-time library, doesn't
    // do any of this, but it sets the obsolete (and no-op) CLONE_DETACHED.
    // The following policy allows for either combination of flags, but it
    // is generally a little more conservative than strictly necessary. We
    // err on the side of rather safe than sorry.
    // Very noticeably though, we disallow fork() (which is often just a
    // wrapper around clone()).
    return sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_HAS_ANY_BITS,
                         ~uint32(CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|
                                 CLONE_THREAD|CLONE_SYSVSEM|CLONE_SETTLS|
                                 CLONE_PARENT_SETTID|CLONE_CHILD_CLEARTID|
                                 CLONE_DETACHED),
                         sandbox->Trap(PthreadTrapHandler,
                                       "Unexpected CLONE_XXX flag found"),
           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_HAS_ALL_BITS,
                         CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|
                         CLONE_THREAD|CLONE_SYSVSEM,
           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_HAS_ALL_BITS,
                         CLONE_SETTLS|CLONE_PARENT_SETTID|CLONE_CHILD_CLEARTID,
                         ErrorCode(ErrorCode::ERR_ALLOWED),
           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_HAS_ANY_BITS,
                         CLONE_SETTLS|CLONE_PARENT_SETTID|CLONE_CHILD_CLEARTID,
                         sandbox->Trap(PthreadTrapHandler,
                                       "Must set either all or none of the TLS"
                                       " and futex bits in call to clone()"),
                         ErrorCode(ErrorCode::ERR_ALLOWED))),
                         sandbox->Trap(PthreadTrapHandler,
                                       "Missing mandatory CLONE_XXX flags "
                                       "when creating new thread")));
  }
  return ErrorCode(ErrorCode::ERR_ALLOWED);
}

static void* ThreadFnc(void* arg) {
  ++*reinterpret_cast<int*>(arg);
  Syscall::Call(__NR_futex, arg, FUTEX_WAKE, 1, 0, 0, 0);
  return NULL;
}

static void PthreadTest() {
  // Attempt to start a joinable thread. This should succeed.
  pthread_t thread;
  int thread_ran = 0;
  BPF_ASSERT(!pthread_create(&thread, NULL, ThreadFnc, &thread_ran));
  BPF_ASSERT(!pthread_join(thread, NULL));
  BPF_ASSERT(thread_ran);

  // Attempt to start a detached thread. This should succeed.
  thread_ran = 0;
  pthread_attr_t attr;
  BPF_ASSERT(!pthread_attr_init(&attr));
  BPF_ASSERT(!pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED));
  BPF_ASSERT(!pthread_create(&thread, &attr, ThreadFnc, &thread_ran));
  BPF_ASSERT(!pthread_attr_destroy(&attr));
  while (Syscall::Call(__NR_futex, &thread_ran, FUTEX_WAIT, 0, 0, 0, 0) ==
         -EINTR) {
  }
  BPF_ASSERT(thread_ran);

  // Attempt to fork() a process using clone(). This should fail. We use the
  // same flags that glibc uses when calling fork(). But we don't actually
  // try calling the fork() implementation in the C run-time library, as
  // run-time libraries other than glibc might call __NR_fork instead of
  // __NR_clone, and that would introduce a bogus test failure.
  int pid;
  BPF_ASSERT(Syscall::Call(__NR_clone,
                           CLONE_CHILD_CLEARTID | CLONE_CHILD_SETTID | SIGCHLD,
                           0,
                           0,
                           &pid) == -EPERM);
}

BPF_TEST_C(SandboxBPF, PthreadEquality, PthreadPolicyEquality) {
  PthreadTest();
}

BPF_TEST_C(SandboxBPF, PthreadBitMask, PthreadPolicyBitMask) {
  PthreadTest();
}

// libc might not define these even though the kernel supports it.
#ifndef PTRACE_O_TRACESECCOMP
#define PTRACE_O_TRACESECCOMP 0x00000080
#endif

#ifdef PTRACE_EVENT_SECCOMP
#define IS_SECCOMP_EVENT(status) ((status >> 16) == PTRACE_EVENT_SECCOMP)
#else
// When Debian/Ubuntu backported seccomp-bpf support into earlier kernels, they
// changed the value of PTRACE_EVENT_SECCOMP from 7 to 8, since 7 was taken by
// PTRACE_EVENT_STOP (upstream chose to renumber PTRACE_EVENT_STOP to 128).  If
// PTRACE_EVENT_SECCOMP isn't defined, we have no choice but to consider both
// values here.
#define IS_SECCOMP_EVENT(status) ((status >> 16) == 7 || (status >> 16) == 8)
#endif

#if defined(__arm__)
#ifndef PTRACE_SET_SYSCALL
#define PTRACE_SET_SYSCALL 23
#endif
#endif

// Changes the syscall to run for a child being sandboxed using seccomp-bpf with
// PTRACE_O_TRACESECCOMP.  Should only be called when the child is stopped on
// PTRACE_EVENT_SECCOMP.
//
// regs should contain the current set of registers of the child, obtained using
// PTRACE_GETREGS.
//
// Depending on the architecture, this may modify regs, so the caller is
// responsible for committing these changes using PTRACE_SETREGS.
long SetSyscall(pid_t pid, regs_struct* regs, int syscall_number) {
#if defined(__arm__)
  // On ARM, the syscall is changed using PTRACE_SET_SYSCALL.  We cannot use the
  // libc ptrace call as the request parameter is an enum, and
  // PTRACE_SET_SYSCALL may not be in the enum.
  return syscall(__NR_ptrace, PTRACE_SET_SYSCALL, pid, NULL, syscall_number);
#endif

  SECCOMP_PT_SYSCALL(*regs) = syscall_number;
  return 0;
}

const uint16_t kTraceData = 0xcc;

class TraceAllPolicy : public SandboxBPFPolicy {
 public:
  TraceAllPolicy() {}
  virtual ~TraceAllPolicy() {}

  virtual ErrorCode EvaluateSyscall(SandboxBPF* sandbox_compiler,
                                    int system_call_number) const OVERRIDE {
    return ErrorCode(ErrorCode::ERR_TRACE + kTraceData);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TraceAllPolicy);
};

SANDBOX_TEST(SandboxBPF, DISABLE_ON_TSAN(SeccompRetTrace)) {
  if (SandboxBPF::SupportsSeccompSandbox(-1) !=
      sandbox::SandboxBPF::STATUS_AVAILABLE) {
    return;
  }

#if defined(__arm__)
  printf("This test is currently disabled on ARM due to a kernel bug.");
  return;
#endif

  pid_t pid = fork();
  BPF_ASSERT_NE(-1, pid);
  if (pid == 0) {
    pid_t my_pid = getpid();
    BPF_ASSERT_NE(-1, ptrace(PTRACE_TRACEME, -1, NULL, NULL));
    BPF_ASSERT_EQ(0, raise(SIGSTOP));
    SandboxBPF sandbox;
    sandbox.SetSandboxPolicy(new TraceAllPolicy);
    BPF_ASSERT(sandbox.StartSandbox(SandboxBPF::PROCESS_SINGLE_THREADED));

    // getpid is allowed.
    BPF_ASSERT_EQ(my_pid, syscall(__NR_getpid));

    // write to stdout is skipped and returns a fake value.
    BPF_ASSERT_EQ(kExpectedReturnValue,
                  syscall(__NR_write, STDOUT_FILENO, "A", 1));

    // kill is rewritten to exit(kExpectedReturnValue).
    syscall(__NR_kill, my_pid, SIGKILL);

    // Should not be reached.
    BPF_ASSERT(false);
  }

  int status;
  BPF_ASSERT(HANDLE_EINTR(waitpid(pid, &status, WUNTRACED)) != -1);
  BPF_ASSERT(WIFSTOPPED(status));

  BPF_ASSERT_NE(-1, ptrace(PTRACE_SETOPTIONS, pid, NULL,
                           reinterpret_cast<void*>(PTRACE_O_TRACESECCOMP)));
  BPF_ASSERT_NE(-1, ptrace(PTRACE_CONT, pid, NULL, NULL));
  while (true) {
    BPF_ASSERT(HANDLE_EINTR(waitpid(pid, &status, 0)) != -1);
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
      BPF_ASSERT(WIFEXITED(status));
      BPF_ASSERT_EQ(kExpectedReturnValue, WEXITSTATUS(status));
      break;
    }

    if (!WIFSTOPPED(status) || WSTOPSIG(status) != SIGTRAP ||
        !IS_SECCOMP_EVENT(status)) {
      BPF_ASSERT_NE(-1, ptrace(PTRACE_CONT, pid, NULL, NULL));
      continue;
    }

    unsigned long data;
    BPF_ASSERT_NE(-1, ptrace(PTRACE_GETEVENTMSG, pid, NULL, &data));
    BPF_ASSERT_EQ(kTraceData, data);

    regs_struct regs;
    BPF_ASSERT_NE(-1, ptrace(PTRACE_GETREGS, pid, NULL, &regs));
    switch (SECCOMP_PT_SYSCALL(regs)) {
      case __NR_write:
        // Skip writes to stdout, make it return kExpectedReturnValue.  Allow
        // writes to stderr so that BPF_ASSERT messages show up.
        if (SECCOMP_PT_PARM1(regs) == STDOUT_FILENO) {
          BPF_ASSERT_NE(-1, SetSyscall(pid, &regs, -1));
          SECCOMP_PT_RESULT(regs) = kExpectedReturnValue;
          BPF_ASSERT_NE(-1, ptrace(PTRACE_SETREGS, pid, NULL, &regs));
        }
        break;

      case __NR_kill:
        // Rewrite to exit(kExpectedReturnValue).
        BPF_ASSERT_NE(-1, SetSyscall(pid, &regs, __NR_exit));
        SECCOMP_PT_PARM1(regs) = kExpectedReturnValue;
        BPF_ASSERT_NE(-1, ptrace(PTRACE_SETREGS, pid, NULL, &regs));
        break;

      default:
        // Allow all other syscalls.
        break;
    }

    BPF_ASSERT_NE(-1, ptrace(PTRACE_CONT, pid, NULL, NULL));
  }
}

// Android does not expose pread64 nor pwrite64.
#if !defined(OS_ANDROID)

bool FullPwrite64(int fd, const char* buffer, size_t count, off64_t offset) {
  while (count > 0) {
    const ssize_t transfered =
        HANDLE_EINTR(pwrite64(fd, buffer, count, offset));
    if (transfered <= 0 || static_cast<size_t>(transfered) > count) {
      return false;
    }
    count -= transfered;
    buffer += transfered;
    offset += transfered;
  }
  return true;
}

bool FullPread64(int fd, char* buffer, size_t count, off64_t offset) {
  while (count > 0) {
    const ssize_t transfered = HANDLE_EINTR(pread64(fd, buffer, count, offset));
    if (transfered <= 0 || static_cast<size_t>(transfered) > count) {
      return false;
    }
    count -= transfered;
    buffer += transfered;
    offset += transfered;
  }
  return true;
}

bool pread_64_was_forwarded = false;

class TrapPread64Policy : public SandboxBPFPolicy {
 public:
  TrapPread64Policy() {}
  virtual ~TrapPread64Policy() {}

  virtual ErrorCode EvaluateSyscall(SandboxBPF* sandbox_compiler,
                                    int system_call_number) const OVERRIDE {
    // Set the global environment for unsafe traps once.
    if (system_call_number == MIN_SYSCALL) {
      EnableUnsafeTraps();
    }

    if (system_call_number == __NR_pread64) {
      return sandbox_compiler->UnsafeTrap(ForwardPreadHandler, NULL);
    }
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  }

 private:
  static intptr_t ForwardPreadHandler(const struct arch_seccomp_data& args,
                                      void* aux) {
    BPF_ASSERT(args.nr == __NR_pread64);
    pread_64_was_forwarded = true;

    return SandboxBPF::ForwardSyscall(args);
  }
  DISALLOW_COPY_AND_ASSIGN(TrapPread64Policy);
};

// pread(2) takes a 64 bits offset. On 32 bits systems, it will be split
// between two arguments. In this test, we make sure that ForwardSyscall() can
// forward it properly.
BPF_TEST_C(SandboxBPF, Pread64, TrapPread64Policy) {
  ScopedTemporaryFile temp_file;
  const uint64_t kLargeOffset = (static_cast<uint64_t>(1) << 32) | 0xBEEF;
  const char kTestString[] = "This is a test!";
  BPF_ASSERT(FullPwrite64(
      temp_file.fd(), kTestString, sizeof(kTestString), kLargeOffset));

  char read_test_string[sizeof(kTestString)] = {0};
  BPF_ASSERT(FullPread64(temp_file.fd(),
                         read_test_string,
                         sizeof(read_test_string),
                         kLargeOffset));
  BPF_ASSERT_EQ(0, memcmp(kTestString, read_test_string, sizeof(kTestString)));
  BPF_ASSERT(pread_64_was_forwarded);
}

#endif  // !defined(OS_ANDROID)

}  // namespace

}  // namespace sandbox
