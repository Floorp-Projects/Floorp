/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.app.Service
import android.content.Intent
import androidx.annotation.CallSuper
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PROTECTED
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch

/**
 * Service that runs suspend functions in parallel.
 * When all jobs are completed, the service is stopped automatically.
 */
abstract class CoroutineService(
    jobDispatcher: CoroutineDispatcher = Dispatchers.IO
) : Service() {

    private val scope = CoroutineScope(jobDispatcher)
    private val runningJobs = mutableSetOf<Job>()

    /**
     * Called by every time a client explicitly starts the service by calling
     * [android.content.Context.startService], providing the arguments it supplied.
     * Do not call this method directly.
     *
     * @param intent The Intent supplied to [android.content.Context.startService], as given.
     * This may be null if the service is being restarted after its process has gone away.
     * @param flags Additional data about this start request.
     */
    @VisibleForTesting(otherwise = PROTECTED)
    internal abstract suspend fun onStartCommand(intent: Intent?, flags: Int)

    /**
     * Starts a job using [onStartCommand] then stops the service once all jobs are complete.
     */
    final override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val job = scope.launch { onStartCommand(intent, flags) }
        synchronized(runningJobs) {
            runningJobs.add(job)
        }
        job.invokeOnCompletion { cleanupJob(job) }
        return START_REDELIVER_INTENT
    }

    /**
     * Stops all jobs when the service is destroyed.
     */
    @CallSuper
    override fun onDestroy() {
        scope.cancel()
    }

    private fun cleanupJob(job: Job) = synchronized(runningJobs) {
        runningJobs.remove(job)
        if (runningJobs.isEmpty()) {
            stopSelf()
        }
    }
}
