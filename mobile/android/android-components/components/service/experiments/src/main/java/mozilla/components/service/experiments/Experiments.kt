/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

import android.content.Context
import mozilla.components.support.base.log.logger.Logger
import android.support.annotation.VisibleForTesting
import mozilla.components.concept.fetch.Client
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import java.io.File

/**
 * Entry point of the library
 *
 * @property source experiment remote source
 * @property storage experiment local storage mechanism
 * @param valuesProvider provider for the device's values
 */
@Suppress("TooManyFunctions")
open class ExperimentsInternalAPI internal constructor() {
    private val logger: Logger = Logger(LOG_TAG)

    @Volatile private var experimentsResult: ExperimentsSnapshot = ExperimentsSnapshot(listOf(), null)
    private var experimentsLoaded: Boolean = false
    private var evaluator: ExperimentEvaluator = ExperimentEvaluator(ValuesProvider())
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var valuesProvider: ValuesProvider = ValuesProvider()
        set(provider) {
            field = provider
            evaluator = ExperimentEvaluator(field)
        }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var source: ExperimentSource? = null
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var storage: ExperimentStorage? = null

    private var isInitialized = false

    /**
     * Initialize the experiments library.
     *
     * This should only be initialized once by the application.
     *
     * @param applicationContext [Context] to access application features, such
     * as shared preferences.
     */
    fun initialize(
        applicationContext: Context,
        fetchClient: Client = HttpURLConnectionClient()
    ) {
        if (isInitialized) {
            logger.error("Experiments library should not be initialized multiple times")
            return
        }

        experimentsResult = ExperimentsSnapshot(listOf(), null)
        experimentsLoaded = false

        if (source == null) {
            source = KintoExperimentSource(
                EXPERIMENTS_BASE_URL,
                EXPERIMENTS_BUCKET_NAME,
                EXPERIMENTS_COLLECTION_NAME,
                fetchClient
            )
        }
        if (storage == null) {
            storage = FlatFileExperimentStorage(
                File(applicationContext.getDir(EXPERIMENTS_DATA_DIR, Context.MODE_PRIVATE),
                    EXPERIMENTS_JSON_FILENAME)
            )
        }

        isInitialized = true
    }

    /**
     * Reset the library in tests.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    internal fun testReset() {
        experimentsResult = ExperimentsSnapshot(listOf(), null)
        experimentsLoaded = false
        evaluator = ExperimentEvaluator(ValuesProvider())
        valuesProvider = ValuesProvider()

        source = null
        storage = null

        isInitialized = false
    }

    /**
     * Provides the list of experiments (active or not)
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal val experiments: List<Experiment>
        get() = experimentsResult.experiments.toList()

    /**
     * Loads experiments from local storage
     */
    @Synchronized
    internal fun loadExperiments() {
        storage?.let {
            experimentsResult = it.retrieve()
            experimentsLoaded = true
        } ?: logger.error("No storage specified")
    }

    /**
     * Requests new experiments from the server and
     * saves them to local storage
     */
    @Synchronized
    internal fun updateExperiments(): Boolean {
        if (!experimentsLoaded) {
            loadExperiments()
        }

        return try {
            source?.let {
                val serverExperiments = it.getExperiments(experimentsResult)
                experimentsResult = serverExperiments
                storage?.save(serverExperiments)
            } ?: logger.error("No source available")
            true
        } catch (e: ExperimentDownloadException) {
            // Keep using the local experiments
            logger.error(e.message, e)
            false
        }
    }

    /**
     * Checks if the user is part of
     * the specified experiment
     *
     * @param context context
     * @param experimentId the id of the experiment
     *
     * @return true if the user is part of the specified experiment, false otherwise
     */
    fun isInExperiment(context: Context, experimentId: String): Boolean {
        return evaluator.evaluate(context, ExperimentDescriptor(experimentId), experimentsResult.experiments) != null
    }

    /**
     * Performs an action if the user is part of the specified experiment
     *
     * @param context context
     * @param experimentId the id of the experiment
     * @param block block of code to be executed if the user is part of the experiment
     */
    fun withExperiment(context: Context, experimentId: String, block: () -> Unit) {
        evaluator.evaluate(context, ExperimentDescriptor(experimentId), experimentsResult.experiments)?.let { block() }
    }

    /**
     * Gets the metadata associated with the specified experiment, even if the user is not part of it
     *
     * @param experimentId the id of the experiment
     *
     * @return metadata associated with the experiment
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun getExperiment(experimentId: String): Experiment? {
        return evaluator.getExperiment(ExperimentDescriptor(experimentId), experimentsResult.experiments)
    }

    /**
     * Provides the list of active experiments
     *
     * @param context context
     *
     * @return active experiments
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun getActiveExperiments(context: Context): List<Experiment> {
        return experiments.filter { isInExperiment(context, it.name) }
    }

    /**
     * Provides a map of active/inactive experiments
     *
     * @param context context
     *
     * @return map of experiments to A/B state
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun getExperimentsMap(context: Context): Map<String, Boolean> {
        return experiments.associate {
            it.name to
                    isInExperiment(context, it.name)
        }
    }

    /**
     * Overrides a specified experiment asynchronously
     *
     * @param context context
     * @param experimentId the id of the experiment
     * @param active overridden value for the experiment, true to activate it, false to deactivate
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun setOverride(context: Context, experimentId: String, active: Boolean) {
        evaluator.setOverride(context, ExperimentDescriptor(experimentId), active)
    }

    /**
     * Overrides a specified experiment as a blocking operation
     *
     * @exception IllegalArgumentException when called from the main thread
     * @param context context
     * @param experimentId the id of the experiment
     * @param active overridden value for the experiment, true to activate it, false to deactivate
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun setOverrideNow(context: Context, experimentId: String, active: Boolean) {
        evaluator.setOverrideNow(context, ExperimentDescriptor(experimentId), active)
    }

    /**
     * Clears an override for a specified experiment asynchronously
     *
     * @param context context
     * @param experimentId the id of the experiment
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun clearOverride(context: Context, experimentId: String) {
        evaluator.clearOverride(context, ExperimentDescriptor(experimentId))
    }

    /**
     * Clears an override for a specified experiment as a blocking operation
     *
     *
     * @exception IllegalArgumentException when called from the main thread
     * @param context context
     * @param experimentId the id of the experiment
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun clearOverrideNow(context: Context, experimentId: String) {
        evaluator.clearOverrideNow(context, ExperimentDescriptor(experimentId))
    }

    /**
     * Clears all experiment overrides asynchronously
     *
     * @param context context
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun clearAllOverrides(context: Context) {
        evaluator.clearAllOverrides(context)
    }

    /**
     * Clears all experiment overrides as a blocking operation
     *
     * @exception IllegalArgumentException when called from the main thread
     * @param context context
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun clearAllOverridesNow(context: Context) {
        evaluator.clearAllOverridesNow(context)
    }

    /**
     * Returns the user bucket number used to determine whether the user
     * is in or out of the experiment
     *
     * @param context context
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun getUserBucket(context: Context): Int {
        return evaluator.getUserBucket(context)
    }

    companion object {
        private const val LOG_TAG = "experiments"
        private const val EXPERIMENTS_DATA_DIR = "experiments-service"

        private const val EXPERIMENTS_JSON_FILENAME = "experiments.json"
        private const val EXPERIMENTS_BASE_URL = "https://settings.prod.mozaws.net/v1"
        private const val EXPERIMENTS_BUCKET_NAME = "<TODO>"
        private const val EXPERIMENTS_COLLECTION_NAME = "<TODO>"
        // E.g.: https://firefox.settings.services.mozilla.com/v1/buckets/fennec/collections/experiments/records
    }
}

object Experiments : ExperimentsInternalAPI() {
    internal const val SCHEMA_VERSION = 1
}
