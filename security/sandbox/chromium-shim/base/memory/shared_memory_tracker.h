/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a dummy version of Chromium source file base/memory/shared_memory_tracker.h.
// To provide a class required in base/memory/shared_memory_win.cc.
// The class is used for memory tracking and dumping, which we don't use and
// has significant dependencies.

#ifndef BASE_MEMORY_SHARED_MEMORY_TRACKER_H_
#define BASE_MEMORY_SHARED_MEMORY_TRACKER_H_

namespace base {

// SharedMemoryTracker tracks shared memory usage.
class BASE_EXPORT SharedMemoryTracker {
 public:
  // Returns a singleton instance.
  static SharedMemoryTracker* GetInstance()
  {
    static SharedMemoryTracker* instance = new SharedMemoryTracker;
    return instance;
  }

  void IncrementMemoryUsage(const SharedMemoryMapping& mapping) {};

  void DecrementMemoryUsage(const SharedMemoryMapping& mapping) {};

 private:
  SharedMemoryTracker() {};
  ~SharedMemoryTracker() = default;

  DISALLOW_COPY_AND_ASSIGN(SharedMemoryTracker);
};

}  // namespace base

#endif  // BASE_MEMORY_SHARED_MEMORY_TRACKER_H_
