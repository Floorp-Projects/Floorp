/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils

import android.content.ComponentName
import android.content.Context
import mozilla.components.service.fretboard.Fretboard
import mozilla.components.service.fretboard.scheduler.jobscheduler.JobSchedulerSyncScheduler
import mozilla.components.service.fretboard.scheduler.jobscheduler.SyncJob
import org.mozilla.focus.activity.MainActivity

class ExperimentsSyncService : SyncJob() {
    companion object {
        fun scheduleSync(context: Context) {
            val scheduler = JobSchedulerSyncScheduler(context)
            scheduler.schedule(MainActivity.EXPERIMENTS_JOB_ID,
                    ComponentName(context, ExperimentsSyncService::class.java))
        }
    }

    override fun getFretboard(): Fretboard {
        return app.fretboard
    }
}
