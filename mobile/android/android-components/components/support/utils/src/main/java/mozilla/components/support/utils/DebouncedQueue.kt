/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

/**
 * A queue that manages function execution with a debounce mechanism. Only the most recent function
 * submitted within a set interval is executed, helping to prevent excessive or redundant operations.
 *
 * @param delayMillis The time delay in milliseconds before the last submitted function is executed.
 * @param scope The coroutine scope in which to launch the tasks.
 */
class DebouncedQueue(
    private val scope: CoroutineScope = MainScope(),
    private val delayMillis: Long = 0L,
) {
    private var debounceJob: Job? = null

    /**
     * Enqueues a function to be executed. If another function is enqueued during the delay period,
     * the previous function is cancelled and only the most recent one will be executed after the delay.
     *
     * @param function The function to execute after the delay period if no other functions are submitted.
     */
    fun enqueue(function: () -> Unit) {
        debounceJob?.cancel()

        debounceJob = scope.launch {
            delay(delayMillis)
            function()
        }
    }
}
