/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.coroutines

import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.asCoroutineDispatcher
import java.util.concurrent.SynchronousQueue
import java.util.concurrent.ThreadPoolExecutor
import java.util.concurrent.TimeUnit

/**
 * Shared [CoroutineDispatcher]s used by Android Components and app code - in addition to dispatchers
 * provided by `kotlinx-coroutines-android`.
 */
object Dispatchers {
    /**
     * [CoroutineDispatcher] for short-lived asynchronous tasks. This dispatcher is using a thread
     * pool that creates new threads as needed, but will reuse previously constructed threads when
     * they are available.
     *
     * Threads that have not been used for sixty seconds are terminated and removed from the cache.
     */
    @Suppress("MagicNumber")
    val Cached = ThreadPoolExecutor(
        0,
        Integer.MAX_VALUE,
        60L,
        TimeUnit.SECONDS,
        SynchronousQueue<Runnable>(),
    ).asCoroutineDispatcher()
}
