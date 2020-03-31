/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import java.util.concurrent.CopyOnWriteArrayList
import java.util.concurrent.atomic.AtomicBoolean

/**
 * A queue that acts as a gate, either executing tasks right away if the queue is marked as "ready",
 * i.e. gate is open, or queues them to be executed whenever the queue is marked as ready in the
 * future, i.e. gate becomes open.
 */
class RunWhenReadyQueue {
    private val tasks = CopyOnWriteArrayList<() -> Unit>()
    private val isReady = AtomicBoolean(false)

    /**
     * Was this queue ever marked as 'ready' via a call to [ready]?
     *
     * @return Boolean value indicating if this queue is 'ready'.
     */
    fun isReady(): Boolean = isReady.get()

    /**
     * Runs the [task] if this queue is marked as ready, or queues it for later execution.
     * Task will be executed on the main thread.
     *
     * @param task: The task to run now if queue is ready or queue for later execution.
     */
    fun runIfReadyOrQueue(task: () -> Unit) {
        if (isReady.get()) {
            CoroutineScope(Dispatchers.Main).launch { task.invoke() }
        } else {
            tasks.add(task)
        }
    }

    /**
     * Mark queue as ready. Pending tasks will execute, and all tasks passed to [runIfReadyOrQueue]
     * after this point will be executed immediately.
     */
    fun ready() {
        // Make sure that calls to `ready` are idempotent.
        if (!isReady.compareAndSet(false, true)) {
            return
        }

        CoroutineScope(Dispatchers.Main).launch {
            tasks.forEach { it.invoke() }.also { tasks.clear() }
        }
    }
}
