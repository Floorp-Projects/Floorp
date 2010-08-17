// Copyright (c) 2010, Google Inc.
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

// exception_handler_test.cc: Unit tests for google_breakpad::ExceptionHandler

#include <sys/stat.h>
#include <unistd.h>

#include "breakpad_googletest_includes.h"
#include "client/mac/handler/exception_handler.h"
#include "client/mac/tests/auto_tempdir.h"
#include "common/mac/MachIPC.h"

namespace {
using std::string;
using google_breakpad::AutoTempDir;
using google_breakpad::ExceptionHandler;
using google_breakpad::MachPortSender;
using google_breakpad::MachReceiveMessage;
using google_breakpad::MachSendMessage;
using google_breakpad::ReceivePort;
using testing::Test;

class ExceptionHandlerTest : public Test {
 public:
  AutoTempDir tempDir;
  string lastDumpName;
};

static void Crasher() {
  int *a = (int*)0x42;

  fprintf(stdout, "Going to crash...\n");
  fprintf(stdout, "A = %d", *a);
}

static void SoonToCrash() {
  Crasher();
}

static bool MDCallback(const char *dump_dir, const char *file_name,
                       void *context, bool success) {
  string path(dump_dir);
  path.append("/");
  path.append(file_name);
  path.append(".dmp");

  int fd = *reinterpret_cast<int*>(context);
  (void)write(fd, path.c_str(), path.length() + 1);
  close(fd);
  exit(0);
  // not reached
  return true;
}

TEST_F(ExceptionHandlerTest, InProcess) {
  AutoTempDir tempDir;
  // Give the child process a pipe to report back on.
  int fds[2];
  ASSERT_EQ(0, pipe(fds));
  // Fork off a child process so it can crash.
  pid_t pid = fork();
  if (pid == 0) {
    // In the child process.
    close(fds[0]);
    ExceptionHandler eh(tempDir.path, NULL, MDCallback, &fds[1], true, NULL);
    // crash
    SoonToCrash();
    // not reached
    exit(1);
  }
  // In the parent process.
  ASSERT_NE(-1, pid);
  // Wait for the background process to return the minidump file.
  close(fds[1]);
  char minidump_file[PATH_MAX];
  ssize_t nbytes = read(fds[0], minidump_file, sizeof(minidump_file));
  ASSERT_NE(0, nbytes);
  // Ensure that minidump file exists and is > 0 bytes.
  struct stat st;
  ASSERT_EQ(0, stat(minidump_file, &st));
  ASSERT_LT(0, st.st_size);

  // Child process should have exited with a zero status.
  int ret;
  ASSERT_EQ(pid, waitpid(pid, &ret, 0));
  EXPECT_NE(0, WIFEXITED(ret));
  EXPECT_EQ(0, WEXITSTATUS(ret));
}

static bool ChildMDCallback(const char *dump_dir, const char *file_name,
			    void *context, bool success) {
  ExceptionHandlerTest *self = reinterpret_cast<ExceptionHandlerTest*>(context);
  if (dump_dir && file_name) {
    self->lastDumpName = dump_dir;
    self->lastDumpName += "/";
    self->lastDumpName += file_name;
    self->lastDumpName += ".dmp";
  }
  return true;
}

TEST_F(ExceptionHandlerTest, DumpChildProcess) {
  const int kTimeoutMs = 2000;
  // Create a mach port to receive the child task on.
  char machPortName[128];
  sprintf(machPortName, "ExceptionHandlerTest.%d", getpid());
  ReceivePort parent_recv_port(machPortName);

  // Give the child process a pipe to block on.
  int fds[2];
  ASSERT_EQ(0, pipe(fds));

  // Fork off a child process to dump.
  pid_t pid = fork();
  if (pid == 0) {
    // In the child process
    close(fds[0]);

    // Send parent process the task and thread ports.
    MachSendMessage child_message(0);
    child_message.AddDescriptor(mach_task_self());
    child_message.AddDescriptor(mach_thread_self());

    MachPortSender child_sender(machPortName);
    if (child_sender.SendMessage(child_message, kTimeoutMs) != KERN_SUCCESS)
      exit(1);

    // Wait for the parent process.
    uint8_t data;
    read(fds[1], &data, 1);
    exit(0);
  }
  // In the parent process.
  ASSERT_NE(-1, pid);
  close(fds[1]);

  // Read the child's task and thread ports.
  MachReceiveMessage child_message;
  ASSERT_EQ(KERN_SUCCESS,
	    parent_recv_port.WaitForMessage(&child_message, kTimeoutMs));
  mach_port_t child_task = child_message.GetTranslatedPort(0);
  mach_port_t child_thread = child_message.GetTranslatedPort(1);
  ASSERT_NE(MACH_PORT_NULL, child_task);
  ASSERT_NE(MACH_PORT_NULL, child_thread);

  // Write a minidump of the child process.
  bool result = ExceptionHandler::WriteMinidumpForChild(child_task,
							child_thread,
							tempDir.path,
							ChildMDCallback,
							this);
  ASSERT_EQ(true, result);

  // Ensure that minidump file exists and is > 0 bytes.
  ASSERT_FALSE(lastDumpName.empty());
  struct stat st;
  ASSERT_EQ(0, stat(lastDumpName.c_str(), &st));
  ASSERT_LT(0, st.st_size);

  // Unblock child process
  uint8_t data = 1;
  (void)write(fds[0], &data, 1);

  // Child process should have exited with a zero status.
  int ret;
  ASSERT_EQ(pid, waitpid(pid, &ret, 0));
  EXPECT_NE(0, WIFEXITED(ret));
  EXPECT_EQ(0, WEXITSTATUS(ret));
}

}
