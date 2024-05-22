/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.fxsuggest

import android.content.Context
import androidx.work.Constraints
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.NetworkType
import androidx.work.PeriodicWorkRequest
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkManager
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.worker.Frequency
import java.util.concurrent.TimeUnit

/**
 * Schedules a periodic background task to incrementally download and persist new Firefox Suggest
 * search suggestions.
 *
 * @property context The Android application context.
 * @property frequency The optional interval period for the background task. Defaults to 1 day.
 */
class FxSuggestIngestionScheduler(
    private val context: Context,
    private val frequency: Frequency = Frequency(repeatInterval = 1, repeatIntervalTimeUnit = TimeUnit.DAYS),
) {
    private val logger = Logger("FxSuggestIngestionScheduler")

    /**
     * Schedules a periodic background task to ingest new suggestions. Does nothing if the task is
     * already scheduled.
     */
    fun startPeriodicIngestion() {
        logger.info("Scheduling periodic ingestion for new suggestions")
        WorkManager.getInstance(context).enqueueUniquePeriodicWork(
            FxSuggestIngestionWorker.WORK_TAG,
            ExistingPeriodicWorkPolicy.KEEP,
            createPeriodicIngestionWorkerRequest(),
        )
    }

    /**
     * Cancels a scheduled background task to ingest new suggestions.
     */
    fun stopPeriodicIngestion() {
        logger.info("Canceling periodic ingestion for new suggestions")
        WorkManager.getInstance(context).cancelAllWorkByTag(FxSuggestIngestionWorker.WORK_TAG)
    }

    internal fun createPeriodicIngestionWorkerRequest(): PeriodicWorkRequest {
        val constraints = getWorkerConstrains()
        return PeriodicWorkRequestBuilder<FxSuggestIngestionWorker>(
            this.frequency.repeatInterval,
            this.frequency.repeatIntervalTimeUnit,
        ).apply {
            // Don't run the ingestion immediately, wait until the first repeat interval has passed.
            // FenixApplication calls `runStartupIngestion` on startup to handle ingestion on first
            // run and after updates.
            setInitialDelay(frequency.repeatInterval, frequency.repeatIntervalTimeUnit)
            setConstraints(constraints)
            addTag(FxSuggestIngestionWorker.WORK_TAG)
        }.build()
    }

    internal fun getWorkerConstrains() = Constraints.Builder()
        .setRequiredNetworkType(NetworkType.UNMETERED)
        .setRequiresBatteryNotLow(true)
        .setRequiresStorageNotLow(true)
        .build()
}
