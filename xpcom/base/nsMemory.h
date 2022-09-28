/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMemory_h__
#define nsMemory_h__

#include "nsError.h"

/**
 *
 * A client that wishes to be notified of low memory situations (for
 * example, because the client maintains a large memory cache that
 * could be released when memory is tight) should register with the
 * observer service (see nsIObserverService) using the topic
 * "memory-pressure".  There are specific types of notifications
 * that can occur.  These types will be passed as the |aData|
 * parameter of the of the "memory-pressure" notification:
 *
 * "low-memory"
 * This will be passed as the extra data when the pressure
 * observer is being asked to flush for low-memory conditions.
 *
 * "low-memory-ongoing"
 * This will be passed when we continue to be in a low-memory
 * condition and we want to flush caches and do other cheap
 * forms of memory minimization, but heavy handed approaches like
 * a GC are unlikely to succeed.
 *
 * "heap-minimize"
 * This will be passed as the extra data when the pressure
 * observer is being asked to flush because of a heap minimize
 * call.
 */

// This is implemented in nsMemoryImpl.cpp.

namespace nsMemory {

/**
 * Attempts to shrink the heap.
 * @param immediate - if true, heap minimization will occur
 *   immediately if the call was made on the main thread. If
 *   false, the flush will be scheduled to happen when the app is
 *   idle.
 * @throws NS_ERROR_FAILURE if 'immediate' is set and the call
 *   was not on the application's main thread.
 */
nsresult HeapMinimize(bool aImmediate);

/**
 * This predicate can be used to determine if the platform is a "low-memory"
 * platform. Callers may use this to dynamically tune their behaviour
 * to favour reduced memory usage at the expense of performance. The value
 * returned by this function will not change over the lifetime of the process.
 */
bool IsLowMemoryPlatform();
}  // namespace nsMemory

#endif  // nsMemory_h__
