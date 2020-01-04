/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThreadSnapshot.h"

#include "Thread.h"

namespace mozilla {
namespace recordreplay {

#define QuoteString(aString) #aString
#define ExpandAndQuote(aMacro) QuoteString(aMacro)

#define THREAD_STACK_TOP_SIZE 2048

// Information about a thread's state, for use in saving or restoring its stack
// and register state. This is similar to setjmp/longjmp, except that stack
// contents are restored after jumping. We don't use setjmp/longjmp to avoid
// problems when the saving thread is different from the restoring thread.
struct ThreadState {
  // Contents of callee-save registers: rbx, rbp, and r12-r15
  uintptr_t mCalleeSaveRegisters[6];

  // Contents of rsp.
  uintptr_t mStackPointer;

  // Contents of the top of the stack, set as for |registers|. This captures
  // parts of the stack that might mutate between the state being saved and the
  // thread actually idling or making a copy of its complete stack.
  uint8_t mStackTop[THREAD_STACK_TOP_SIZE];
  size_t mStackTopBytes;

  // Stack contents to copy to |mStackPointer|.
  // is set.
  uint8_t* mStackContents;

  // Length of |stackContents|.
  size_t mStackBytes;
};

// For each non-main thread, whether that thread should update its stack and
// state when it is no longer idle. This also stores restore info for the
// main thread, which immediately updates its state when restoring snapshots.
static ThreadState* gThreadState;

void InitializeThreadSnapshots() {
  size_t numThreads = MaxThreadId + 1;
  gThreadState = new ThreadState[numThreads];
  memset(gThreadState, 0, numThreads * sizeof(ThreadState));
}

static void ClearThreadState(ThreadState* aInfo) {
  free(aInfo->mStackContents);
  aInfo->mStackContents = nullptr;
  aInfo->mStackBytes = 0;
}

extern "C" {

#define THREAD_STACK_POINTER_OFFSET 48
#define THREAD_STACK_TOP_OFFSET 56
#define THREAD_STACK_TOP_BYTES_OFFSET 2104
#define THREAD_STACK_CONTENTS_OFFSET 2112
#define THREAD_STACK_BYTES_OFFSET 2120

extern int SaveThreadStateOrReturnFromRestore(ThreadState* aInfo,
                                              int* aStackSeparator);

__asm(
"_SaveThreadStateOrReturnFromRestore:"

  // Update |aInfo->mStackPointer|. Everything above this on the stack will be
  // restored later.
  "movq %rsp, " ExpandAndQuote(THREAD_STACK_POINTER_OFFSET) "(%rdi);"

  // Compute the number of bytes to store on the stack top.
  "subq %rsp, %rsi;" // rsi is the second arg reg

  // Bounds check against the size of the stack top buffer.
  "cmpl $" ExpandAndQuote(THREAD_STACK_TOP_SIZE) ", %esi;"
  "jg SaveThreadStateOrReturnFromRestore_crash;"

  // Store the number of bytes written to the stack top buffer.
  "movq %rsi, " ExpandAndQuote(THREAD_STACK_TOP_BYTES_OFFSET) "(%rdi);"

  // Load the start of the stack top buffer and the stack pointer.
  "movq %rsp, %r8;"
  "movq %rdi, %r9;"
  "addq $" ExpandAndQuote(THREAD_STACK_TOP_OFFSET) ", %r9;"

  // Fill in the stack top buffer.
  "jmp SaveThreadStateOrReturnFromRestore_copyTopRestart;"
"SaveThreadStateOrReturnFromRestore_copyTopRestart:"
  "testq %rsi, %rsi;"
  "je SaveThreadStateOrReturnFromRestore_copyTopDone;"
  "movl 0(%r8), %ecx;"
  "movl %ecx, 0(%r9);"
  "addq $4, %r8;"
  "addq $4, %r9;"
  "subq $4, %rsi;"
  "jmp SaveThreadStateOrReturnFromRestore_copyTopRestart;"
"SaveThreadStateOrReturnFromRestore_copyTopDone:"

  // Save callee save registers.
  "movq %rbx, 0(%rdi);"
  "movq %rbp, 8(%rdi);"
  "movq %r12, 16(%rdi);"
  "movq %r13, 24(%rdi);"
  "movq %r14, 32(%rdi);"
  "movq %r15, 40(%rdi);"

  // Return zero when saving.
  "movq $0, %rax;"
  "ret;"

"SaveThreadStateOrReturnFromRestore_crash:"
  "movq $0, %rbx;"
  "movq 0(%rbx), %rbx;"
);

extern void RestoreThreadState(ThreadState* aInfo);

__asm(
"_RestoreThreadState:"

  // Restore callee save registers.
  "movq 0(%rdi), %rbx;"
  "movq 8(%rdi), %rbp;"
  "movq 16(%rdi), %r12;"
  "movq 24(%rdi), %r13;"
  "movq 32(%rdi), %r14;"
  "movq 40(%rdi), %r15;"

  // Restore stack pointer.
  "movq " ExpandAndQuote(THREAD_STACK_POINTER_OFFSET) "(%rdi), %rsp;"

  // Load |mStackPointer|, |mStackContents|, and |mStackBytes| from |aInfo|.
  "movq %rsp, %rcx;"
  "movq " ExpandAndQuote(THREAD_STACK_CONTENTS_OFFSET) "(%rdi), %r8;"
  "movq " ExpandAndQuote(THREAD_STACK_BYTES_OFFSET) "(%rdi), %r9;"

  // Fill in the contents of the entire stack.
  "jmp RestoreThreadState_copyAfterRestart;"
"RestoreThreadState_copyAfterRestart:"
  "testq %r9, %r9;"
  "je RestoreThreadState_done;"
  "movl 0(%r8), %edx;"
  "movl %edx, 0(%rcx);"
  "addq $4, %rcx;"
  "addq $4, %r8;"
  "subq $4, %r9;"
  "jmp RestoreThreadState_copyAfterRestart;"
"RestoreThreadState_done:"

  // Return non-zero when restoring.
  "movq $1, %rax;"
  "ret;"
);

}  // extern "C"

bool SaveThreadState(size_t aId, int* aStackSeparator) {
  static_assert(
      offsetof(ThreadState, mStackPointer) == THREAD_STACK_POINTER_OFFSET &&
          offsetof(ThreadState, mStackTop) == THREAD_STACK_TOP_OFFSET &&
          offsetof(ThreadState, mStackTopBytes) ==
              THREAD_STACK_TOP_BYTES_OFFSET &&
          offsetof(ThreadState, mStackContents) ==
              THREAD_STACK_CONTENTS_OFFSET &&
          offsetof(ThreadState, mStackBytes) == THREAD_STACK_BYTES_OFFSET,
      "Incorrect ThreadState offsets");

  MOZ_RELEASE_ASSERT(aId <= MaxThreadId);
  ThreadState* info = &gThreadState[aId];
  bool res =
      SaveThreadStateOrReturnFromRestore(info, aStackSeparator) == 0;
  if (!res) {
    ClearThreadState(info);
  }
  return res;
}

void SaveThreadStack(size_t aId) {
  Thread* thread = Thread::GetById(aId);

  MOZ_RELEASE_ASSERT(aId <= MaxThreadId);
  ThreadState& info = gThreadState[aId];

  uint8_t* stackPointer = (uint8_t*)info.mStackPointer;
  uint8_t* stackTop = thread->StackBase() + thread->StackSize();
  MOZ_RELEASE_ASSERT(stackTop >= stackPointer);
  size_t stackBytes = stackTop - stackPointer;

  MOZ_RELEASE_ASSERT(stackBytes >= info.mStackTopBytes);

  // Release any existing stack contents from a previous fork.
  free(info.mStackContents);

  info.mStackContents = (uint8_t*) malloc(stackBytes);
  info.mStackBytes = stackBytes;

  memcpy(info.mStackContents, info.mStackTop, info.mStackTopBytes);
  memcpy(info.mStackContents + info.mStackTopBytes,
         stackPointer + info.mStackTopBytes,
         stackBytes - info.mStackTopBytes);
}

void RestoreThreadStack(size_t aId) {
  MOZ_RELEASE_ASSERT(aId <= MaxThreadId);

  ThreadState* info = &gThreadState[aId];
  MOZ_RELEASE_ASSERT(info->mStackContents);

  RestoreThreadState(info);
  Unreachable();
}

}  // namespace recordreplay
}  // namespace mozilla
