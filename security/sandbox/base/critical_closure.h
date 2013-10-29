// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CRITICAL_CLOSURE_H_
#define BASE_CRITICAL_CLOSURE_H_

#include "base/callback.h"

namespace base {

// Returns a closure that will continue to run for a period of time when the
// application goes to the background if possible on platforms where
// applications don't execute while backgrounded, otherwise the original task is
// returned.
//
// Example:
//   file_message_loop_proxy_->PostTask(
//       FROM_HERE,
//       MakeCriticalClosure(base::Bind(&WriteToDiskTask, path_, data)));
//
// Note new closures might be posted in this closure. If the new closures need
// background running time, |MakeCriticalClosure| should be applied on them
// before posting.
#if defined(OS_IOS)
base::Closure MakeCriticalClosure(const base::Closure& closure);
#else
inline base::Closure MakeCriticalClosure(const base::Closure& closure) {
  // No-op for platforms where the application does not need to acquire
  // background time for closures to finish when it goes into the background.
  return closure;
}
#endif  // !defined(OS_IOS)

}  // namespace base

#endif  // BASE_CRITICAL_CLOSURE_H_
