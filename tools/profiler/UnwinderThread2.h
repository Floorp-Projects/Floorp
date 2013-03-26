/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_UNWINDER_THREAD_2_H
#define MOZ_UNWINDER_THREAD_2_H

#include "GeckoProfilerImpl.h"
#include "ProfileEntry.h"

/* Top level exports of UnwinderThread.cpp. */

// Abstract type.  A buffer which is used to transfer information between
// the sampled thread(s) and the unwinder thread(s).
typedef
  struct _UnwinderThreadBuffer 
  UnwinderThreadBuffer;

// RUNS IN SIGHANDLER CONTEXT
// Called in the sampled thread (signal) context.  Adds a ProfileEntry
// into an UnwinderThreadBuffer that the thread has previously obtained
// by a call to utb__acquire_empty_buffer.
void utb__addEntry(/*MOD*/UnwinderThreadBuffer* utb,
                   ProfileEntry ent);

// Create the unwinder thread.  At the moment there can be only one.
void uwt__init();

// Request the unwinder thread to exit, and wait until it has done so.
void uwt__deinit();

// Registers a sampler thread for profiling.  Threads must be registered
// before they are allowed to call utb__acquire_empty_buffer or
// utb__release_full_buffer.
void uwt__register_thread_for_profiling(void* stackTop);

// RUNS IN SIGHANDLER CONTEXT
// Called in the sampled thread (signal) context.  Get an empty buffer
// into which ProfileEntries can be put.  It may return NULL if no
// empty buffers can be found, which will be the case if the unwinder
// thread(s) have fallen behind for some reason.  In this case the
// sampled thread must simply give up and return from the signal handler
// immediately, else it risks deadlock.
UnwinderThreadBuffer* uwt__acquire_empty_buffer();

// RUNS IN SIGHANDLER CONTEXT
// Called in the sampled thread (signal) context.  Release a buffer
// that the sampled thread has acquired, handing the contents to
// the unwinder thread, and, if necessary, passing sufficient
// information (stack top chunk, + registers) to also do a native
// unwind.  If 'ucV' is NULL, no native unwind is done.  If non-NULL,
// it is assumed to point to a ucontext_t* that holds the initial 
// register state for the unwind.  The results of all of this are
// dumped into |aProfile| (by the unwinder thread, not the calling thread).
void uwt__release_full_buffer(ThreadProfile* aProfile,
                              UnwinderThreadBuffer* utb,
                              void* /* ucontext_t*, really */ ucV);

#endif /* ndef MOZ_UNWINDER_THREAD_2_H */
