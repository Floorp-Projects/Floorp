/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_ThreadSnapshot_h
#define mozilla_recordreplay_ThreadSnapshot_h

#include "File.h"
#include "ProcessRewind.h"
#include "Thread.h"

namespace mozilla {
namespace recordreplay {

// Thread Snapshots Overview.
//
// The functions below are used when a thread saves or restores its stack and
// register state at a checkpoint. The steps taken in saving and restoring a
// thread snapshot are as follows:
//
// 1. Before idling (non-main threads) or before reaching a checkpoint (main
//    thread), the thread calls SaveThreadState. This saves the register state
//    for the thread as well as a portion of the top of the stack, and after
//    saving the state it returns true.
//
// 2. Once all other threads are idle, the main thread saves the remainder of
//    all thread stacks. (The portion saved earlier gives threads leeway to
//    perform operations after saving their stack, mainly for entering an idle
//    state.)
//
// 3. The thread stacks are now stored on the heap. Later on, the main thread
//    may ensure that all threads are idle and then call, for all threads,
//    RestoreStackForLoadingByThread. This prepares the stacks for restoring by
//    the associated threads.
//
// 4. While still in their idle state, threads call ShouldRestoreThreadStack to
//    see if there is stack information for them to restore.
//
// 5. If ShouldRestoreThreadStack returns true, RestoreThreadStack is then
//    called to restore the stack and register state to the point where
//    SaveThreadState was originally called.
//
// 6. RestoreThreadStack does not return. Instead, control transfers to the
//    call to SaveThreadState, which returns false after being restored to.

// aStackSeparator is a pointer into the stack. Values shallower than this in
// the stack will be preserved as they are at the time of the SaveThreadState
// call, whereas deeper values will be preserved as they are at the point where
// the main thread saves the remainder of the stack.
bool SaveThreadState(size_t aId, int* aStackSeparator);

// Information saved about the state of a thread.
struct SavedThreadStack
{
  // Saved stack pointer.
  void* mStackPointer;

  // Saved stack contents, starting at |mStackPointer|.
  uint8_t* mStack;
  size_t mStackBytes;

  // Saved register state.
  jmp_buf mRegisters;

  SavedThreadStack()
  {
    PodZero(this);
  }

  void ReleaseContents() {
    if (mStackBytes) {
      DeallocateMemory(mStack, mStackBytes, MemoryKind::ThreadSnapshot);
    }
  }
};

struct SavedCheckpoint
{
  CheckpointId mCheckpoint;
  SavedThreadStack mStacks[MaxRecordedThreadId];

  explicit SavedCheckpoint(CheckpointId aCheckpoint)
    : mCheckpoint(aCheckpoint)
  {}

  void ReleaseContents() {
    for (SavedThreadStack& stack : mStacks) {
      stack.ReleaseContents();
    }
  }
};

// When all other threads are idle, the main thread may call this to save its
// own stack and the stacks of all other threads. The return value is true if
// the stacks were just saved, or false if they were just restored due to a
// rewind from a later point of execution.
bool SaveAllThreads(SavedCheckpoint& aSavedCheckpoint);

// Restore the saved stacks for a checkpoint and rewind state to that point.
// This function does not return.
void RestoreAllThreads(const SavedCheckpoint& aSavedCheckpoint);

// After rewinding to an earlier checkpoint, the main thread will call this to
// ensure that each thread has woken up and restored its own stack contents.
// The main thread does not itself write to the stacks of other threads.
void WaitForIdleThreadsToRestoreTheirStacks();

bool ShouldRestoreThreadStack(size_t aId);
void RestoreThreadStack(size_t aId);

// Initialize state for taking thread snapshots.
void InitializeThreadSnapshots(size_t aNumThreads);

} // namespace recordreplay
} // namespace mozilla

#endif // mozilla_recordreplay_ThreadSnapshot_h
