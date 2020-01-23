/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import org.mozilla.gecko.util.ThreadUtils
import java.lang.IllegalStateException

typealias StartupTask = () -> Unit

/**
 * A queue of tasks that are performed at specific points during Fenix startup.
 *
 * This queue contains a list of startup tasks. Each task in the queue will be started once Fenix
 * is visually complete.
 *
 * This class is not thread safe and should only be called from the main thread.
 */
class StartupTaskManager {
    private var tasks = mutableListOf<StartupTask>()
    private var hasStarted = false
        private set

    /**
     * Add a task to the queue.
     * Each task will execute on the main thread.
     *
     * @param task: The task to add to the queue.
     */
    @Synchronized
    fun add(task: StartupTask) {
        ThreadUtils.assertOnUiThread()
        if (hasStarted) {
            throw IllegalStateException("New tasks should not be added because queue already " +
                    "started, and these newly added tasks will not execute.")
        }

        tasks.add(task)
    }

    /**
     * Start all tasks in the queue. When all the tasks have been started,
     * clear the queue.
     */
    fun start() {
        ThreadUtils.assertOnUiThread()
        hasStarted = true

        tasks.forEach { it.invoke() }

        // Anything captured by the lambda will remain captured if we hold on to these tasks,
        // which takes up more memory than we need to.
        tasks.clear()
    }
}
