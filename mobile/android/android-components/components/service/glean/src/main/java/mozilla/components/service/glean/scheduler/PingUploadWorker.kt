/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.scheduler

import android.content.Context
import androidx.work.Worker
import androidx.work.WorkerParameters
import mozilla.components.service.glean.Glean
import mozilla.components.service.glean.net.HttpPingUploader

/**
 * This class is the worker class used by [WorkManager] to handle uploading the ping to the server.
 */
class PingUploadWorker(context: Context, params: WorkerParameters) : Worker(context, params) {
    companion object {
        internal const val PING_WORKER_TAG = "glean_ping_worker"
        internal const val DEFAULT_BACKOFF_FOR_UPLOAD = 30L
    }

    /**
     * This method is called on a background thread - you are required to **synchronously** do your
     * work and return the [androidx.work.ListenableWorker.Result] from this method.  Once you
     * return from this method, the Worker is considered to have finished what its doing and will be
     * destroyed.
     *
     * A Worker is given a maximum of ten minutes to finish its execution and return a
     * [androidx.work.ListenableWorker.Result].  After this time has expired, the Worker will
     * be signalled to stop.
     *
     * @return The [androidx.work.ListenableWorker.Result] of the computation
     */
    override fun doWork(): Result {
        val httpPingUploader = HttpPingUploader()

        if (!Glean.pingScheduler.pingStorageEngine.process(httpPingUploader::upload)) {
            return Result.retry()
        }

        return Result.success()
    }
}
