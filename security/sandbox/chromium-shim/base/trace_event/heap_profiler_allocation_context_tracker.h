/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a dummy version of Chromium source file base/trace_event/heap_profiler_allocation_context_tracker.h.
// To provide a function required in base/threading/thread_id_name_manager.cc
// SetCurrentThreadName. We don't use the heap profiler.

#ifndef BASE_TRACE_EVENT_HEAP_PROFILER_ALLOCATION_CONTEXT_TRACKER_H_
#define BASE_TRACE_EVENT_HEAP_PROFILER_ALLOCATION_CONTEXT_TRACKER_H_

namespace base {
namespace trace_event {

// The allocation context tracker keeps track of thread-local context for heap
// profiling. It includes a pseudo stack of trace events. On every allocation
// the tracker provides a snapshot of its context in the form of an
// |AllocationContext| that is to be stored together with the allocation
// details.
class BASE_EXPORT AllocationContextTracker {
 public:
  static void SetCurrentThreadName(const char* name) {}

  DISALLOW_COPY_AND_ASSIGN(AllocationContextTracker);
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_HEAP_PROFILER_ALLOCATION_CONTEXT_TRACKER_H_
