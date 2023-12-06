/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/WindowsDiagnostics.h"

#include "nsError.h"

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/NativeNt.h"
#include "mozilla/Types.h"

#include <windows.h>
#include <winnt.h>

#include <functional>

#if defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED) && defined(_M_X64)

namespace mozilla {

static OnSingleStepCallback sOnSingleStepCallback;
static void* sOnSingleStepCallbackState;

MFBT_API AutoOnSingleStepCallback::AutoOnSingleStepCallback(
    OnSingleStepCallback aOnSingleStepCallback, void* aState) {
  MOZ_DIAGNOSTIC_ASSERT(!sOnSingleStepCallback && !sOnSingleStepCallbackState,
                        "Single-stepping is already active");

  sOnSingleStepCallback = std::move(aOnSingleStepCallback);
  sOnSingleStepCallbackState = aState;
}

MFBT_API AutoOnSingleStepCallback::~AutoOnSingleStepCallback() {
  sOnSingleStepCallback = OnSingleStepCallback();
}

// Going though this assembly code turns on the trap flag, which will trigger
// a first single step exception. It is then up to the exception handler to
// keep the trap flag enabled so that a new single step exception gets
// triggered with the following instruction.
MFBT_API MOZ_NEVER_INLINE __attribute__((naked)) void EnableTrapFlag() {
  asm volatile(
      "pushfq;"
      "orw $0x100,(%rsp);"
      "popfq;"
      "retq;");
}

// This function does not do anything particular, but when we reach its address
// while single-stepping the exception handler will know that it is now time to
// let the trap flag turned off.
MFBT_API MOZ_NEVER_INLINE __attribute__((naked)) void DisableTrapFlag() {
  asm volatile("retq;");
}

MFBT_API LONG SingleStepExceptionHandler(_EXCEPTION_POINTERS* aExceptionInfo) {
  if (sOnSingleStepCallback &&
      aExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP) {
    auto instructionPointer = aExceptionInfo->ContextRecord->Rip;
    bool keepOnSingleStepping = false;
    if (instructionPointer != reinterpret_cast<uintptr_t>(&DisableTrapFlag)) {
      keepOnSingleStepping = sOnSingleStepCallback(
          sOnSingleStepCallbackState, aExceptionInfo->ContextRecord);
    }
    if (keepOnSingleStepping) {
      aExceptionInfo->ContextRecord->EFlags |= 0x100;
    } else {
      sOnSingleStepCallback = OnSingleStepCallback();
    }
    return EXCEPTION_CONTINUE_EXECUTION;
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

}  // namespace mozilla

#endif  // MOZ_DIAGNOSTIC_ASSERT_ENABLED && _M_X64
