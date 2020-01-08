/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_ThreadSnapshot_h
#define mozilla_recordreplay_ThreadSnapshot_h

#include "ProcessRewind.h"
#include "Thread.h"

namespace mozilla {
namespace recordreplay {

// Save the current thread's registers and the top of the stack, such that the
// state can be restored later. Returns true if the state was just saved, and
// false if the state was just restored. aStackSeparator is a pointer into the
// stack. Values shallower than this in the stack will be preserved as they are
// at the time of the SaveThreadState call, whereas deeper values will be
// preserved as they are at the point where the main thread saves the remainder
// of the stack.
bool SaveThreadState(size_t aId, int* aStackSeparator);

// Remember the entire stack contents for a thread, after forking.
void SaveThreadStack(size_t aId);

// Restore the saved stack contents after a fork.
void RestoreThreadStack(size_t aId);

// Initialize state for taking thread snapshots.
void InitializeThreadSnapshots();

}  // namespace recordreplay
}  // namespace mozilla

#endif  // mozilla_recordreplay_ThreadSnapshot_h
