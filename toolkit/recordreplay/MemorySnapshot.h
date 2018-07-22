/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_MemorySnapshot_h
#define mozilla_recordreplay_MemorySnapshot_h

#include "mozilla/Types.h"
#include "ProcessRecordReplay.h"

namespace mozilla {
namespace recordreplay {

// Memory Snapshots Overview.
//
// As described in ProcessRewind.h, some subset of the checkpoints which are
// reached during execution are saved, so that their state can be restored
// later. Memory snapshots are used to save and restore the contents of all
// heap memory: everything except thread stacks (see ThreadSnapshot.h for
// saving and restoring these) and untracked memory (which is not saved or
// restored, see ProcessRecordReplay.h).
//
// Each memory snapshot is a diff of the heap memory contents compared to the
// next one. See MemorySnapshot.cpp for how diffs are represented and computed.
//
// Rewinding must restore the exact contents of heap memory that existed when
// the target checkpoint was reached. Because of this, memory that is allocated
// at a point when a checkpoint is saved will never actually be returned to the
// system. We instead keep a set of free blocks that are unused at the current
// point of execution and are available to satisfy new allocations.

// Make sure that a block of memory in a fixed allocation is already allocated.
void CheckFixedMemory(void* aAddress, size_t aSize);

// After marking a block of memory in a fixed allocation as non-writable,
// restore writability to any dirty pages in the range.
void RestoreWritableFixedMemory(void* aAddress, size_t aSize);

// Allocate memory, trying to use a specific address if provided but only if
// it is free.
void* AllocateMemoryTryAddress(void* aAddress, size_t aSize, AllocatedMemoryKind aKind);

// Note a range of memory that was just allocated from the system, and the
// kind of memory allocation that was performed.
void RegisterAllocatedMemory(void* aBaseAddress, size_t aSize, AllocatedMemoryKind aKind);

// Initialize the memory snapshots system.
void InitializeMemorySnapshots();

// Take the first heap memory snapshot.
void TakeFirstMemorySnapshot();

// Take a differential heap memory snapshot compared to the last one,
// associated with the last saved checkpoint.
void TakeDiffMemorySnapshot();

// Restore all heap memory to its state when the most recent checkpoint was
// saved. This requires no checkpoints to have been saved after this one.
void RestoreMemoryToLastSavedCheckpoint();

// Restore all heap memory to its state at a checkpoint where a complete diff
// was saved vs. the following saved checkpoint. This requires that no
// tracked heap memory has been changed since the last saved checkpoint.
void RestoreMemoryToLastSavedDiffCheckpoint();

// Erase all information from the last diff snapshot taken, so that tracked
// heap memory changes are with respect to the previous checkpoint.
void EraseLastSavedDiffMemorySnapshot();

// Set whether to allow changes to tracked heap memory at this point. If such
// changes occur when they are not allowed then the process will crash.
void SetMemoryChangesAllowed(bool aAllowed);

struct MOZ_RAII AutoDisallowMemoryChanges
{
  AutoDisallowMemoryChanges() { SetMemoryChangesAllowed(false); }
  ~AutoDisallowMemoryChanges() { SetMemoryChangesAllowed(true); }
};

// After a SEGV on the specified address, check if the violation occurred due
// to the memory having been write protected by the snapshot mechanism. This
// function returns whether the fault has been handled and execution may
// continue.
bool HandleDirtyMemoryFault(uint8_t* aAddress);

// For debugging, note a point where we hit an unrecoverable failure and try
// to make things easier for the debugger.
void UnrecoverableSnapshotFailure();

// After rewinding, mark all memory that has been allocated since the snapshot
// was taken as free.
void FixupFreeRegionsAfterRewind();

// Set whether to allow intentionally crashing in this process via the
// RecordReplayDirective method.
void SetAllowIntentionalCrashes(bool aAllowed);

// When WANT_COUNTDOWN_THREAD is defined (see MemorySnapshot.cpp), set a count
// that, after a thread consumes it, causes the thread to report a fatal error.
// This is used for debugging and is a workaround for lldb often being unable
// to interrupt a running process.
void StartCountdown(size_t aCount);

// Per StartCountdown, set a countdown and remove it on destruction.
struct MOZ_RAII AutoCountdown
{
  explicit AutoCountdown(size_t aCount);
  ~AutoCountdown();
};

// Initialize the thread consuming the countdown.
void InitializeCountdownThread();

// This is an alternative to memmove/memcpy that can be called in areas where
// faults in write protected memory are not allowed. It's hard to avoid dynamic
// code loading when calling memmove/memcpy directly.
void MemoryMove(void* aDst, const void* aSrc, size_t aSize);

// Similarly, zero out a range of memory without doing anything weird with
// dynamic code loading.
void MemoryZero(void* aDst, size_t aSize);

} // namespace recordreplay
} // namespace mozilla

#endif // mozilla_recordreplay_MemorySnapshot_h
