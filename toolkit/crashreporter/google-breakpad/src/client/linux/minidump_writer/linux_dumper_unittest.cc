// Copyright (c) 2009, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <limits.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>

#include "breakpad_googletest_includes.h"
#include "client/linux/minidump_writer/linux_dumper.h"
#include "common/linux/file_id.h"
#include "common/memory.h"

using namespace google_breakpad;

// This provides a wrapper around system calls which may be
// interrupted by a signal and return EINTR. See man 7 signal.
#define HANDLE_EINTR(x) ({ \
  typeof(x) __eintr_result__; \
  do { \
    __eintr_result__ = x; \
  } while (__eintr_result__ == -1 && errno == EINTR); \
  __eintr_result__;\
})

namespace {
typedef testing::Test LinuxDumperTest;
}

TEST(LinuxDumperTest, Setup) {
  LinuxDumper dumper(getpid());
}

TEST(LinuxDumperTest, FindMappings) {
  LinuxDumper dumper(getpid());
  ASSERT_TRUE(dumper.Init());

  ASSERT_TRUE(dumper.FindMapping(reinterpret_cast<void*>(getpid)));
  ASSERT_TRUE(dumper.FindMapping(reinterpret_cast<void*>(printf)));
  ASSERT_FALSE(dumper.FindMapping(NULL));
}

TEST(LinuxDumperTest, ThreadList) {
  LinuxDumper dumper(getpid());
  ASSERT_TRUE(dumper.Init());

  ASSERT_GE(dumper.threads().size(), (size_t)1);
  bool found = false;
  for (size_t i = 0; i < dumper.threads().size(); ++i) {
    if (dumper.threads()[i] == getpid()) {
      found = true;
      break;
    }
  }
}

TEST(LinuxDumperTest, VerifyStackReadWithMultipleThreads) {
  static const int kNumberOfThreadsInHelperProgram = 5;
  char kNumberOfThreadsArgument[2];
  sprintf(kNumberOfThreadsArgument, "%d", kNumberOfThreadsInHelperProgram);

  pid_t child_pid = fork();
  if (child_pid == 0) {
    // Set the number of threads
    execl("src/client/linux/linux_dumper_unittest_helper",
          "linux_dumper_unittest_helper",
          kNumberOfThreadsArgument,
          NULL);
    // Kill if we get here.
    printf("Errno from exec: %d", errno);
    FAIL() << "Exec failed: " << strerror(errno);
    exit(0);
  }
  // The sleep is flaky, but prevents us from reading
  // the child process before all threads have been created.
  sleep(1);
  LinuxDumper dumper(child_pid);
  EXPECT_TRUE(dumper.Init());
  EXPECT_EQ((size_t)kNumberOfThreadsInHelperProgram, dumper.threads().size());
  EXPECT_TRUE(dumper.ThreadsSuspend());

  ThreadInfo one_thread;
  for(size_t i = 0; i < dumper.threads().size(); ++i) {
    EXPECT_TRUE(dumper.ThreadInfoGet(dumper.threads()[i], &one_thread));
    // In the helper program, we stored a pointer to the thread id in a
    // specific register. Check that we can recover its value.
#if defined(__ARM_EABI__)
    pid_t *process_tid_location = (pid_t *)(one_thread.regs.uregs[3]);
#elif defined(__i386)
    pid_t *process_tid_location = (pid_t *)(one_thread.regs.ecx);
#elif defined(__x86_64)
    pid_t *process_tid_location = (pid_t *)(one_thread.regs.rcx);
#else
#error This test has not been ported to this platform.
#endif
    pid_t one_thread_id;
    dumper.CopyFromProcess(&one_thread_id,
                           dumper.threads()[i],
                           process_tid_location,
                           4);
    EXPECT_EQ(dumper.threads()[i], one_thread_id);
  }
  kill(child_pid, SIGKILL);
}

TEST(LinuxDumperTest, BuildProcPath) {
  const pid_t pid = getpid();
  LinuxDumper dumper(pid);

  char maps_path[256] = "dummymappath";
  char maps_path_expected[256];
  snprintf(maps_path_expected, sizeof(maps_path_expected),
           "/proc/%d/maps", pid);
  dumper.BuildProcPath(maps_path, pid, "maps");
  ASSERT_STREQ(maps_path, maps_path_expected);

  // In release mode, we expect BuildProcPath to handle the invalid
  // parameters correctly and fill map_path with an empty
  // NULL-terminated string.
#ifdef NDEBUG
  snprintf(maps_path, sizeof(maps_path), "dummymappath");
  dumper.BuildProcPath(maps_path, 0, "maps");
  EXPECT_STREQ(maps_path, "");

  snprintf(maps_path, sizeof(maps_path), "dummymappath");
  dumper.BuildProcPath(maps_path, getpid(), "");
  EXPECT_STREQ(maps_path, "");

  snprintf(maps_path, sizeof(maps_path), "dummymappath");
  dumper.BuildProcPath(maps_path, getpid(), NULL);
  EXPECT_STREQ(maps_path, "");
#endif
}

#if !defined(__ARM_EABI__)
TEST(LinuxDumperTest, MappingsIncludeLinuxGate) {
  LinuxDumper dumper(getpid());
  ASSERT_TRUE(dumper.Init());

  void* linux_gate_loc = reinterpret_cast<void *>(dumper.auxv()[AT_SYSINFO_EHDR]);
  ASSERT_TRUE(linux_gate_loc);
  bool found_linux_gate = false;

  const wasteful_vector<MappingInfo*> mappings = dumper.mappings();
  const MappingInfo* mapping;
  for (unsigned i = 0; i < mappings.size(); ++i) {
    mapping = mappings[i];
    if (!strcmp(mapping->name, kLinuxGateLibraryName)) {
      found_linux_gate = true;
      break;
    }
  }
  EXPECT_TRUE(found_linux_gate);
  EXPECT_EQ(linux_gate_loc, reinterpret_cast<void*>(mapping->start_addr));
  EXPECT_EQ(0, memcmp(linux_gate_loc, ELFMAG, SELFMAG));
}
#endif

TEST(LinuxDumperTest, FileIDsMatch) {
  // Calculate the File ID of our binary using both
  // FileID::ElfFileIdentifier and LinuxDumper::ElfFileIdentifierForMapping
  // and ensure that we get the same result from both.
  char exe_name[PATH_MAX];
  ssize_t len = readlink("/proc/self/exe", exe_name, PATH_MAX - 1);
  ASSERT_NE(len, -1);
  exe_name[len] = '\0';

  int fds[2];
  ASSERT_NE(-1, pipe(fds));

  // fork a child so we can ptrace it
  const pid_t child = fork();
  if (child == 0) {
    close(fds[1]);
    // now wait forever for the parent
    char b;
    HANDLE_EINTR(read(fds[0], &b, sizeof(b)));
    close(fds[0]);
    syscall(__NR_exit);
  }
  close(fds[0]);

  LinuxDumper dumper(child);
  ASSERT_TRUE(dumper.Init());
  const wasteful_vector<MappingInfo*> mappings = dumper.mappings();
  bool found_exe = false;
  unsigned i;
  for (i = 0; i < mappings.size(); ++i) {
    const MappingInfo* mapping = mappings[i];
    if (!strcmp(mapping->name, exe_name)) {
      found_exe = true;
      break;
    }
  }
  ASSERT_TRUE(found_exe);

  uint8_t identifier1[sizeof(MDGUID)];
  uint8_t identifier2[sizeof(MDGUID)];
  EXPECT_TRUE(dumper.ElfFileIdentifierForMapping(*mappings[i], identifier1));
  FileID fileid(exe_name);
  EXPECT_TRUE(fileid.ElfFileIdentifier(identifier2));
  char identifier_string1[37];
  char identifier_string2[37];
  FileID::ConvertIdentifierToString(identifier1, identifier_string1,
                                    37);
  FileID::ConvertIdentifierToString(identifier2, identifier_string2,
                                    37);
  EXPECT_STREQ(identifier_string1, identifier_string2);
  close(fds[1]);
}
