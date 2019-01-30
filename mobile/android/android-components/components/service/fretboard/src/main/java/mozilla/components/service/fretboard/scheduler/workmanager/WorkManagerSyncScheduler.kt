/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard.scheduler.workmanager

import androidx.work.Constraints
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.NetworkType
import androidx.work.PeriodicWorkRequest
import androidx.work.WorkManager
import java.util.concurrent.TimeUnit

/**
 * Class used to schedule sync of experiment
 * configuration from the server using WorkManager
 */
class WorkManagerSyncScheduler {
    /**
     * Schedule sync with the default constraints
     * (once a day and charging)
     *
     * @param worker worker class
     */
    fun schedule(
        worker: Class<out SyncWorker>,
        interval: Pair<Long, TimeUnit> = Pair(1, TimeUnit.DAYS)
    ) {
        val constraints = Constraints.Builder()
            .setRequiredNetworkType(NetworkType.CONNECTED)
            .build()
        val syncWork = PeriodicWorkRequest.Builder(worker, interval.first, interval.second)
            .addTag(TAG)
            .setConstraints(constraints)
            .build()
        WorkManager.getInstance().enqueueUniquePeriodicWork(TAG, ExistingPeriodicWorkPolicy.KEEP, syncWork)
    }

    companion object {
        private const val TAG = "mozilla.components.service.fretboard"
    }
}
