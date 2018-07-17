/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard.scheduler.jobscheduler

import android.app.job.JobParameters
import android.app.job.JobService
import mozilla.components.service.fretboard.Fretboard
import java.util.concurrent.Executors

abstract class SyncJob : JobService() {
    private val executor = Executors.newSingleThreadExecutor()

    override fun onStartJob(params: JobParameters): Boolean {
        executor.execute {
            try {
                getFretboard().updateExperiments()
            } catch (e: InterruptedException) {
                // Cancel thread
            } finally {
                jobFinished(params, false)
            }
        }
        return true
    }

    override fun onStopJob(params: JobParameters?): Boolean {
        executor.shutdownNow()
        return true
    }

    /**
     * Used to provide the instance of Fretboard
     * the app is using
     *
     * @return current Fretboard instance
     */
    abstract fun getFretboard(): Fretboard
}
