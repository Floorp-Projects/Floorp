// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_restrictions.h"

#if DCHECK_IS_ON()

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/threading/thread_local.h"

namespace base {

namespace {

LazyInstance<ThreadLocalBoolean>::Leaky
    g_io_disallowed = LAZY_INSTANCE_INITIALIZER;

LazyInstance<ThreadLocalBoolean>::Leaky
    g_singleton_disallowed = LAZY_INSTANCE_INITIALIZER;

LazyInstance<ThreadLocalBoolean>::Leaky
    g_wait_disallowed = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
bool ThreadRestrictions::SetIOAllowed(bool allowed) {
  bool previous_disallowed = g_io_disallowed.Get().Get();
  g_io_disallowed.Get().Set(!allowed);
  return !previous_disallowed;
}

// static
void ThreadRestrictions::AssertIOAllowed() {
  if (g_io_disallowed.Get().Get()) {
    NOTREACHED() <<
        "Function marked as IO-only was called from a thread that "
        "disallows IO!  If this thread really should be allowed to "
        "make IO calls, adjust the call to "
        "base::ThreadRestrictions::SetIOAllowed() in this thread's "
        "startup.";
  }
}

// static
bool ThreadRestrictions::SetSingletonAllowed(bool allowed) {
  bool previous_disallowed = g_singleton_disallowed.Get().Get();
  g_singleton_disallowed.Get().Set(!allowed);
  return !previous_disallowed;
}

// static
void ThreadRestrictions::AssertSingletonAllowed() {
  if (g_singleton_disallowed.Get().Get()) {
    NOTREACHED() << "LazyInstance/Singleton is not allowed to be used on this "
                 << "thread.  Most likely it's because this thread is not "
                 << "joinable (or the current task is running with "
                 << "TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN semantics), so "
                 << "AtExitManager may have deleted the object on shutdown, "
                 << "leading to a potential shutdown crash. If you need to use "
                 << "the object from this context, it'll have to be updated to "
                 << "use Leaky traits.";
  }
}

// static
void ThreadRestrictions::DisallowWaiting() {
  g_wait_disallowed.Get().Set(true);
}

// static
void ThreadRestrictions::AssertWaitAllowed() {
  if (g_wait_disallowed.Get().Get()) {
    NOTREACHED() << "Waiting is not allowed to be used on this thread to "
                 << "prevent jank and deadlock.";
  }
}

bool ThreadRestrictions::SetWaitAllowed(bool allowed) {
  bool previous_disallowed = g_wait_disallowed.Get().Get();
  g_wait_disallowed.Get().Set(!allowed);
  return !previous_disallowed;
}

}  // namespace base

#endif  // DCHECK_IS_ON()
