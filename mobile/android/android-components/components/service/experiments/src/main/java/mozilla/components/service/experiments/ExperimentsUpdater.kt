/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.work.Constraints
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.NetworkType
import androidx.work.PeriodicWorkRequest
import androidx.work.WorkManager
import androidx.work.Worker
import androidx.work.WorkerParameters
import mozilla.components.support.base.log.logger.Logger
import java.util.concurrent.TimeUnit

/**
 * This class represents a [Worker] object that will fetch updates to the experiments.
 */
internal class ExperimentsUpdaterWorker(
    context: Context,
    workerParameters: WorkerParameters
) : Worker(context, workerParameters) {

    override fun doWork(): Result {
        return if (Experiments.updater.updateExperiments()) {
            Result.success()
        } else {
            Result.failure()
        }
    }
}

/**
 * Class used to schedule sync of experiment
 * configuration from the server using WorkManager
 */
internal class ExperimentsUpdater(
    private val context: Context,
    private val experiments: ExperimentsInternalAPI
) {
    private val logger: Logger = Logger(LOG_TAG)
    private lateinit var config: Configuration

    internal lateinit var source: KintoExperimentSource

    internal fun initialize(configuration: Configuration) {
        config = configuration
        source = getExperimentSource(configuration)

        // Schedule the periodic experiment updates
        scheduleUpdates()
    }

    /**
     * Schedule update with the default constraints, but only if the periodic worker is not already
     * scheduled.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun scheduleUpdates() {
        WorkManager.getInstance().enqueueUniquePeriodicWork(
            TAG,
            // We rely on REPLACE behavior to run immediately and then schedule the next update per
            // the specified interval.  KEEP behavior does not run immediately and it is desired to
            // have fresh experiments from the server on startup.
            ExistingPeriodicWorkPolicy.REPLACE,
            getWorkRequest())
    }

    /**
     * Builds the periodic work request that will control the scheduling of the worker and provide
     * the tag value so that the worker can be interacted with later.
     *
     * @return [PeriodicWorkRequest]
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun getWorkRequest(): PeriodicWorkRequest {
        return PeriodicWorkRequest.Builder(
            ExperimentsUpdaterWorker::class.java, UPDATE_INTERVAL_HOURS, TimeUnit.HOURS)
            .addTag(TAG)
            .setConstraints(getWorkConstraints())
            .build()
    }

    /**
     * Builds the [Constraints] for when the worker is allowed to run.
     *
     * @return [Constraints]
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun getWorkConstraints(): Constraints {
        return Constraints.Builder()
            .setRequiredNetworkType(NetworkType.CONNECTED)
            .build()
    }

    /**
     * Returns the experiment source for which to update the experiments
     *
     * @return [KintoExperimentSource]
     */
    internal fun getExperimentSource(configuration: Configuration): KintoExperimentSource {
        return KintoExperimentSource(
                configuration.kintoEndpoint,
                EXPERIMENTS_BUCKET_NAME,
                EXPERIMENTS_COLLECTION_NAME,
                configuration.httpClient.value
        )
    }

    /**
     * Requests new experiments from the server and
     * saves them to local storage
     *
     * @return true if the experiments were updated, false if there was an [ExperimentDownloadException]
     */
    @Synchronized
    internal fun updateExperiments(): Boolean {
        return try {
            val serverExperiments = getExperimentSource(config).getExperiments(experiments.experimentsResult)
            logger.info("Experiments update from server: $serverExperiments")
            experiments.onExperimentsUpdated(serverExperiments)
            true
        } catch (e: ExperimentDownloadException) {
            // Keep using the local experiments
            logger.error("Failed to update experiments: ${e.message}", e)
            false
        }
    }

    companion object {
        private const val LOG_TAG = "experiments"

        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        internal const val TAG = "mozilla.components.service.experiments"
        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        internal const val UPDATE_INTERVAL_HOURS = 6L

        // Where to access the experiments data in Kinto.
        // Live example: https://firefox.settings.services.mozilla.com/v1/buckets/fennec/collections/experiments/records
        // There are different base URIs to use:
        const val KINTO_ENDPOINT_DEV = "https://kinto.dev.mozaws.net/v1"
        const val KINTO_ENDPOINT_STAGING = "https://settings-writer.stage.mozaws.net/v1"
        const val KINTO_ENDPOINT_PROD = "https://firefox.settings.services.mozilla.com/v1"

        private const val EXPERIMENTS_BUCKET_NAME = "main"
        private const val EXPERIMENTS_COLLECTION_NAME = "mobile-experiments"
    }
}
