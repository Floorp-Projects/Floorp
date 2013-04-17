/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#ifndef runnable_utils_h__
#define runnable_utils_h__

#include "nsThreadUtils.h"
#include "mozilla/RefPtr.h"

// Abstract base class for all of our templates
namespace mozilla {

class runnable_args_base : public nsRunnable {
 public:
  NS_IMETHOD Run() = 0;
  virtual bool returns_value() const { return false; }
};

// The generated file contains four major function templates
// (in variants for arbitrary numbers of arguments up to 10,
// which is why it is machine generated). The four templates
// are:
//
// WrapRunnable(o, m, ...) -- wraps a member function m of an object ptr o
// WrapRunnableRet(o, m, ..., r) -- wraps a member function m of an object ptr o
//                                  the function returns something that can
//                                  be assigned to *r
// WrapRunnableNM(f, ...) -- wraps a function f
// WrapRunnableNMRet(f, ..., r) -- wraps a function f that returns something
//                                 that can be assigned to *r
//
// All of these template functions return a Runnable* which can be passed
// to Dispatch().
#include "runnable_utils_generated.h"

// Temporary hack. Really we want to have a template which will do this
static inline nsresult RUN_ON_THREAD(nsIEventTarget *thread, nsIRunnable *runnable, uint32_t flags) {
  RefPtr<nsIRunnable> runnable_ref(runnable);
  if (thread) {
    bool on;
    nsresult rv;
    rv = thread->IsOnCurrentThread(&on);

    // If the target thread has already shut down, we don't want to assert.
    if (rv != NS_ERROR_NOT_INITIALIZED) {
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }

    NS_ENSURE_SUCCESS(rv, rv);
    if(!on) {
      return thread->Dispatch(runnable_ref, flags);
    }
  }
  return runnable_ref->Run();
}

static inline nsresult RUN_ON_THREAD(nsIEventTarget *thread, runnable_args_base *runnable, uint32_t flags) {
  // Detect attempts to return a value when in async mode, since this
  // most likely means someone is trying to assign to a heap variable
  // which is now out of scope.
  MOZ_ASSERT((!(runnable->returns_value()) || (flags != NS_DISPATCH_NORMAL)));

  return RUN_ON_THREAD(thread, static_cast<nsIRunnable *>(runnable), flags);
}

#ifdef DEBUG
#define ASSERT_ON_THREAD(t) do {                \
    if (t) {                                    \
      bool on;                                    \
      nsresult rv;                                \
      rv = t->IsOnCurrentThread(&on);             \
      MOZ_ASSERT(NS_SUCCEEDED(rv));               \
      MOZ_ASSERT(on);                             \
    }                                           \
  } while(0)
#else
#define ASSERT_ON_THREAD(t)
#endif

} /* namespace mozilla */

#endif
