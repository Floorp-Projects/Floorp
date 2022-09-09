/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This class implements the alternative ways to invoke runBlocking with some
// monitoring by wrapping the raw methods. This lint check tells us not to use the raw
// methods so we suppress the check.
@file:Suppress("MozillaRunBlockingCheck")

package org.mozilla.fenix.perf

import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.runBlocking
import org.mozilla.fenix.ext.getAndIncrementNoOverflow
import java.util.concurrent.atomic.AtomicInteger
import kotlin.coroutines.CoroutineContext

/**
 * Counts the number of runBlocking calls made
 */
@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
object RunBlockingCounter {
    val count = AtomicInteger(0)
}

/**
 * Wrapper around `runBlocking`. RunBlocking seems to be a "fix-all" to return values to the thread
 * where the coroutine is called. The official doc explains runBlocking:  "Runs a new coroutine and
 * blocks the current thread interruptibly until its completion`. This can block our main thread
 * which could lead to significant jank. This wrapper aims to count the number of runBlocking call
 * to try to limit them as much as possible to encourage alternatives solutions whenever this function
 * might be needed.
 */
fun <T> runBlockingIncrement(
    context: CoroutineContext? = null,
    action: suspend CoroutineScope.() -> T,
): T {
    RunBlockingCounter.count.getAndIncrementNoOverflow()
    return if (context != null) {
        runBlocking(context) { action() }
    } else {
        runBlocking { action() }
    }
}
