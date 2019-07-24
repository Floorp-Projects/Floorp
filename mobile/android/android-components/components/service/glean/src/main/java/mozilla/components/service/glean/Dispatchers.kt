/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.ObsoleteCoroutinesApi
import kotlinx.coroutines.launch
import kotlinx.coroutines.newSingleThreadContext
import kotlinx.coroutines.runBlocking
import mozilla.components.support.base.log.logger.Logger
import java.util.concurrent.ConcurrentLinkedQueue

@ObsoleteCoroutinesApi
internal object Dispatchers {
    class WaitableCoroutineScope(val coroutineScope: CoroutineScope) {
        // When true, jobs will be run synchronously
        internal var testingMode = false

        // When true, jobs will be queued and not ran until triggered by calling
        // flushQueuedInitialTasks()
        @Volatile
        private var queueInitialTasks = true

        // Use a [ConcurrentLinkedQueue] to take advantage of it's thread safety and no locking
        internal val taskQueue: ConcurrentLinkedQueue<() -> Unit> = ConcurrentLinkedQueue()

        private val logger = Logger("glean/Dispatchers")

        companion object {
            // This value was chosen in order to allow several tasks to be queued for execution but
            // still be conservative of memory. This queue size is important for cases where
            // setUploadEnabled(false) is not called so that we don't continue to queue tasks and
            // waste memory.
            const val MAX_QUEUE_SIZE = 100
        }

        /**
         * Launch a block of work asynchronously.
         *
         * If [queueInitialTasks] is true, then the work will be queued and executed when
         * [flushQueuedInitialTasks] is called.
         *
         * If [setTestingMode] has enabled testing mode, the work will run synchronously.  This is
         * true regardless of whether [queueInitialTasks] is true or not.
         *
         * @return [Job], or null if queued or run synchronously.
         */
        fun launch(
            block: suspend CoroutineScope.() -> Unit
        ): Job? {
            return when {
                queueInitialTasks -> {
                    addTaskToQueue(block)
                    null
                }
                else -> executeTask(block)
            }
        }

        /**
         * Helper function to ensure Glean is being used in testing mode and async
         * jobs are being run synchronously.  This should be called from every method
         * in the testing API to make sure that the results of the main API can be
         * tested as expected.
        */
        @VisibleForTesting(otherwise = VisibleForTesting.NONE)
        fun assertInTestingMode() {
            assert(
                testingMode
            ) {
                "To use the testing API, Glean must be in testing mode by calling " +
                "Glean.enableTestingMode() (for example, in a @Before method)."
            }
        }

        /**
         * Enable testing mode, which makes all of the Glean public API synchronous.
         *
         * @param enabled whether or not to enable the testing mode
         */
        @VisibleForTesting(otherwise = VisibleForTesting.NONE)
        fun setTestingMode(enabled: Boolean) {
            testingMode = enabled
        }

        /**
         * Enable queueing mode, which causes tasks to be queued until launched by calling
         * [flushQueuedInitialTasks].
         *
         * @param enabled whether or not to enable the testing mode
         */
        @VisibleForTesting(otherwise = VisibleForTesting.NONE)
        fun setTaskQueueing(enabled: Boolean) {
            queueInitialTasks = enabled
        }

        /**
         * Stops queueing tasks and processes any tasks in the queue. Since [queueInitialTasks] is
         * set to false prior to processing the queue, newly launched tasks should be executed
         * on the couroutine scope rather than added to the queue.
         */
        internal fun flushQueuedInitialTasks() {
            // Setting this to false first should cause any new tasks to just be executed (see
            // launch() above) making it safer to process the queue.
            //
            // NOTE: This has the potential for causing a task to execute out of order in certain
            // situations. If a library or thread that runs before init happens to record
            // between when the queueInitialTasks is set to false and the taskQueue finishing
            // launching, then that task could be executed out of the queued order. See the bugzilla
            // bug tracking this for more info: https://bugzilla.mozilla.org/show_bug.cgi?id=1568503
            queueInitialTasks = false
            taskQueue.forEach { task ->
                task.invoke()
            }
            taskQueue.clear()
        }

        /**
         * Helper function to add task to queue as either a synchronous or asynchronous operation,
         * depending on whether [testingMode] is true.
         */
        private fun addTaskToQueue(block: suspend CoroutineScope.() -> Unit) {
            if (taskQueue.size >= MAX_QUEUE_SIZE) {
                logger.error("Exceeded maximum queue size, discarding task")
                return
            }

            if (testingMode) {
                logger.info("Task queued for execution in test mode")
                taskQueue.add {
                    runBlocking {
                        block()
                    }
                }
            } else {
                logger.info("Task queued for execution and delayed until flushed")
                taskQueue.add {
                    coroutineScope.launch(block = block)
                }
            }
        }

        /**
         * Helper function to execute the task as either an synchronous or asynchronous operation,
         * depending on whether [testingMode] is true.
         */
        private fun executeTask(block: suspend CoroutineScope.() -> Unit): Job? {
            return when {
                testingMode -> {
                    runBlocking {
                        block()
                    }
                    null
                }
                else -> coroutineScope.launch(block = block)
            }
        }
    }

    /**
     * A coroutine scope to make it easy to dispatch API calls off the main thread.
     * This needs to be a `var` so that our tests can override this.
     */
    var API = WaitableCoroutineScope(CoroutineScope(newSingleThreadContext("GleanAPIPool")))
}
