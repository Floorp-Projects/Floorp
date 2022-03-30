/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <cassert>
#include <intrin.h>
#include "temporary_stack.h"

// Ensures, for the duration of its lifetime, that the current thread is a
// fiber.
struct Fiberizer {
  bool const was_already_a_fiber = ::IsThreadAFiber();
  Fiberizer() {
    if (!was_already_a_fiber) {
      ::ConvertThreadToFiberEx(NULL, FIBER_FLAG_FLOAT_SWITCH);
    }
  }
  ~Fiberizer() {
    if (!was_already_a_fiber) {
      ::ConvertFiberToThread();
    }
  }
};

// N.B.: This function takes shameless advantage of the fact that its only
// caller is running a void(*)(void *) already, and so it doesn't need to do any
// marshalling of multiple arguments, nor of a return value.
void RunOnTemporaryStack(void (*func)(void*), void* param,
                         size_t reserved_stack_size) {
  Fiberizer fiberizer_;

  struct Args {
    LPVOID calling_fiber;
    void (*func)(void*);
    void* param;
  } args = {
      .calling_fiber = ::GetCurrentFiber(),
      .func = func,
      .param = param,
  };

  // Note: no cross-fiber error propagation is done.
  //
  // We're building without exceptions, but SEH is still present. However,
  // there's no simple way to transfer an SEH exception from one fiber to
  // another -- we could transfer the exception _code_ easily enough, but the
  // associated chain of exception records needs to be on the current stack,
  // or else Windows will assume that the process has become corrupted. [0]
  //
  // [0] Chen, Raymond. "The Old New Thing". "Why is the stack overflow
  //     exception raised before the stack has overflowed?"
  //     https://devblogs.microsoft.com/oldnewthing/20211217-00/?p=106040
  auto const adaptor = [](void* v) {
    Args const& args = *static_cast<Args*>(v);
    (args.func)(args.param);
    ::SwitchToFiber(args.calling_fiber);
  };

  // https://docs.microsoft.com/en-us/windows/win32/procthread/thread-stack-size
  LPVOID const alt_fiber = ::CreateFiberEx(
      0, reserved_stack_size, FIBER_FLAG_FLOAT_SWITCH, adaptor, &args);
  assert(alt_fiber);
  ::SwitchToFiber(alt_fiber);
  ::DeleteFiber(alt_fiber);
}
