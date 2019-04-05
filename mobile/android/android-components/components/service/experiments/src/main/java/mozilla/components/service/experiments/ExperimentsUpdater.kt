/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

import android.content.Context
import android.support.annotation.VisibleForTesting
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.launch
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.Job
import mozilla.components.support.base.log.logger.Logger

/**
 * Class used to schedule sync of experiment
 * configuration from the server using WorkManager
 */
internal class ExperimentsUpdater(
    private val context: Context,
    private val experiments: ExperimentsInternalAPI
) {
    private val logger: Logger = Logger(LOG_TAG)

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal lateinit var source: KintoExperimentSource

    @Volatile private var updateJob: Job? = null

    internal fun initialize(configuration: Configuration) {
        source = getExperimentSource(configuration)

        updateJob = GlobalScope.launch(IO) {
            // Update the experiments list from the server async, without blocking
            // the app launch.
            updateExperiments()
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    internal suspend fun testWaitForUpdate() {
        updateJob!!.join()
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun getExperimentSource(configuration: Configuration): KintoExperimentSource {
        return KintoExperimentSource(
                EXPERIMENTS_BASE_URL,
                EXPERIMENTS_BUCKET_NAME,
                EXPERIMENTS_COLLECTION_NAME,
                configuration.httpClient.value
        )
    }

    /**
     * Requests new experiments from the server and
     * saves them to local storage
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @Synchronized
    internal fun updateExperiments(): Boolean {
        return try {
            val serverExperiments = source.getExperiments(experiments.experimentsResult)
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

        private const val TAG = "mozilla.components.service.experiments"
        private const val UPDATE_INTERVAL_HOURS = 6L

        // Where to access the experiments data in Kinto.
        // E.g.: https://firefox.settings.services.mozilla.com/v1/buckets/fennec/collections/experiments/records
        private const val EXPERIMENTS_BASE_URL = "https://settings.prod.mozaws.net/v1"
        private const val EXPERIMENTS_BUCKET_NAME = "main"
        private const val EXPERIMENTS_COLLECTION_NAME = "focus-experiments"
    }
}
