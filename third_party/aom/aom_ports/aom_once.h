/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_PORTS_AOM_ONCE_H_
#define AOM_PORTS_AOM_ONCE_H_

#include "config/aom_config.h"

/* Implement a function wrapper to guarantee initialization
 * thread-safety for library singletons.
 *
 * NOTE: These functions use static locks, and can only be
 * used with one common argument per compilation unit. So
 *
 * file1.c:
 *   aom_once(foo);
 *   ...
 *   aom_once(foo);
 *
 *   file2.c:
 *     aom_once(bar);
 *
 * will ensure foo() and bar() are each called only once, but in
 *
 * file1.c:
 *   aom_once(foo);
 *   aom_once(bar):
 *
 * bar() will never be called because the lock is used up
 * by the call to foo().
 */

#if CONFIG_MULTITHREAD && defined(_WIN32)
#include <windows.h>
#include <stdlib.h>
/* Declare a per-compilation-unit state variable to track the progress
 * of calling func() only once. This must be at global scope because
 * local initializers are not thread-safe in MSVC prior to Visual
 * Studio 2015.
 *
 * As a static, once_state will be zero-initialized as program start.
 */
static LONG once_state;
static void once(void (*func)(void)) {
  /* Try to advance once_state from its initial value of 0 to 1.
   * Only one thread can succeed in doing so.
   */
  if (InterlockedCompareExchange(&once_state, 1, 0) == 0) {
    /* We're the winning thread, having set once_state to 1.
     * Call our function. */
    func();
    /* Now advance once_state to 2, unblocking any other threads. */
    InterlockedIncrement(&once_state);
    return;
  }

  /* We weren't the winning thread, but we want to block on
   * the state variable so we don't return before func()
   * has finished executing elsewhere.
   *
   * Try to advance once_state from 2 to 2, which is only possible
   * after the winning thead advances it from 1 to 2.
   */
  while (InterlockedCompareExchange(&once_state, 2, 2) != 2) {
    /* State isn't yet 2. Try again.
     *
     * We are used for singleton initialization functions,
     * which should complete quickly. Contention will likewise
     * be rare, so it's worthwhile to use a simple but cpu-
     * intensive busy-wait instead of successive backoff,
     * waiting on a kernel object, or another heavier-weight scheme.
     *
     * We can at least yield our timeslice.
     */
    Sleep(0);
  }

  /* We've seen once_state advance to 2, so we know func()
   * has been called. And we've left once_state as we found it,
   * so other threads will have the same experience.
   *
   * It's safe to return now.
   */
  return;
}

#elif CONFIG_MULTITHREAD && defined(__OS2__)
#define INCL_DOS
#include <os2.h>
static void once(void (*func)(void)) {
  static int done;

  /* If the initialization is complete, return early. */
  if (done) return;

  /* Causes all other threads in the process to block themselves
   * and give up their time slice.
   */
  DosEnterCritSec();

  if (!done) {
    func();
    done = 1;
  }

  /* Restores normal thread dispatching for the current process. */
  DosExitCritSec();
}

#elif CONFIG_MULTITHREAD && HAVE_PTHREAD_H
#include <pthread.h>
static void once(void (*func)(void)) {
  static pthread_once_t lock = PTHREAD_ONCE_INIT;
  pthread_once(&lock, func);
}

#else
/* No-op version that performs no synchronization. *_rtcd() is idempotent,
 * so as long as your platform provides atomic loads/stores of pointers
 * no synchronization is strictly necessary.
 */

static void once(void (*func)(void)) {
  static int done;

  if (!done) {
    func();
    done = 1;
  }
}
#endif

#endif  // AOM_PORTS_AOM_ONCE_H_
