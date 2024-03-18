/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.update

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.work.Constraints
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.ExistingWorkPolicy
import androidx.work.NetworkType
import androidx.work.OneTimeWorkRequest
import androidx.work.OneTimeWorkRequestBuilder
import androidx.work.PeriodicWorkRequest
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkManager
import mozilla.components.service.pocket.PocketStoriesConfig
import mozilla.components.service.pocket.logger
import mozilla.components.service.pocket.update.DeleteSpocsProfileWorker.Companion.DELETE_SPOCS_PROFILE_WORK_TAG
import mozilla.components.service.pocket.update.RefreshSpocsWorker.Companion.REFRESH_SPOCS_WORK_TAG
import mozilla.components.support.base.worker.Frequency

/**
 * Class used to schedule Pocket recommended stories refresh.
 */
internal class SpocsRefreshScheduler(
    private val pocketStoriesConfig: PocketStoriesConfig,
) {
    internal fun schedulePeriodicRefreshes(context: Context) {
        logger.info("Scheduling sponsored stories background refresh")

        val refreshWork = createPeriodicRefreshWorkerRequest(
            frequency = pocketStoriesConfig.sponsoredStoriesRefreshFrequency,
        )

        getWorkManager(context)
            .enqueueUniquePeriodicWork(REFRESH_SPOCS_WORK_TAG, ExistingPeriodicWorkPolicy.KEEP, refreshWork)
    }

    internal fun stopPeriodicRefreshes(context: Context) {
        getWorkManager(context)
            .cancelAllWorkByTag(REFRESH_SPOCS_WORK_TAG)
    }

    internal fun scheduleProfileDeletion(context: Context) {
        logger.info("Scheduling sponsored stories profile deletion")

        val deleteProfileWork = createOneTimeProfileDeletionWorkerRequest()

        getWorkManager(context)
            .enqueueUniqueWork(DELETE_SPOCS_PROFILE_WORK_TAG, ExistingWorkPolicy.KEEP, deleteProfileWork)
    }

    internal fun stopProfileDeletion(context: Context) {
        getWorkManager(context)
            .cancelAllWorkByTag(DELETE_SPOCS_PROFILE_WORK_TAG)
    }

    @VisibleForTesting
    internal fun createOneTimeProfileDeletionWorkerRequest(): OneTimeWorkRequest {
        val constraints = getWorkerConstrains()

        return OneTimeWorkRequestBuilder<DeleteSpocsProfileWorker>()
            .apply {
                setConstraints(constraints)
                addTag(DELETE_SPOCS_PROFILE_WORK_TAG)
            }
            .build()
    }

    @VisibleForTesting
    internal fun createPeriodicRefreshWorkerRequest(
        frequency: Frequency,
    ): PeriodicWorkRequest {
        val constraints = getWorkerConstrains()

        return PeriodicWorkRequestBuilder<RefreshSpocsWorker>(
            frequency.repeatInterval,
            frequency.repeatIntervalTimeUnit,
        ).apply {
            setConstraints(constraints)
            addTag(REFRESH_SPOCS_WORK_TAG)
        }.build()
    }

    @VisibleForTesting
    internal fun getWorkerConstrains() = Constraints.Builder()
        .setRequiredNetworkType(NetworkType.CONNECTED)
        .build()

    @VisibleForTesting
    internal fun getWorkManager(context: Context) = WorkManager.getInstance(context)
}
