// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <asm/unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <vector>

#include "base/basictypes.h"
#include "base/posix/eintr_wrapper.h"
#include "sandbox/linux/seccomp-bpf/bpf_tests.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/seccomp-bpf/syscall.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

// Different platforms use different symbols for the six-argument version
// of the mmap() system call. Test for the correct symbol at compile time.
#ifdef __NR_mmap2
const int kMMapNr = __NR_mmap2;
#else
const int kMMapNr = __NR_mmap;
#endif

TEST(Syscall, WellKnownEntryPoint) {
// Test that SandboxSyscall(-1) is handled specially. Don't do this on ARM,
// where syscall(-1) crashes with SIGILL. Not running the test is fine, as we
// are still testing ARM code in the next set of tests.
#if !defined(__arm__)
  EXPECT_NE(SandboxSyscall(-1), syscall(-1));
#endif

// If possible, test that SandboxSyscall(-1) returns the address right after
// a kernel entry point.
#if defined(__i386__)
  EXPECT_EQ(0x80CDu, ((uint16_t*)SandboxSyscall(-1))[-1]);  // INT 0x80
#elif defined(__x86_64__)
  EXPECT_EQ(0x050Fu, ((uint16_t*)SandboxSyscall(-1))[-1]);  // SYSCALL
#elif defined(__arm__)
#if defined(__thumb__)
  EXPECT_EQ(0xDF00u, ((uint16_t*)SandboxSyscall(-1))[-1]);  // SWI 0
#else
  EXPECT_EQ(0xEF000000u, ((uint32_t*)SandboxSyscall(-1))[-1]);  // SVC 0
#endif
#else
#warning Incomplete test case; need port for target platform
#endif
}

TEST(Syscall, TrivialSyscallNoArgs) {
  // Test that we can do basic system calls
  EXPECT_EQ(SandboxSyscall(__NR_getpid), syscall(__NR_getpid));
}

TEST(Syscall, TrivialSyscallOneArg) {
  int new_fd;
  // Duplicate standard error and close it.
  ASSERT_GE(new_fd = SandboxSyscall(__NR_dup, 2), 0);
  int close_return_value = IGNORE_EINTR(SandboxSyscall(__NR_close, new_fd));
  ASSERT_EQ(close_return_value, 0);
}

// SIGSYS trap handler that will be called on __NR_uname.
intptr_t CopySyscallArgsToAux(const struct arch_seccomp_data& args, void* aux) {
  // |aux| is a pointer to our BPF_AUX.
  std::vector<uint64_t>* const seen_syscall_args =
      static_cast<std::vector<uint64_t>*>(aux);
  BPF_ASSERT(arraysize(args.args) == 6);
  seen_syscall_args->assign(args.args, args.args + arraysize(args.args));
  return -ENOMEM;
}

ErrorCode CopyAllArgsOnUnamePolicy(SandboxBPF* sandbox, int sysno, void* aux) {
  if (!SandboxBPF::IsValidSyscallNumber(sysno)) {
    return ErrorCode(ENOSYS);
  }
  if (sysno == __NR_uname) {
    return sandbox->Trap(CopySyscallArgsToAux, aux);
  } else {
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  }
}

// We are testing SandboxSyscall() by making use of a BPF filter that allows us
// to inspect the system call arguments that the kernel saw.
BPF_TEST(Syscall,
         SyntheticSixArgs,
         CopyAllArgsOnUnamePolicy,
         std::vector<uint64_t> /* BPF_AUX */) {
  const int kExpectedValue = 42;
  // In this test we only pass integers to the kernel. We might want to make
  // additional tests to try other types. What we will see depends on
  // implementation details of kernel BPF filters and we will need to document
  // the expected behavior very clearly.
  int syscall_args[6];
  for (size_t i = 0; i < arraysize(syscall_args); ++i) {
    syscall_args[i] = kExpectedValue + i;
  }

  // We could use pretty much any system call we don't need here. uname() is
  // nice because it doesn't have any dangerous side effects.
  BPF_ASSERT(SandboxSyscall(__NR_uname,
                            syscall_args[0],
                            syscall_args[1],
                            syscall_args[2],
                            syscall_args[3],
                            syscall_args[4],
                            syscall_args[5]) == -ENOMEM);

  // We expect the trap handler to have copied the 6 arguments.
  BPF_ASSERT(BPF_AUX.size() == 6);

  // Don't loop here so that we can see which argument does cause the failure
  // easily from the failing line.
  // uint64_t is the type passed to our SIGSYS handler.
  BPF_ASSERT(BPF_AUX[0] == static_cast<uint64_t>(syscall_args[0]));
  BPF_ASSERT(BPF_AUX[1] == static_cast<uint64_t>(syscall_args[1]));
  BPF_ASSERT(BPF_AUX[2] == static_cast<uint64_t>(syscall_args[2]));
  BPF_ASSERT(BPF_AUX[3] == static_cast<uint64_t>(syscall_args[3]));
  BPF_ASSERT(BPF_AUX[4] == static_cast<uint64_t>(syscall_args[4]));
  BPF_ASSERT(BPF_AUX[5] == static_cast<uint64_t>(syscall_args[5]));
}

TEST(Syscall, ComplexSyscallSixArgs) {
  int fd;
  ASSERT_LE(0, fd = SandboxSyscall(__NR_open, "/dev/null", O_RDWR, 0L));

  // Use mmap() to allocate some read-only memory
  char* addr0;
  ASSERT_NE((char*)NULL,
            addr0 = reinterpret_cast<char*>(
                SandboxSyscall(kMMapNr,
                               (void*)NULL,
                               4096,
                               PROT_READ,
                               MAP_PRIVATE | MAP_ANONYMOUS,
                               fd,
                               0L)));

  // Try to replace the existing mapping with a read-write mapping
  char* addr1;
  ASSERT_EQ(addr0,
            addr1 = reinterpret_cast<char*>(
                SandboxSyscall(kMMapNr,
                               addr0,
                               4096L,
                               PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
                               fd,
                               0L)));
  ++*addr1;  // This should not seg fault

  // Clean up
  EXPECT_EQ(0, SandboxSyscall(__NR_munmap, addr1, 4096L));
  EXPECT_EQ(0, IGNORE_EINTR(SandboxSyscall(__NR_close, fd)));

  // Check that the offset argument (i.e. the sixth argument) is processed
  // correctly.
  ASSERT_GE(fd = SandboxSyscall(__NR_open, "/proc/self/exe", O_RDONLY, 0L), 0);
  char* addr2, *addr3;
  ASSERT_NE((char*)NULL,
            addr2 = reinterpret_cast<char*>(SandboxSyscall(
                kMMapNr, (void*)NULL, 8192L, PROT_READ, MAP_PRIVATE, fd, 0L)));
  ASSERT_NE((char*)NULL,
            addr3 = reinterpret_cast<char*>(SandboxSyscall(kMMapNr,
                                                           (void*)NULL,
                                                           4096L,
                                                           PROT_READ,
                                                           MAP_PRIVATE,
                                                           fd,
#if defined(__NR_mmap2)
                                                           1L
#else
                                                           4096L
#endif
                                                           )));
  EXPECT_EQ(0, memcmp(addr2 + 4096, addr3, 4096));

  // Just to be absolutely on the safe side, also verify that the file
  // contents matches what we are getting from a read() operation.
  char buf[8192];
  EXPECT_EQ(8192, SandboxSyscall(__NR_read, fd, buf, 8192L));
  EXPECT_EQ(0, memcmp(addr2, buf, 8192));

  // Clean up
  EXPECT_EQ(0, SandboxSyscall(__NR_munmap, addr2, 8192L));
  EXPECT_EQ(0, SandboxSyscall(__NR_munmap, addr3, 4096L));
  EXPECT_EQ(0, IGNORE_EINTR(SandboxSyscall(__NR_close, fd)));
}

}  // namespace

}  // namespace sandbox
