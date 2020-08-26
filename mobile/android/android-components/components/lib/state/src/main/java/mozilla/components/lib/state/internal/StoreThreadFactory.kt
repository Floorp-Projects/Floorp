/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.state.internal

import java.util.concurrent.Executors
import java.util.concurrent.ThreadFactory

/**
 * Custom [ThreadFactory] implementation wrapping [Executors.defaultThreadFactory] that allows
 * asserting whether a caller is on the created thread.
 *
 * For usage with [Executors.newSingleThreadExecutor]: Only the last created thread is kept and
 * compared when [assertOnThread] is called.
 */
internal class StoreThreadFactory : ThreadFactory {
    @Volatile
    private var thread: Thread? = null

    private val actualFactory = Executors.defaultThreadFactory()

    override fun newThread(r: Runnable): Thread {
        return actualFactory.newThread(r).also {
            thread = it
        }
    }

    /**
     * Asserts that the calling thread is the thread of this [StoreDispatcher]. Otherwise throws an
     * [IllegalThreadStateException].
     */
    fun assertOnThread() {
        val currentThread = Thread.currentThread()
        val currentThreadId = currentThread.id
        val expectedThreadId = thread?.id

        if (currentThreadId == expectedThreadId) {
            return
        }

        throw IllegalThreadStateException(
            "Expected `store` thread, but running on thread `${currentThread.name}`. Leaked MiddlewareContext?"
        )
    }
}
