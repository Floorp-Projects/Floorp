/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a dummy version of Chromium source file base/debug/activity_tracker.h.
// To provide a class required in base/synchronization/lock_impl_win.cc
// ScopedLockAcquireActivity. We don't use activity tracking.

#ifndef BASE_DEBUG_ACTIVITY_TRACKER_H_
#define BASE_DEBUG_ACTIVITY_TRACKER_H_

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/macros.h"

namespace base {
class PlatformThreadHandle;
class WaitableEvent;

namespace internal {
class LockImpl;
}

namespace debug {

class BASE_EXPORT GlobalActivityTracker {
 public:
  static bool IsEnabled() { return false; }
  DISALLOW_COPY_AND_ASSIGN(GlobalActivityTracker);
};

class BASE_EXPORT ScopedLockAcquireActivity
{
 public:
  ALWAYS_INLINE
  explicit ScopedLockAcquireActivity(const base::internal::LockImpl* lock) {}
  DISALLOW_COPY_AND_ASSIGN(ScopedLockAcquireActivity);
};

class BASE_EXPORT ScopedEventWaitActivity
{
 public:
  ALWAYS_INLINE
  explicit ScopedEventWaitActivity(const base::WaitableEvent* event) {}
  DISALLOW_COPY_AND_ASSIGN(ScopedEventWaitActivity);
};

class BASE_EXPORT ScopedThreadJoinActivity
{
 public:
  ALWAYS_INLINE
  explicit ScopedThreadJoinActivity(const base::PlatformThreadHandle* thread) {}
  DISALLOW_COPY_AND_ASSIGN(ScopedThreadJoinActivity);
};

}  // namespace debug
}  // namespace base

#endif  // BASE_DEBUG_ACTIVITY_TRACKER_H_
