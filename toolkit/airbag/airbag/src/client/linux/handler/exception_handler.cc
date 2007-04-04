// Copyright (c) 2006, Google Inc.
// All rights reserved.
//
// Author: Li Liu
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

#include <asm/sigcontext.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cassert>
#include <cstdlib>
#include <ctime>

#include "client/linux/handler/exception_handler.h"
#include "common/linux/guid_creator.h"
#include "google_breakpad/common/minidump_format.h"

namespace google_breakpad {

// Signals that we are interested.
int SigTable[] = {
#if defined(SIGSEGV)
  SIGSEGV,
#endif
#ifdef SIGABRT
  SIGABRT,
#endif
#ifdef SIGFPE
  SIGFPE,
#endif
#ifdef SIGILL
  SIGILL,
#endif
#ifdef SIGBUS
  SIGBUS,
#endif
};

std::vector<ExceptionHandler*> *ExceptionHandler::handler_stack_ = NULL;
int ExceptionHandler::handler_stack_index_ = 0;
pthread_mutex_t ExceptionHandler::handler_stack_mutex_ =
PTHREAD_MUTEX_INITIALIZER;

ExceptionHandler::ExceptionHandler(const string &dump_path,
                                   FilterCallback filter,
                                   MinidumpCallback callback,
                                   void *callback_context,
                                   bool install_handler)
    : filter_(filter),
      callback_(callback),
      callback_context_(callback_context),
      dump_path_(),
      installed_handler_(install_handler) {
  set_dump_path(dump_path);

  if (install_handler) {
    SetupHandler();
    pthread_mutex_lock(&handler_stack_mutex_);
    if (handler_stack_ == NULL)
      handler_stack_ = new std::vector<ExceptionHandler *>;
    handler_stack_->push_back(this);
    pthread_mutex_unlock(&handler_stack_mutex_);
  }
}

ExceptionHandler::~ExceptionHandler() {
  TeardownAllHandler();
  pthread_mutex_lock(&handler_stack_mutex_);
  if (handler_stack_->back() == this) {
    handler_stack_->pop_back();
  } else {
    fprintf(stderr, "warning: removing Breakpad handler out of order\n");
    for (std::vector<ExceptionHandler *>::iterator iterator =
         handler_stack_->begin();
         iterator != handler_stack_->end();
         ++iterator) {
      if (*iterator == this) {
        handler_stack_->erase(iterator);
      }
    }
  }

  if (handler_stack_->empty()) {
    // When destroying the last ExceptionHandler that installed a handler,
    // clean up the handler stack.
    delete handler_stack_;
    handler_stack_ = NULL;
  }
  pthread_mutex_unlock(&handler_stack_mutex_);
}

bool ExceptionHandler::WriteMinidump() {
  return InternalWriteMinidump(0, NULL);
}

// static
bool ExceptionHandler::WriteMinidump(const string &dump_path,
                   MinidumpCallback callback,
                   void *callback_context) {
  ExceptionHandler handler(dump_path, NULL, callback,
                           callback_context, false);
  return handler.InternalWriteMinidump(0, NULL);
}

void ExceptionHandler::SetupHandler() {
  // Signal on a different stack to avoid using the stack
  // of the crashing thread.
  struct sigaltstack sig_stack;
  sig_stack.ss_sp = malloc(MINSIGSTKSZ);
  if (sig_stack.ss_sp == NULL)
    return;
  sig_stack.ss_size = MINSIGSTKSZ;
  sig_stack.ss_flags = 0;

  if (sigaltstack(&sig_stack, NULL) < 0)
    return;
  for (size_t i = 0; i < sizeof(SigTable) / sizeof(SigTable[0]); ++i)
    SetupHandler(SigTable[i]);
}

void ExceptionHandler::SetupHandler(int signo) {
  struct sigaction act, old_act;
  act.sa_handler = HandleException;
  act.sa_flags = SA_ONSTACK;
  if (sigaction(signo, &act, &old_act) < 0)
    return;
  old_handlers_[signo] = old_act.sa_handler;
}

void ExceptionHandler::TeardownHandler(int signo) {
  if (old_handlers_.find(signo) != old_handlers_.end()) {
    struct sigaction act;
    act.sa_handler = old_handlers_[signo];
    act.sa_flags = 0;
    sigaction(signo, &act, 0);
  }
}

void ExceptionHandler::TeardownAllHandler() {
  for (size_t i = 0; i < sizeof(SigTable) / sizeof(SigTable[0]); ++i) {
    TeardownHandler(SigTable[i]);
  }
}

// static
void ExceptionHandler::HandleException(int signo) {
  // In Linux, the context information about the signal is put on the stack of
  // the signal handler frame as value parameter. For some reasons, the
  // prototype of the handler doesn't declare this information as parameter, we
  // will do it by hand. It is the second parameter above the signal number.
  const struct sigcontext *sig_ctx =
    reinterpret_cast<const struct sigcontext *>(&signo + 1);
  pthread_mutex_lock(&handler_stack_mutex_);
  ExceptionHandler *current_handler =
    handler_stack_->at(handler_stack_->size() - ++handler_stack_index_);
  pthread_mutex_unlock(&handler_stack_mutex_);

  // Restore original handler.
  current_handler->TeardownHandler(signo);
  if (current_handler->InternalWriteMinidump(signo, sig_ctx)) {
    // Fully handled this exception, safe to exit.
    exit(EXIT_FAILURE);
  } else {
    // Exception not fully handled, will call the next handler in stack to
    // process it.
    typedef void (*SignalHandler)(int signo, struct sigcontext);
    SignalHandler old_handler =
      reinterpret_cast<SignalHandler>(current_handler->old_handlers_[signo]);
    if (old_handler != NULL)
      old_handler(signo, *sig_ctx);
  }

  pthread_mutex_lock(&handler_stack_mutex_);
  current_handler->SetupHandler(signo);
  --handler_stack_index_;
  // All the handlers in stack have been invoked to handle the exception,
  // normally the process should be terminated and should not reach here.
  // In case we got here, ask the OS to handle it to avoid endless loop,
  // normally the OS will generate a core and termiate the process. This
  // may be desired to debug the program.
  if (handler_stack_index_ == 0)
    signal(signo, SIG_DFL);
  pthread_mutex_unlock(&handler_stack_mutex_);
}

bool ExceptionHandler::InternalWriteMinidump(int signo,
                                    const struct sigcontext *sig_ctx) {
  if (filter_ && !filter_(callback_context_))
    return false;

  GUID guid;
  bool success = false;;
  char guid_str[kGUIDStringLength + 1];
  if (CreateGUID(&guid) && GUIDToString(&guid, guid_str, sizeof(guid_str))) {
    char minidump_path[PATH_MAX];
    snprintf(minidump_path, sizeof(minidump_path), "%s/%s.dmp",
             dump_path_c_,
             guid_str);

    // Block all the signals we want to process when writting minidump.
    // We don't want it to be interrupted.
    sigset_t sig_blocked, sig_old;
    bool blocked = true;
    sigfillset(&sig_blocked);
    for (size_t i = 0; i < sizeof(SigTable) / sizeof(SigTable[0]); ++i)
      sigdelset(&sig_blocked, SigTable[i]);
    if (sigprocmask(SIG_BLOCK, &sig_blocked, &sig_old) != 0) {
      blocked = false;
      fprintf(stderr, "google_breakpad::ExceptionHandler::HandleException: "
                      "failed to block signals.\n");
    }

    success = minidump_generator_.WriteMinidumpToFile(
        minidump_path, signo, sig_ctx);

    // Unblock the signals.
    if (blocked) {
      sigprocmask(SIG_SETMASK, &sig_old, &sig_old);
    }

    if (callback_)
      success = callback_(dump_path_c_, guid_str,
                          callback_context_, success);
  }
  return success;
}

}  // namespace google_breakpad
