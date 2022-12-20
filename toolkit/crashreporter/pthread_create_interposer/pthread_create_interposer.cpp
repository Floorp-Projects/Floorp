/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include <dlfcn.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"

using mozilla::DebugOnly;

struct SigAltStack {
  void* mem;
  size_t size;
};

struct PthreadCreateParams {
  void* (*start_routine)(void*);
  void* arg;
};

// Install the alternate signal stack, returns a pointer to the memory area we
// mapped to store the stack only if it was installed successfully, otherwise
// returns NULL.
static void* install_sig_alt_stack(size_t size) {
  void* alt_stack_mem = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (alt_stack_mem) {
    stack_t alt_stack = {
        .ss_sp = alt_stack_mem,
        .ss_flags = 0,
        .ss_size = size,
    };

    int rv = sigaltstack(&alt_stack, nullptr);
    if (rv == 0) {
      return alt_stack_mem;
    }

    rv = munmap(alt_stack_mem, size);
    MOZ_ASSERT(rv == 0);
  }

  return nullptr;
}

// Uninstall the alternate signal handler and unmaps it. Does nothing if
// alt_stack_mem is NULL.
static void uninstall_sig_alt_stack(void* alt_stack_ptr) {
  SigAltStack* alt_stack = static_cast<SigAltStack*>(alt_stack_ptr);
  if (alt_stack->mem) {
    stack_t disable_alt_stack = {};
    disable_alt_stack.ss_flags = SS_DISABLE;
    DebugOnly<int> rv = sigaltstack(&disable_alt_stack, nullptr);
    MOZ_ASSERT(rv == 0);
    rv = munmap(alt_stack->mem, alt_stack->size);
    MOZ_ASSERT(rv == 0);
  }
}

// This replaces the routine passed to pthread_create() when a thread is
// started, it handles the alternate signal stack and calls the thread's
// actual routine.
void* set_alt_signal_stack_and_start(PthreadCreateParams* params) {
  void* (*start_routine)(void*) = params->start_routine;
  void* arg = params->arg;
  free(params);

  void* thread_rv = nullptr;
  static const size_t kSigStackSize = std::max(size_t(16384), size_t(SIGSTKSZ));
  void* alt_stack_mem = install_sig_alt_stack(kSigStackSize);
  SigAltStack alt_stack{alt_stack_mem, kSigStackSize};
  pthread_cleanup_push(uninstall_sig_alt_stack, &alt_stack);
  thread_rv = start_routine(arg);
  pthread_cleanup_pop(1);

  return thread_rv;
}

using pthread_create_func_t = int (*)(pthread_t*, const pthread_attr_t*,
                                      void* (*)(void*), void*);

extern "C" {
// This interposer replaces libpthread's pthread_create() so that we can
// inject an alternate signal stack in every new thread.
__attribute__((visibility("default"))) int pthread_create(
    pthread_t* thread, const pthread_attr_t* attr,
    void* (*start_routine)(void*), void* arg) {
  // static const pthread_create_func_t real_pthread_create =
  static const pthread_create_func_t real_pthread_create =
      (pthread_create_func_t)dlsym(RTLD_NEXT, "pthread_create");

  if (real_pthread_create == nullptr) {
    MOZ_CRASH(
        "pthread_create() interposition failed but the interposer function is "
        "still being called, this won't work!");
  }

  if (real_pthread_create == pthread_create) {
    MOZ_CRASH(
        "We could not obtain the real pthread_create(). Calling the symbol we "
        "got would make us enter an infinte loop so stop here instead.");
  }

  PthreadCreateParams* params =
      (PthreadCreateParams*)malloc(sizeof(PthreadCreateParams));
  params->start_routine = start_routine;
  params->arg = arg;

  int result = real_pthread_create(
      thread, attr, (void* (*)(void*))set_alt_signal_stack_and_start, params);

  if (result != 0) {
    free(params);
  }

  return result;
}
}
