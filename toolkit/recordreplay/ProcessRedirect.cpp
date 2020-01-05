/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProcessRedirect.h"

#include "InfallibleVector.h"
#include "ExternalCall.h"
#include "ipc/ChildInternal.h"
#include "ipc/ParentInternal.h"
#include "mozilla/Sprintf.h"

#include <dlfcn.h>
#include <string.h>

namespace mozilla {
namespace recordreplay {

///////////////////////////////////////////////////////////////////////////////
// Redirection Skeleton
///////////////////////////////////////////////////////////////////////////////

static bool CallPreambleHook(PreambleFn aPreamble, size_t aCallId,
                             CallArguments* aArguments) {
  PreambleResult result = aPreamble(aArguments);
  switch (result) {
    case PreambleResult::Veto:
      return true;
    case PreambleResult::IgnoreRedirect:
      RecordReplayInvokeCall(OriginalFunction(aCallId), aArguments);
      return true;
    case PreambleResult::PassThrough: {
      AutoEnsurePassThroughThreadEvents pt;
      RecordReplayInvokeCall(OriginalFunction(aCallId), aArguments);
      return true;
    }
    case PreambleResult::Redirect:
      return false;
  }
  Unreachable();
}

extern "C" {

__attribute__((used)) int RecordReplayInterceptCall(int aCallId,
                                                    CallArguments* aArguments) {
  Redirection& redirection = GetRedirection(aCallId);

  // Call the preamble to see if this call needs special handling, even when
  // events have been passed through.
  if (redirection.mPreamble) {
    if (CallPreambleHook(redirection.mPreamble, aCallId, aArguments)) {
      return 0;
    }
  }

  Thread* thread = Thread::Current();
  Maybe<RecordingEventSection> res;
  res.emplace(thread);

  if (!res.ref().CanAccessEvents()) {
    // When events are passed through, invoke the call with the original stack
    // and register state.
    if (!thread || thread->PassThroughEvents()) {
      // RecordReplayRedirectCall will load the function to call from the
      // return value slot.
      aArguments->Rval<uint8_t*>() = redirection.mOriginalFunction;
      return 1;
    }

    MOZ_RELEASE_ASSERT(thread->HasDivergedFromRecording());

    // After we have diverged from the recording, we can't access the thread's
    // recording anymore.

    // If the redirection has an external preamble hook, call it to see if it
    // can handle this call. The external preamble hook is separate from the
    // normal preamble hook because entering the RecordingEventSection can
    // cause the current thread to diverge from the recording; testing for
    // HasDivergedFromRecording() does not work reliably in the normal preamble.
    if (redirection.mExternalPreamble) {
      if (CallPreambleHook(redirection.mExternalPreamble, aCallId,
                           aArguments)) {
        return 0;
      }
    }

    // If the redirection has an external call hook, try to get its result
    // from another process.
    if (redirection.mExternalCall) {
      if (OnExternalCall(aCallId, aArguments, /* aPopulateOutput = */ true)) {
        return 0;
      }
    }

    if (child::CurrentRepaintCannotFail()) {
      // EnsureNotDivergedFromRecording is going to force us to crash, so fail
      // earlier with a more helpful error message.
      child::ReportFatalError("Could not perform external call: %s\n",
                              redirection.mName);
    }

    // Calling any redirection which performs the standard steps will cause
    // debugger operations that have diverged from the recording to fail.
    EnsureNotDivergedFromRecording(Some(aCallId));
    Unreachable();
  }

  if (IsRecording()) {
    // Call the original function, passing through events while we do so.
    // Destroy the RecordingEventSection so that we don't prevent the file
    // from being flushed in case we end up blocking.
    res.reset();
    thread->SetPassThrough(true);
    RecordReplayInvokeCall(redirection.mOriginalFunction, aArguments);
    thread->SetPassThrough(false);
    res.emplace(thread);
  }

  // Save any system error in case we want to record/replay it.
  ErrorType error = SaveError();

  // Add an event for the thread.
  thread->Events().RecordOrReplayThreadEvent(CallIdToThreadEvent(aCallId));

  // Save any output produced by the call.
  if (redirection.mSaveOutput) {
    redirection.mSaveOutput(thread->Events(), aArguments, &error);
  }

  // Save information about any external calls encountered if we haven't
  // diverged from the recording, in case we diverge and later calls
  // access data produced by this one.
  if (IsReplaying() && redirection.mExternalCall) {
    (void)OnExternalCall(aCallId, aArguments, /* aDiverged = */ false);
  }

  RestoreError(error);
  return 0;
}

// Entry point for all redirections. When generated code jumps here, %rax holds
// the CallEvent being invoked, and all other registers and stack contents are
// the same as when the call was originally invoked. This fills in a
// CallArguments structure with information about the call, before invoking
// RecordReplayInterceptCall.
extern size_t RecordReplayRedirectCall(...);

__asm(
    "_RecordReplayRedirectCall:"

    // Save rbp for backtraces.
    "pushq %rbp;"
    "movq %rsp, %rbp;"

    // Make space for a CallArguments struct on the stack, with a little extra
    // space for alignment.
    "subq $624, %rsp;"

    // Fill in the structure's contents.
    "movq %rdi, 0(%rsp);"
    "movq %rsi, 8(%rsp);"
    "movq %rdx, 16(%rsp);"
    "movq %rcx, 24(%rsp);"
    "movq %r8, 32(%rsp);"
    "movq %r9, 40(%rsp);"
    "movsd %xmm0, 48(%rsp);"
    "movsd %xmm1, 56(%rsp);"
    "movsd %xmm2, 64(%rsp);"

    // Count how many stack arguments we need to save.
    "movq $64, %rsi;"

    // Enter the loop below. The compiler might not place this block of code
    // adjacent to the loop, so perform the jump explicitly.
    "jmp _RecordReplayRedirectCall_Loop;"

    // Save stack arguments into the structure.
    "_RecordReplayRedirectCall_Loop:"
    "subq $1, %rsi;"
    "movq 640(%rsp, %rsi, 8), %rdx;"  // Ignore the rip/rbp saved on stack.
    "movq %rdx, 104(%rsp, %rsi, 8);"
    "testq %rsi, %rsi;"
    "jne _RecordReplayRedirectCall_Loop;"

    // Place the CallEvent being made into the first argument register.
    "movq %rax, %rdi;"

    // Place the structure's address into the second argument register.
    "movq %rsp, %rsi;"

    // Determine how to handle this call.
    "call _RecordReplayInterceptCall;"

    "testq %rax, %rax;"
    "je RecordReplayRedirectCall_done;"

    // Events are passed through. The function to call has been stored in rval0.
    // Call the function with the original stack and register state.
    "movq 0(%rsp), %rdi;"
    "movq 8(%rsp), %rsi;"
    "movq 16(%rsp), %rdx;"
    "movq 24(%rsp), %rcx;"
    "movq 32(%rsp), %r8;"
    "movq 40(%rsp), %r9;"
    "movsd 48(%rsp), %xmm0;"
    "movsd 56(%rsp), %xmm1;"
    "movsd 64(%rsp), %xmm2;"
    "movq 72(%rsp), %rax;"
    "addq $624, %rsp;"
    "popq %rbp;"
    "jmpq *%rax;"

    // The message has been recorded/replayed.
    "RecordReplayRedirectCall_done:"
    // Restore scalar and floating point return values.
    "movq 72(%rsp), %rax;"
    "movq 80(%rsp), %rdx;"
    "movsd 88(%rsp), %xmm0;"
    "movsd 96(%rsp), %xmm1;"

    // Pop the structure from the stack.
    "addq $624, %rsp;"

    // Return to caller.
    "popq %rbp;"
    "ret;");

// Call a function address with the specified arguments.
extern void RecordReplayInvokeCallRaw(CallArguments* aArguments, void* aFnPtr);

__asm(
    "_RecordReplayInvokeCallRaw:"

    // Save rbp for backtraces.
    "pushq %rbp;"
    "movq %rsp, %rbp;"

    // Save function pointer in rax.
    "movq %rsi, %rax;"

    // Save arguments on the stack, with a second copy for alignment.
    "push %rdi;"
    "push %rdi;"

    // Count how many stack arguments we need to copy.
    "movq $64, %rsi;"

    // Enter the loop below. The compiler might not place this block of code
    // adjacent to the loop, so perform the jump explicitly.
    "jmp _RecordReplayInvokeCallRaw_Loop;"

    // Copy each stack argument to the stack.
    "_RecordReplayInvokeCallRaw_Loop:"
    "subq $1, %rsi;"
    "movq 104(%rdi, %rsi, 8), %rdx;"
    "push %rdx;"
    "testq %rsi, %rsi;"
    "jne _RecordReplayInvokeCallRaw_Loop;"

    // Copy each register argument into the appropriate register.
    "movq 8(%rdi), %rsi;"
    "movq 16(%rdi), %rdx;"
    "movq 24(%rdi), %rcx;"
    "movq 32(%rdi), %r8;"
    "movq 40(%rdi), %r9;"
    "movsd 48(%rdi), %xmm0;"
    "movsd 56(%rdi), %xmm1;"
    "movsd 64(%rdi), %xmm2;"
    "movq 0(%rdi), %rdi;"

    // Call the saved function pointer.
    "callq *%rax;"

    // Pop the copied stack arguments.
    "addq $512, %rsp;"

    // Save any return values to the arguments.
    "pop %rdi;"
    "pop %rdi;"
    "movq %rax, 72(%rdi);"
    "movq %rdx, 80(%rdi);"
    "movsd %xmm0, 88(%rdi);"
    "movsd %xmm1, 96(%rdi);"

    "popq %rbp;"
    "ret;");

}  // extern "C"

MOZ_NEVER_INLINE void RecordReplayInvokeCall(void* aFunction,
                                             CallArguments* aArguments) {
  RecordReplayInvokeCallRaw(aArguments, aFunction);
}

///////////////////////////////////////////////////////////////////////////////
// Library API Redirections
///////////////////////////////////////////////////////////////////////////////

uint8_t* GenerateRedirectStub(Assembler& aAssembler, size_t aCallId,
                              bool aPreserveCallerSaveRegisters) {
  // Generate code to set %rax and enter RecordReplayRedirectCall.
  uint8_t* newFunction = aAssembler.Current();
  if (aPreserveCallerSaveRegisters) {
    static Register registers[] = {
      Register::RDI, Register::RDI /* for alignment */,
      Register::RSI, Register::RDX, Register::RCX,
      Register::R8, Register::R9, Register::R10, Register::R11,
    };
    for (size_t i = 0; i < ArrayLength(registers); i++) {
      aAssembler.MoveRegisterToRax(registers[i]);
      aAssembler.PushRax();
    }
    aAssembler.MoveImmediateToRax((void*)aCallId);
    uint8_t* after = aAssembler.Current() + Assembler::PushImmediateBytes + Assembler::JumpBytes;
    aAssembler.PushImmediate(after);
    aAssembler.Jump(BitwiseCast<void*>(RecordReplayRedirectCall));
    for (int i = ArrayLength(registers) - 1; i >= 0; i--) {
      aAssembler.PopRegister(registers[i]);
    }
    aAssembler.Return();
  } else {
    aAssembler.MoveImmediateToRax((void*)aCallId);
    aAssembler.Jump(BitwiseCast<void*>(RecordReplayRedirectCall));
  }
  return newFunction;
}

void* OriginalFunction(const char* aName) {
  size_t numRedirections = NumRedirections();
  for (size_t i = 0; i < numRedirections; i++) {
    const Redirection& redirection = GetRedirection(i);
    if (!strcmp(aName, redirection.mName)) {
      return redirection.mOriginalFunction;
    }
  }
  MOZ_CRASH("OriginalFunction: unknown redirection");
}

///////////////////////////////////////////////////////////////////////////////
// Utility
///////////////////////////////////////////////////////////////////////////////

Atomic<size_t, SequentiallyConsistent, Behavior::DontPreserve> gMemoryLeakBytes;

void* BindFunctionArgument(void* aFunction, void* aArgument,
                           size_t aArgumentPosition, Assembler& aAssembler) {
  void* res = aAssembler.Current();

  // On x64 the argument will be in a register, so to add an extra argument for
  // the callee we just need to fill in the appropriate register for the
  // argument position with the bound argument value.
  aAssembler.MoveImmediateToRax(aArgument);

  switch (aArgumentPosition) {
    case 1:
      aAssembler.MoveRaxToRegister(Register::RSI);
      break;
    case 2:
      aAssembler.MoveRaxToRegister(Register::RDX);
      break;
    case 3:
      aAssembler.MoveRaxToRegister(Register::RCX);
      break;
    default:
      MOZ_CRASH();
  }

  // Jump to the function that was bound.
  aAssembler.Jump(aFunction);

  return res;
}

}  // namespace recordreplay
}  // namespace mozilla
