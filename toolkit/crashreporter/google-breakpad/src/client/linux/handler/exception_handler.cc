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

// The ExceptionHandler object installs signal handlers for a number of
// signals. We rely on the signal handler running on the thread which crashed
// in order to identify it. This is true of the synchronous signals (SEGV etc),
// but not true of ABRT. Thus, if you send ABRT to yourself in a program which
// uses ExceptionHandler, you need to use tgkill to direct it to the current
// thread.
//
// The signal flow looks like this:
//
//   SignalHandler (uses a global stack of ExceptionHandler objects to find
//        |         one to handle the signal. If the first rejects it, try
//        |         the second etc...)
//        V
//   HandleSignal ----------------------------| (clones a new process which
//        |                                   |  shares an address space with
//   (wait for cloned                         |  the crashed process. This
//     process)                               |  allows us to ptrace the crashed
//        |                                   |  process)
//        V                                   V
//   (set signal handler to             ThreadEntry (static function to bounce
//    SIG_DFL and rethrow,                    |      back into the object)
//    killing the crashed                     |
//    process)                                V
//                                          DoDump  (writes minidump)
//                                            |
//                                            V
//                                         sys_exit
//

// This code is a little fragmented. Different functions of the ExceptionHandler
// class run in a number of different contexts. Some of them run in a normal
// context and are easy to code, others run in a compromised context and the
// restrictions at the top of minidump_writer.cc apply: no libc and use the
// alternative malloc. Each function should have comment above it detailing the
// context which it runs in.

#include "client/linux/handler/exception_handler.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/signal.h>
#include <sys/syscall.h>
#include <sys/ucontext.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <ucontext.h>
#include <unistd.h>

#include <algorithm>
#include <vector>

#include "common/linux/linux_libc_support.h"
#include "common/linux/linux_syscall_support.h"
#include "common/memory.h"
#include "client/linux/minidump_writer/minidump_writer.h"
#include "common/linux/guid_creator.h"

// A wrapper for the tgkill syscall: send a signal to a specific thread.
static int tgkill(pid_t tgid, pid_t tid, int sig) {
  return syscall(__NR_tgkill, tgid, tid, sig);
  return 0;
}

namespace google_breakpad {

// The list of signals which we consider to be crashes. The default action for
// all these signals must be Core (see man 7 signal) because we rethrow the
// signal after handling it and expect that it'll be fatal.
static const int kExceptionSignals[] = {
  SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS, -1
};

// We can stack multiple exception handlers. In that case, this is the global
// which holds the stack.
std::vector<ExceptionHandler*>* ExceptionHandler::handler_stack_ = NULL;
unsigned ExceptionHandler::handler_stack_index_ = 0;
pthread_mutex_t ExceptionHandler::handler_stack_mutex_ =
    PTHREAD_MUTEX_INITIALIZER;

// Runs before crashing: normal context.
ExceptionHandler::ExceptionHandler(const std::string &dump_path,
                                   FilterCallback filter,
                                   MinidumpCallback callback,
                                   void *callback_context,
                                   bool install_handler)
  : filter_(filter),
    callback_(callback),
    callback_context_(callback_context),
    handler_installed_(install_handler)
{
  Init(dump_path, -1);
}

ExceptionHandler::ExceptionHandler(const std::string &dump_path,
                                   FilterCallback filter,
                                   MinidumpCallback callback,
                                   void* callback_context,
                                   bool install_handler,
                                   const int server_fd)
  : filter_(filter),
    callback_(callback),
    callback_context_(callback_context),
    handler_installed_(install_handler)
{
  Init(dump_path, server_fd);
}

// Runs before crashing: normal context.
ExceptionHandler::~ExceptionHandler() {
  UninstallHandlers();
}

void ExceptionHandler::Init(const std::string &dump_path,
                            const int server_fd)
{
  crash_handler_ = NULL;
  if (0 <= server_fd)
    crash_generation_client_
      .reset(CrashGenerationClient::TryCreate(server_fd));

  if (handler_installed_)
    InstallHandlers();

  if (!IsOutOfProcess())
    set_dump_path(dump_path);

  pthread_mutex_lock(&handler_stack_mutex_);
  if (handler_stack_ == NULL)
    handler_stack_ = new std::vector<ExceptionHandler *>;
  handler_stack_->push_back(this);
  pthread_mutex_unlock(&handler_stack_mutex_);
}

// Runs before crashing: normal context.
bool ExceptionHandler::InstallHandlers() {
  // We run the signal handlers on an alternative stack because we might have
  // crashed because of a stack overflow.

  // We use this value rather than SIGSTKSZ because we would end up overrunning
  // such a small stack.
  static const unsigned kSigStackSize = 8192;

  signal_stack = malloc(kSigStackSize);
  stack_t stack;
  memset(&stack, 0, sizeof(stack));
  stack.ss_sp = signal_stack;
  stack.ss_size = kSigStackSize;

  if (sigaltstack(&stack, NULL) == -1)
    return false;

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sigemptyset(&sa.sa_mask);

  // mask all exception signals when we're handling one of them.
  for (unsigned i = 0; kExceptionSignals[i] != -1; ++i)
    sigaddset(&sa.sa_mask, kExceptionSignals[i]);

  sa.sa_sigaction = SignalHandler;
  sa.sa_flags = SA_ONSTACK | SA_SIGINFO;

  for (unsigned i = 0; kExceptionSignals[i] != -1; ++i) {
    struct sigaction* old = new struct sigaction;
    if (sigaction(kExceptionSignals[i], &sa, old) == -1)
      return false;
    old_handlers_.push_back(std::make_pair(kExceptionSignals[i], old));
  }
  return true;
}

// Runs before crashing: normal context.
void ExceptionHandler::UninstallHandlers() {
  for (unsigned i = 0; i < old_handlers_.size(); ++i) {
    struct sigaction *action =
        reinterpret_cast<struct sigaction*>(old_handlers_[i].second);
    sigaction(old_handlers_[i].first, action, NULL);
    delete action;
  }
  pthread_mutex_lock(&handler_stack_mutex_);
  std::vector<ExceptionHandler*>::iterator handler =
      std::find(handler_stack_->begin(), handler_stack_->end(), this);
  handler_stack_->erase(handler);
  pthread_mutex_unlock(&handler_stack_mutex_);
  old_handlers_.clear();
}

// Runs before crashing: normal context.
void ExceptionHandler::UpdateNextID() {
  GUID guid;
  char guid_str[kGUIDStringLength + 1];
  if (CreateGUID(&guid) && GUIDToString(&guid, guid_str, sizeof(guid_str))) {
    next_minidump_id_ = guid_str;
    next_minidump_id_c_ = next_minidump_id_.c_str();

    char minidump_path[PATH_MAX];
    snprintf(minidump_path, sizeof(minidump_path), "%s/%s.dmp",
             dump_path_c_,
             guid_str);

    next_minidump_path_ = minidump_path;
    next_minidump_path_c_ = next_minidump_path_.c_str();
  }
}

// void ExceptionHandler::set_crash_handler(HandlerCallback callback) {
//   crash_handler_ = callback;
// }

// This function runs in a compromised context: see the top of the file.
// Runs on the crashing thread.
// static
void ExceptionHandler::SignalHandler(int sig, siginfo_t* info, void* uc) {
  // All the exception signals are blocked at this point.
  pthread_mutex_lock(&handler_stack_mutex_);

  if (!handler_stack_->size()) {
    pthread_mutex_unlock(&handler_stack_mutex_);
    return;
  }

  for (int i = handler_stack_->size() - 1; i >= 0; --i) {
    if ((*handler_stack_)[i]->HandleSignal(sig, info, uc)) {
      // successfully handled: We are in an invalid state since an exception
      // signal has been delivered. We don't call the exit handlers because
      // they could end up corrupting on-disk state.
      break;
    }
  }

  pthread_mutex_unlock(&handler_stack_mutex_);

  // Terminate ourselves with the same signal so that our parent knows that we
  // crashed. The default action for all the signals which we catch is Core, so
  // this is the end of us.
  signal(sig, SIG_DFL);

  // TODO(markus): mask signal and return to caller
  tgkill(getpid(), syscall(__NR_gettid), sig);
  _exit(1);

  // not reached.
}

struct ThreadArgument {
  pid_t pid;  // the crashing process
  ExceptionHandler* handler;
  const void* context;  // a CrashContext structure
  size_t context_size;
};

// This is the entry function for the cloned process. We are in a compromised
// context here: see the top of the file.
// static
int ExceptionHandler::ThreadEntry(void *arg) {
  const ThreadArgument *thread_arg = reinterpret_cast<ThreadArgument*>(arg);
  return thread_arg->handler->DoDump(thread_arg->pid, thread_arg->context,
                                     thread_arg->context_size) == false;
}

// This function runs in a compromised context: see the top of the file.
// Runs on the crashing thread.
bool ExceptionHandler::HandleSignal(int sig, siginfo_t* info, void* uc) {
  if (filter_ && !filter_(callback_context_))
    return false;

  // Allow ourselves to be dumped.
  prctl(PR_SET_DUMPABLE, 1);
  CrashContext context;
  memcpy(&context.siginfo, info, sizeof(siginfo_t));
  memcpy(&context.context, uc, sizeof(struct ucontext));
#if !defined(__ARM_EABI__)
  // FP state is not part of user ABI on ARM Linux.
  struct ucontext *uc_ptr = (struct ucontext*)uc;
  if (uc_ptr->uc_mcontext.fpregs) {
    memcpy(&context.float_state,
           uc_ptr->uc_mcontext.fpregs,
           sizeof(context.float_state));
  }
#endif
  context.tid = syscall(__NR_gettid);
  if (crash_handler_ != NULL) {
    if (crash_handler_(&context, sizeof(context),
                       callback_context_)) {
      return true;
    }
  }
  return GenerateDump(&context);
}

// This function may run in a compromised context: see the top of the file.
bool ExceptionHandler::GenerateDump(CrashContext *context) {
  if (IsOutOfProcess())
    return crash_generation_client_->RequestDump(context, sizeof(*context));

  static const unsigned kChildStackSize = 8000;
  PageAllocator allocator;
  uint8_t* stack = (uint8_t*) allocator.Alloc(kChildStackSize);
  if (!stack)
    return false;
  // clone() needs the top-most address. (scrub just to be safe)
  stack += kChildStackSize;
  my_memset(stack - 16, 0, 16);

  ThreadArgument thread_arg;
  thread_arg.handler = this;
  thread_arg.pid = getpid();
  thread_arg.context = context;
  thread_arg.context_size = sizeof(*context);

  const pid_t child = sys_clone(
      ThreadEntry, stack, CLONE_FILES | CLONE_FS | CLONE_UNTRACED,
      &thread_arg, NULL, NULL, NULL);
  int r, status;
  do {
    r = sys_waitpid(child, &status, __WALL);
  } while (r == -1 && errno == EINTR);

  if (r == -1) {
    static const char msg[] = "ExceptionHandler::GenerateDump waitpid failed:";
    sys_write(2, msg, sizeof(msg) - 1);
    sys_write(2, strerror(errno), strlen(strerror(errno)));
    sys_write(2, "\n", 1);
  }

  bool success = r != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0;

  if (callback_)
    success = callback_(dump_path_c_, next_minidump_id_c_,
                        callback_context_, success);

  return success;
}

// This function runs in a compromised context: see the top of the file.
// Runs on the cloned process.
bool ExceptionHandler::DoDump(pid_t crashing_process, const void* context,
                              size_t context_size) {
  return google_breakpad::WriteMinidump(
      next_minidump_path_c_, crashing_process, context, context_size);
}

// static
bool ExceptionHandler::WriteMinidump(const std::string &dump_path,
                                     MinidumpCallback callback,
                                     void* callback_context) {
  return WriteMinidump(dump_path, false, callback, callback_context);
}

// static
bool ExceptionHandler::WriteMinidump(const std::string &dump_path,
                                     bool write_exception_stream,
                                     MinidumpCallback callback,
                                     void* callback_context) {
  ExceptionHandler eh(dump_path, NULL, callback, callback_context, false);
  return eh.WriteMinidump(write_exception_stream);
}

bool ExceptionHandler::WriteMinidump() {
  return WriteMinidump(false);
}

bool ExceptionHandler::WriteMinidump(bool write_exception_stream) {
#if !defined(__ARM_EABI__)
  // Allow ourselves to be dumped.
  sys_prctl(PR_SET_DUMPABLE, 1);

  CrashContext context;
  int getcontext_result = getcontext(&context.context);
  if (getcontext_result)
    return false;
  memcpy(&context.float_state, context.context.uc_mcontext.fpregs,
         sizeof(context.float_state));
  context.tid = sys_gettid();

  if (write_exception_stream) {
    memset(&context.siginfo, 0, sizeof(context.siginfo));
    context.siginfo.si_signo = SIGSTOP;
#if defined(__i386)
    context.siginfo.si_addr =
      reinterpret_cast<void*>(context.context.uc_mcontext.gregs[REG_EIP]);
#elif defined(__x86_64)
    context.siginfo.si_addr =
      reinterpret_cast<void*>(context.context.uc_mcontext.gregs[REG_RIP]);
#elif defined(__ARMEL__)
    context.siginfo.si_addr =
      reinterpret_cast<void*>(context.context.uc_mcontext.arm_ip);
#endif
  }

  bool success = GenerateDump(&context);
  UpdateNextID();
  return success;
#else
  return false;
#endif  // !defined(__ARM_EABI__)
}

// static
bool ExceptionHandler::WriteMinidumpForChild(pid_t child,
                                             pid_t child_blamed_thread,
                                             const std::string &dump_path,
                                             MinidumpCallback callback,
                                             void *callback_context)
{
  // This function is not run in a compromised context.
  ExceptionHandler eh(dump_path, NULL, NULL, NULL, false);
  if (!google_breakpad::WriteMinidump(eh.next_minidump_path_c_,
                                      child,
                                      child_blamed_thread))
      return false;

  return callback ? callback(eh.dump_path_c_, eh.next_minidump_id_c_,
                             callback_context, true) : true;
}

}  // namespace google_breakpad
