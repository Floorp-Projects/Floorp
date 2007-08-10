// Copyright (c) 2007, Google Inc.
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

// Author: Alfred Peng

#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cassert>
#include <cstdlib>
#include <ctime>

#include "client/solaris/handler/exception_handler.h"
#include "common/solaris/guid_creator.h"
#include "common/solaris/message_output.h"
#include "google_breakpad/common/minidump_format.h"
#include "processor/scoped_ptr.h"

namespace google_breakpad {

static const int kStackSize = 1024 * 1024;

// Signals that we are interested.
static const int kSigTable[] = {
  SIGSEGV,
  SIGABRT,
  SIGFPE,
  SIGILL,
  SIGBUS
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
      handler_thread_(0),
      handler_return_value_(false),
      callback_(callback),
      callback_context_(callback_context),
      installed_handler_(install_handler) {
  set_dump_path(dump_path);

  if (install_handler) {
    SetupHandler();
  }

  sem_init(&handler_start_semaphore_, 0, 0);
  sem_init(&handler_finish_semaphore_, 0, 0);
  pthread_attr_t attr;
  scoped_array<char> thread_stack;

  pthread_attr_init(&attr);
  thread_stack.reset(new char[kStackSize]);
  pthread_attr_setstackaddr(&attr, thread_stack.get());
  pthread_attr_setstacksize(&attr, kStackSize);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  pthread_create(&handler_thread_, &attr, ExceptionHandlerThreadMain, this);
  pthread_attr_destroy(&attr);

  if (install_handler) {
    pthread_mutex_lock(&handler_stack_mutex_);

    if (handler_stack_ == NULL)
      handler_stack_ = new std::vector<ExceptionHandler *>;
    handler_stack_->push_back(this);
    pthread_mutex_unlock(&handler_stack_mutex_);
  }
}

ExceptionHandler::~ExceptionHandler() {
  TeardownAllHandlers();
  pthread_mutex_lock(&handler_stack_mutex_);
  if (handler_stack_->back() == this) {
    handler_stack_->pop_back();
  } else {
    print_message1(2, "warning: removing Breakpad handler out of order\n");
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
  pthread_exit((void *)handler_thread_);
  sem_destroy(&handler_start_semaphore_);
  sem_destroy(&handler_finish_semaphore_);
  pthread_mutex_unlock(&handler_stack_mutex_);
}

// static
void* ExceptionHandler::ExceptionHandlerThreadMain(void *lpParameter) {
  ExceptionHandler *self = reinterpret_cast<ExceptionHandler *>(lpParameter);
  assert(self);

  while (true) {
    if (!sem_wait(&(self->handler_start_semaphore_))) {
      // Perform the requested action.
      self->handler_return_value_ = self->InternalWriteMinidump();

      // Allow the requesting thread to proceed.
      sem_post(&(self->handler_finish_semaphore_));
    }
  }

  // Not reached.  This thread will be terminated by ExceptionHandler's
  // destructor.
  return 0;
}

bool ExceptionHandler::WriteMinidump() {
  return WriteMinidumpOnHandlerThread(0);
}

// static
bool ExceptionHandler::WriteMinidump(const string &dump_path,
                                     MinidumpCallback callback,
                                     void *callback_context) {
  ExceptionHandler handler(dump_path, NULL, callback,
                           callback_context, false);
  return handler.WriteMinidumpOnHandlerThread(0);
}

void ExceptionHandler::SetupHandler() {
  // Signal on a different stack to avoid using the stack
  // of the crashing lwp.
  struct sigaltstack sig_stack;
  sig_stack.ss_sp = malloc(MINSIGSTKSZ);
  if (sig_stack.ss_sp == NULL)
    return;
  sig_stack.ss_size = MINSIGSTKSZ;
  sig_stack.ss_flags = 0;

  if (sigaltstack(&sig_stack, NULL) < 0)
    return;
  for (size_t i = 0; i < sizeof(kSigTable) / sizeof(kSigTable[0]); ++i)
    SetupHandler(kSigTable[i]);
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

void ExceptionHandler::TeardownAllHandlers() {
  for (size_t i = 0; i < sizeof(kSigTable) / sizeof(kSigTable[0]); ++i) {
    TeardownHandler(kSigTable[i]);
  }
}

bool ExceptionHandler::WriteMinidumpOnHandlerThread(int signo) {
  // Set up data to be passed in to the handler thread.
  signo_ = signo;

  // This causes the handler thread to call InternalWriteMinidump.
  sem_post(&handler_start_semaphore_);

  // Wait until InternalWriteMinidump is done and collect its return value.
  sem_wait(&handler_finish_semaphore_);
  bool status = handler_return_value_;

  return status;
}

// static
void ExceptionHandler::HandleException(int signo) {
  pthread_mutex_lock(&handler_stack_mutex_);
  ExceptionHandler *current_handler =
    handler_stack_->at(handler_stack_->size() - ++handler_stack_index_);
  pthread_mutex_unlock(&handler_stack_mutex_);

  // Restore original handler.
  current_handler->TeardownHandler(signo);
  if (current_handler->WriteMinidumpOnHandlerThread(signo)) {
    // Fully handled this exception, safe to exit.
    exit(EXIT_FAILURE);
  } else {
    // Exception not fully handled, will call the next handler in stack to
    // process it.
    typedef void (*SignalHandler)(int signo);
    SignalHandler old_handler =
      reinterpret_cast<SignalHandler>(current_handler->old_handlers_[signo]);
    if (old_handler != NULL)
      old_handler(signo);
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

bool ExceptionHandler::InternalWriteMinidump() {
  if (filter_ && !filter_(callback_context_))
    return false;

  bool success = false;
  GUID guid;
  char guid_str[kGUIDStringLength + 1];
  if (CreateGUID(&guid) && GUIDToString(&guid, guid_str, sizeof(guid_str))) {
    char minidump_path[PATH_MAX];
    snprintf(minidump_path, sizeof(minidump_path), "%s/%s.dmp",
             dump_path_c_, guid_str);

    // Block all the signals we want to process when writing minidump.
    // We don't want it to be interrupted.
    sigset_t sig_blocked, sig_old;
    bool blocked = true;
    sigfillset(&sig_blocked);
    for (size_t i = 0; i < sizeof(kSigTable) / sizeof(kSigTable[0]); ++i)
      sigdelset(&sig_blocked, kSigTable[i]);
    if (sigprocmask(SIG_BLOCK, &sig_blocked, &sig_old) != 0) {
      blocked = false;
      print_message1(2, "HandleException: failed to block signals.\n");
    }

    success = minidump_generator_.WriteMinidumpToFile(minidump_path, signo_);

    // Unblock the signals.
    if (blocked)
      sigprocmask(SIG_SETMASK, &sig_old, &sig_old);

    if (callback_)
      success = callback_(dump_path_c_, guid_str, callback_context_, success);
  }
  return success;
}

}  // namespace google_breakpad
