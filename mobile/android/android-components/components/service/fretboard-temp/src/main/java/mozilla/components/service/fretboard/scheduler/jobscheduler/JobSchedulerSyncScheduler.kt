/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard.scheduler.jobscheduler

import android.app.job.JobInfo
import android.app.job.JobScheduler
import android.content.ComponentName
import android.content.Context
import java.util.concurrent.TimeUnit

/**
 * Class used to schedule sync of experiment
 * configuration from the server
 *
 * @param context context
 */
class JobSchedulerSyncScheduler(context: Context) {
    private val jobScheduler = context.getSystemService(Context.JOB_SCHEDULER_SERVICE) as JobScheduler

    /**
     * Schedule sync with the constrains specified
     *
     * @param jobInfo object with the job constraints
     */
    fun schedule(jobInfo: JobInfo) {
        jobScheduler.schedule(jobInfo)
    }

    /**
     * Schedule sync with the default constraints
     * (once a day)
     *
     * @param jobId unique identifier of the job
     * @param serviceName object with the service to run
     */
    fun schedule(jobId: Int, serviceName: ComponentName) {
        val jobInfo = JobInfo.Builder(jobId, serviceName)
            .setPeriodic(TimeUnit.DAYS.toMillis(1))
            .setRequiredNetworkType(JobInfo.NETWORK_TYPE_ANY)
            .build()
        jobScheduler.schedule(jobInfo)
    }
}
