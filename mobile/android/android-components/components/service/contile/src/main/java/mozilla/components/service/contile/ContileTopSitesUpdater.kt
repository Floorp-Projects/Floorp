/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.contile

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.work.Constraints
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.NetworkType
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkManager
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.worker.Frequency
import java.util.concurrent.TimeUnit

/**
 * Provides functionality to schedule updates of Contile top sites.
 *
 * @property context A reference to the application context.
 * @property provider An instance of [ContileTopSitesProvider] which provides access to the Contile
 * services API for fetching top sites.
 * @property frequency Optional [Frequency] that specifies how often the Contile top site updates
 * should happen.
 */
class ContileTopSitesUpdater(
    private val context: Context,
    private val provider: ContileTopSitesProvider,
    private val frequency: Frequency = Frequency(1, TimeUnit.DAYS),
) {

    private val logger = Logger("ContileTopSitesUpdater")

    /**
     * Starts a work request in the background to periodically update the list of
     * Contile top sites.
     */
    fun startPeriodicWork() {
        ContileTopSitesUseCases.initialize(provider)

        WorkManager.getInstance(context).enqueueUniquePeriodicWork(
            PERIODIC_WORK_TAG,
            ExistingPeriodicWorkPolicy.KEEP,
            createPeriodicWorkRequest(),
        )

        logger.info("Started periodic work to update Contile top sites")
    }

    /**
     * Stops the work request to periodically update the list of Contile top sites.
     */
    fun stopPeriodicWork() {
        ContileTopSitesUseCases.destroy()

        WorkManager.getInstance(context).cancelUniqueWork(PERIODIC_WORK_TAG)

        logger.info("Stopped periodic work to update Contile top sites")
    }

    @VisibleForTesting
    internal fun createPeriodicWorkRequest() =
        PeriodicWorkRequestBuilder<ContileTopSitesUpdaterWorker>(
            repeatInterval = frequency.repeatInterval,
            repeatIntervalTimeUnit = frequency.repeatIntervalTimeUnit,
        ).apply {
            setConstraints(getWorkerConstraints())
            addTag(PERIODIC_WORK_TAG)
        }.build()

    @VisibleForTesting
    internal fun getWorkerConstraints() = Constraints.Builder()
        .setRequiresStorageNotLow(true)
        .setRequiredNetworkType(NetworkType.CONNECTED)
        .build()

    companion object {
        internal const val PERIODIC_WORK_TAG = "mozilla.components.service.contile.periodicWork"
    }
}
