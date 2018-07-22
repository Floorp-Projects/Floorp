/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThreadSnapshot.h"

#include "MemorySnapshot.h"
#include "SpinLock.h"
#include "Thread.h"

namespace mozilla {
namespace recordreplay {

#define QuoteString(aString) #aString
#define ExpandAndQuote(aMacro) QuoteString(aMacro)

#define THREAD_STACK_TOP_SIZE 2048

// Information about a thread's state, for use in saving or restoring checkpoints.
// The contents of this structure are in preserved memory.
struct ThreadState {
  // Whether this thread should update its state when no longer idle. This is
  // only used for non-main threads.
  size_t /* bool */ mShouldRestore;

  // Register state, as stored by setjmp and restored by longjmp. Saved when a
  // non-main thread idles or the main thread begins to save all thread states.
  // When |mShouldRestore| is set, this is the state to set it to.
  jmp_buf mRegisters; // jmp_buf is 148 bytes
  uint32_t mPadding;

  // Top of the stack, set as for |registers|. Stack pointer information is
  // actually included in |registers| as well, but jmp_buf is opaque.
  void* mStackPointer;

  // Contents of the top of the stack, set as for |registers|. This captures
  // parts of the stack that might mutate between the state being saved and the
  // thread actually idling or making a copy of its complete stack.
  uint8_t mStackTop[THREAD_STACK_TOP_SIZE];
  size_t mStackTopBytes;

  // Stack contents to copy to |stackPointer|, non-nullptr if |mShouldRestore| is set.
  uint8_t* mStackContents;

  // Length of |stackContents|.
  size_t mStackBytes;
};

// For each non-main thread, whether that thread should update its stack and
// state when it is no longer idle. This also stores restore info for the
// main thread, which immediately updates its state when restoring checkpoints.
static ThreadState* gThreadState;

void
InitializeThreadSnapshots(size_t aNumThreads)
{
  gThreadState = (ThreadState*) AllocateMemory(aNumThreads * sizeof(ThreadState),
                                               UntrackedMemoryKind::ThreadSnapshot);

  jmp_buf buf;
  if (setjmp(buf) == 0) {
    longjmp(buf, 1);
  }
  ThreadYield();
}

static void
ClearThreadState(ThreadState* aInfo)
{
  MOZ_RELEASE_ASSERT(aInfo->mShouldRestore);
  DeallocateMemory(aInfo->mStackContents, aInfo->mStackBytes, UntrackedMemoryKind::ThreadSnapshot);
  aInfo->mShouldRestore = false;
  aInfo->mStackContents = nullptr;
  aInfo->mStackBytes = 0;
}

extern "C" {

extern int
SaveThreadStateOrReturnFromRestore(ThreadState* aInfo, int (*aSetjmpArg)(jmp_buf),
                                   int* aStackSeparator);

#define THREAD_REGISTERS_OFFSET 8
#define THREAD_STACK_POINTER_OFFSET 160
#define THREAD_STACK_TOP_OFFSET 168
#define THREAD_STACK_TOP_BYTES_OFFSET 2216
#define THREAD_STACK_CONTENTS_OFFSET 2224
#define THREAD_STACK_BYTES_OFFSET 2232

__asm(
"_SaveThreadStateOrReturnFromRestore:"

  // On Unix/x64, the first integer arg is in %rdi. Move this into a
  // callee save register so that setjmp/longjmp will save/restore it even
  // though the rest of the stack is incoherent after the longjmp.
  "push %rbx;"
  "movq %rdi, %rbx;"

  // Update |aInfo->mStackPointer|. Everything above this on the stack will be
  // restored after getting here from longjmp.
  "movq %rsp, " ExpandAndQuote(THREAD_STACK_POINTER_OFFSET) "(%rbx);"

  // Compute the number of bytes to store on the stack top.
  "subq %rsp, %rdx;" // rdx is the third arg reg

  // Bounds check against the size of the stack top buffer.
  "cmpl $" ExpandAndQuote(THREAD_STACK_TOP_SIZE) ", %edx;"
  "jg SaveThreadStateOrReturnFromRestore_crash;"

  // Store the number of bytes written to the stack top buffer.
  "movq %rdx, " ExpandAndQuote(THREAD_STACK_TOP_BYTES_OFFSET) "(%rbx);"

  // Load the start of the stack top buffer and the stack pointer.
  "movq %rsp, %r8;"
  "movq %rbx, %r9;"
  "addq $" ExpandAndQuote(THREAD_STACK_TOP_OFFSET) ", %r9;"

  "jmp SaveThreadStateOrReturnFromRestore_copyTopRestart;"

  // Fill in the stack top buffer.
"SaveThreadStateOrReturnFromRestore_copyTopRestart:"
  "testq %rdx, %rdx;"
  "je SaveThreadStateOrReturnFromRestore_copyTopDone;"
  "movl 0(%r8), %ecx;"
  "movl %ecx, 0(%r9);"
  "addq $4, %r8;"
  "addq $4, %r9;"
  "subq $4, %rdx;"
  "jmp SaveThreadStateOrReturnFromRestore_copyTopRestart;"

"SaveThreadStateOrReturnFromRestore_copyTopDone:"

  // Call setjmp, passing |aInfo->mRegisters|.
  "addq $" ExpandAndQuote(THREAD_REGISTERS_OFFSET) ", %rdi;"
  "callq *%rsi;" // rsi is the second arg reg

  // If setjmp returned zero, we just saved the state and are done.
  "testl %eax, %eax;"
  "je SaveThreadStateOrReturnFromRestore_done;"

  // Otherwise we just returned from longjmp, and need to restore the stack
  // contents before anything else can be performed. Use caller save registers
  // exclusively for this, don't touch the stack at all.

  // Load |mStackPointer|, |mStackContents|, and |mStackBytes| from |aInfo|.
  "movq " ExpandAndQuote(THREAD_STACK_POINTER_OFFSET) "(%rbx), %rcx;"
  "movq " ExpandAndQuote(THREAD_STACK_CONTENTS_OFFSET) "(%rbx), %r8;"
  "movq " ExpandAndQuote(THREAD_STACK_BYTES_OFFSET) "(%rbx), %r9;"

  // The stack pointer we loaded should be identical to the stack pointer we have.
  "cmpq %rsp, %rcx;"
  "jne SaveThreadStateOrReturnFromRestore_crash;"

  "jmp SaveThreadStateOrReturnFromRestore_copyAfterRestart;"

  // Fill in the contents of the entire stack.
"SaveThreadStateOrReturnFromRestore_copyAfterRestart:"
  "testq %r9, %r9;"
  "je SaveThreadStateOrReturnFromRestore_done;"
  "movl 0(%r8), %edx;"
  "movl %edx, 0(%rcx);"
  "addq $4, %rcx;"
  "addq $4, %r8;"
  "subq $4, %r9;"
  "jmp SaveThreadStateOrReturnFromRestore_copyAfterRestart;"

"SaveThreadStateOrReturnFromRestore_crash:"
  "movq $0, %rbx;"
  "movq 0(%rbx), %rbx;"

"SaveThreadStateOrReturnFromRestore_done:"
  "pop %rbx;"
  "ret;"
);

} // extern "C"

bool
SaveThreadState(size_t aId, int* aStackSeparator)
{
  static_assert(offsetof(ThreadState, mRegisters) == THREAD_REGISTERS_OFFSET &&
                offsetof(ThreadState, mStackPointer) == THREAD_STACK_POINTER_OFFSET &&
                offsetof(ThreadState, mStackTop) == THREAD_STACK_TOP_OFFSET &&
                offsetof(ThreadState, mStackTopBytes) == THREAD_STACK_TOP_BYTES_OFFSET &&
                offsetof(ThreadState, mStackContents) == THREAD_STACK_CONTENTS_OFFSET &&
                offsetof(ThreadState, mStackBytes) == THREAD_STACK_BYTES_OFFSET,
                "Incorrect ThreadState offsets");

  ThreadState* info = &gThreadState[aId];
  MOZ_RELEASE_ASSERT(!info->mShouldRestore);
  bool res = SaveThreadStateOrReturnFromRestore(info, setjmp, aStackSeparator) == 0;
  if (!res) {
    ClearThreadState(info);
  }
  return res;
}

void
RestoreThreadStack(size_t aId)
{
  ThreadState* info = &gThreadState[aId];
  longjmp(info->mRegisters, 1);
  MOZ_CRASH(); // longjmp does not return.
}

static void
SaveThreadStack(SavedThreadStack& aStack, size_t aId)
{
  Thread* thread = Thread::GetById(aId);

  ThreadState& info = gThreadState[aId];
  aStack.mStackPointer = info.mStackPointer;
  MemoryMove(aStack.mRegisters, info.mRegisters, sizeof(jmp_buf));

  uint8_t* stackPointer = (uint8_t*) info.mStackPointer;
  uint8_t* stackTop = thread->StackBase() + thread->StackSize();
  MOZ_RELEASE_ASSERT(stackTop >= stackPointer);
  size_t stackBytes = stackTop - stackPointer;

  MOZ_RELEASE_ASSERT(stackBytes >= info.mStackTopBytes);

  aStack.mStack = (uint8_t*) AllocateMemory(stackBytes, UntrackedMemoryKind::ThreadSnapshot);
  aStack.mStackBytes = stackBytes;

  MemoryMove(aStack.mStack, info.mStackTop, info.mStackTopBytes);
  MemoryMove(aStack.mStack + info.mStackTopBytes,
             stackPointer + info.mStackTopBytes, stackBytes - info.mStackTopBytes);
}

static void
RestoreStackForLoadingByThread(const SavedThreadStack& aStack, size_t aId)
{
  ThreadState& info = gThreadState[aId];
  MOZ_RELEASE_ASSERT(!info.mShouldRestore);

  info.mStackPointer = aStack.mStackPointer;
  MemoryMove(info.mRegisters, aStack.mRegisters, sizeof(jmp_buf));

  info.mStackBytes = aStack.mStackBytes;

  uint8_t* stackContents =
    (uint8_t*) AllocateMemory(info.mStackBytes, UntrackedMemoryKind::ThreadSnapshot);
  MemoryMove(stackContents, aStack.mStack, aStack.mStackBytes);
  info.mStackContents = stackContents;
  info.mShouldRestore = true;
}

bool
ShouldRestoreThreadStack(size_t aId)
{
  return gThreadState[aId].mShouldRestore;
}

bool
SaveAllThreads(SavedCheckpoint& aSaved)
{
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());

  AutoPassThroughThreadEvents pt; // setjmp may perform system calls.
  AutoDisallowMemoryChanges disallow;

  int stackSeparator = 0;
  if (!SaveThreadState(MainThreadId, &stackSeparator)) {
    // We just restored this state from a later point of execution.
    return false;
  }

  for (size_t i = MainThreadId; i <= MaxRecordedThreadId; i++) {
    SaveThreadStack(aSaved.mStacks[i - 1], i);
  }
  return true;
}

void
RestoreAllThreads(const SavedCheckpoint& aSaved)
{
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());

  // These will be matched by the Auto* classes in SaveAllThreads().
  BeginPassThroughThreadEvents();
  SetMemoryChangesAllowed(false);

  for (size_t i = MainThreadId; i <= MaxRecordedThreadId; i++) {
    RestoreStackForLoadingByThread(aSaved.mStacks[i - 1], i);
  }

  // Restore this stack to its state when we saved it in SaveAllThreads(), and
  // continue executing from there.
  RestoreThreadStack(MainThreadId);
  Unreachable();
}

void
WaitForIdleThreadsToRestoreTheirStacks()
{
  // Wait for all other threads to restore their stack before resuming execution.
  while (true) {
    bool done = true;
    for (size_t i = MainThreadId + 1; i <= MaxRecordedThreadId; i++) {
      if (ShouldRestoreThreadStack(i)) {
        Thread::Notify(i);
        done = false;
      }
    }
    if (done) {
      break;
    }
    Thread::WaitNoIdle();
  }
}

} // namespace recordreplay
} // namespace mozilla
