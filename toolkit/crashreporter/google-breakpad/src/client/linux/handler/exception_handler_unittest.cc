// Copyright (c) 2010 Google Inc.
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

#include <string>

#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include "client/linux/handler/exception_handler.h"
#include "client/linux/minidump_writer/minidump_writer.h"
#include "common/linux/eintr_wrapper.h"
#include "common/linux/linux_libc_support.h"
#include "common/linux/linux_syscall_support.h"
#include "breakpad_googletest_includes.h"

using namespace google_breakpad;

static void sigchld_handler(int signo) { }

class ExceptionHandlerTest : public ::testing::Test {
 protected:
  void SetUp() {
    // We need to be able to wait for children, so SIGCHLD cannot be SIG_IGN.
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    ASSERT_NE(sigaction(SIGCHLD, &sa, &old_action), -1);
  }

  void TearDown() {
    sigaction(SIGCHLD, &old_action, NULL);
  }

  struct sigaction old_action;
};

TEST(ExceptionHandlerTest, Simple) {
  ExceptionHandler handler("/tmp", NULL, NULL, NULL, true);
}

static bool DoneCallback(const char* dump_path,
                         const char* minidump_id,
                         void* context,
                         bool succeeded) {
  if (!succeeded)
    return succeeded;

  int fd = (intptr_t) context;
  uint32_t len = my_strlen(minidump_id);
  HANDLE_EINTR(sys_write(fd, &len, sizeof(len)));
  HANDLE_EINTR(sys_write(fd, minidump_id, len));
  sys_close(fd);

  return true;
}

TEST(ExceptionHandlerTest, ChildCrash) {
  int fds[2];
  ASSERT_NE(pipe(fds), -1);

  const pid_t child = fork();
  if (child == 0) {
    close(fds[0]);
    ExceptionHandler handler("/tmp", NULL, DoneCallback, (void*) fds[1],
                             true);
    *reinterpret_cast<int*>(NULL) = 0;
  }
  close(fds[1]);

  int status;
  ASSERT_NE(HANDLE_EINTR(waitpid(child, &status, 0)), -1);
  ASSERT_TRUE(WIFSIGNALED(status));
  ASSERT_EQ(WTERMSIG(status), SIGSEGV);

  struct pollfd pfd;
  memset(&pfd, 0, sizeof(pfd));
  pfd.fd = fds[0];
  pfd.events = POLLIN | POLLERR;

  const int r = HANDLE_EINTR(poll(&pfd, 1, 0));
  ASSERT_EQ(r, 1);
  ASSERT_TRUE(pfd.revents & POLLIN);

  uint32_t len;
  ASSERT_EQ(read(fds[0], &len, sizeof(len)), (ssize_t)sizeof(len));
  ASSERT_LT(len, (uint32_t)2048);
  char* filename = reinterpret_cast<char*>(malloc(len + 1));
  ASSERT_EQ(read(fds[0], filename, len), len);
  filename[len] = 0;
  close(fds[0]);

  const std::string minidump_filename = std::string("/tmp/") + filename +
                                        ".dmp";

  struct stat st;
  ASSERT_EQ(stat(minidump_filename.c_str(), &st), 0);
  ASSERT_GT(st.st_size, 0u);
  unlink(minidump_filename.c_str());
}

static const unsigned kControlMsgSize =
    CMSG_SPACE(sizeof(int)) + CMSG_SPACE(sizeof(struct ucred));

static bool
CrashHandler(const void* crash_context, size_t crash_context_size,
             void* context) {
  const int fd = (intptr_t) context;
  int fds[2];
  pipe(fds);
  struct kernel_msghdr msg = {0};
  struct kernel_iovec iov;
  iov.iov_base = const_cast<void*>(crash_context);
  iov.iov_len = crash_context_size;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  char cmsg[kControlMsgSize];
  memset(cmsg, 0, kControlMsgSize);
  msg.msg_control = cmsg;
  msg.msg_controllen = sizeof(cmsg);

  struct cmsghdr *hdr = CMSG_FIRSTHDR(&msg);
  hdr->cmsg_level = SOL_SOCKET;
  hdr->cmsg_type = SCM_RIGHTS;
  hdr->cmsg_len = CMSG_LEN(sizeof(int));
  *((int*) CMSG_DATA(hdr)) = fds[1];
  hdr = CMSG_NXTHDR((struct msghdr*) &msg, hdr);
  hdr->cmsg_level = SOL_SOCKET;
  hdr->cmsg_type = SCM_CREDENTIALS;
  hdr->cmsg_len = CMSG_LEN(sizeof(struct ucred));
  struct ucred *cred = reinterpret_cast<struct ucred*>(CMSG_DATA(hdr));
  cred->uid = getuid();
  cred->gid = getgid();
  cred->pid = getpid();

  HANDLE_EINTR(sys_sendmsg(fd, &msg, 0));
  sys_close(fds[1]);

  char b;
  HANDLE_EINTR(sys_read(fds[0], &b, 1));

  return true;
}

TEST(ExceptionHandlerTest, ExternalDumper) {
  int fds[2];
  ASSERT_NE(socketpair(AF_UNIX, SOCK_DGRAM, 0, fds), -1);
  static const int on = 1;
  setsockopt(fds[0], SOL_SOCKET, SO_PASSCRED, &on, sizeof(on));
  setsockopt(fds[1], SOL_SOCKET, SO_PASSCRED, &on, sizeof(on));

  const pid_t child = fork();
  if (child == 0) {
    close(fds[0]);
    ExceptionHandler handler("/tmp1", NULL, NULL, (void*) fds[1], true);
    handler.set_crash_handler(CrashHandler);
    *reinterpret_cast<int*>(NULL) = 0;
  }
  close(fds[1]);
  struct msghdr msg = {0};
  struct iovec iov;
  static const unsigned kCrashContextSize =
      sizeof(ExceptionHandler::CrashContext);
  char context[kCrashContextSize];
  char control[kControlMsgSize];
  iov.iov_base = context;
  iov.iov_len = kCrashContextSize;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = control;
  msg.msg_controllen = kControlMsgSize;

  const ssize_t n = HANDLE_EINTR(recvmsg(fds[0], &msg, 0));
  ASSERT_EQ(n, kCrashContextSize);
  ASSERT_EQ(msg.msg_controllen, kControlMsgSize);
  ASSERT_EQ(msg.msg_flags, 0);

  pid_t crashing_pid = -1;
  int signal_fd = -1;
  for (struct cmsghdr *hdr = CMSG_FIRSTHDR(&msg); hdr;
       hdr = CMSG_NXTHDR(&msg, hdr)) {
    if (hdr->cmsg_level != SOL_SOCKET)
      continue;
    if (hdr->cmsg_type == SCM_RIGHTS) {
      const unsigned len = hdr->cmsg_len -
          (((uint8_t*)CMSG_DATA(hdr)) - (uint8_t*)hdr);
      ASSERT_EQ(len, sizeof(int));
      signal_fd = *((int *) CMSG_DATA(hdr));
    } else if (hdr->cmsg_type == SCM_CREDENTIALS) {
      const struct ucred *cred =
          reinterpret_cast<struct ucred*>(CMSG_DATA(hdr));
      crashing_pid = cred->pid;
    }
  }

  ASSERT_NE(crashing_pid, -1);
  ASSERT_NE(signal_fd, -1);

  char templ[] = "/tmp/exception-handler-unittest-XXXXXX";
  mktemp(templ);
  ASSERT_TRUE(WriteMinidump(templ, crashing_pid, context,
                            kCrashContextSize));
  static const char b = 0;
  HANDLE_EINTR(write(signal_fd, &b, 1));

  int status;
  ASSERT_NE(HANDLE_EINTR(waitpid(child, &status, 0)), -1);
  ASSERT_TRUE(WIFSIGNALED(status));
  ASSERT_EQ(WTERMSIG(status), SIGSEGV);

  struct stat st;
  ASSERT_EQ(stat(templ, &st), 0);
  ASSERT_GT(st.st_size, 0u);
  unlink(templ);
}
