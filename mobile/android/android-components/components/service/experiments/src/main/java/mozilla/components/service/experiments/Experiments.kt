/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

import android.content.Context
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.service.glean.Glean
import androidx.annotation.VisibleForTesting
import java.io.File

/**
 * Entry point of the library.
 */
@Suppress("TooManyFunctions")
open class ExperimentsInternalAPI internal constructor() {
    private val logger: Logger = Logger(LOG_TAG)

    @Volatile internal var experimentsResult: ExperimentsSnapshot = ExperimentsSnapshot(listOf(), null)
    private var experimentsLoaded: Boolean = false
    private var evaluator: ExperimentEvaluator = ExperimentEvaluator(ValuesProvider())
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var valuesProvider: ValuesProvider = ValuesProvider()
        set(provider) {
            field = provider
            evaluator = ExperimentEvaluator(field)
        }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var activeExperiment: ActiveExperiment? = null

    private lateinit var storage: FlatFileExperimentStorage
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal lateinit var updater: ExperimentsUpdater

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var isInitialized = false

    internal lateinit var configuration: Configuration

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
        configuration: Configuration = Configuration()
    ) {
        if (isInitialized) {
            logger.error("Experiments library should not be initialized multiple times")
            return
        }

        // Any code below might trigger recording into Glean, so make sure Glean is initialized.
        if (!isGleanInitialized()) {
            logger.error("Glean library must be initialized first")
            return
        }

        this.configuration = configuration

        experimentsResult = ExperimentsSnapshot(listOf(), null)
        experimentsLoaded = false

        storage = getExperimentsStorage(applicationContext)

        isInitialized = true

        // Load cached experiments from storage. After this, experiments status
        // is available.
        loadExperiments()

        // Load active experiment from cache, if any.
        activeExperiment = loadActiveExperiment(applicationContext, experiments)

        // We now have the last known experiment state loaded for product code
        // that needs to check it early in startup.
        // Next we need to update the experiments list from the server async,
        // without blocking the app launch and schedule future periodic
        // updates.
        updater = getExperimentsUpdater(applicationContext)
        updater.initialize(configuration)
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun isGleanInitialized(): Boolean {
        return Glean.isInitialized()
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun getExperimentsUpdater(context: Context): ExperimentsUpdater {
        return ExperimentsUpdater(context, this)
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun getExperimentsStorage(context: Context): FlatFileExperimentStorage {
        return FlatFileExperimentStorage(
                File(context.getDir(EXPERIMENTS_DATA_DIR, Context.MODE_PRIVATE),
                    EXPERIMENTS_JSON_FILENAME)
        )
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
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun loadExperiments() {
        experimentsResult = storage.retrieve()
        experimentsLoaded = true
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun loadActiveExperiment(
        context: Context,
        experiments: List<Experiment>
    ): ActiveExperiment? {
        val activeExperiment = ActiveExperiment.load(context, experiments)
        logger.info(activeExperiment?.let
            { """Loaded active experiment - id="${it.experiment.id}", branch="${it.branch}"""" }
            ?: "No active experiment"
        )

        return activeExperiment
    }

    /**
     * Requests new experiments from the server and
     * saves them to local storage
     */
    @Synchronized
    internal fun onExperimentsUpdated(serverState: ExperimentsSnapshot) {
        assert(experimentsLoaded) { "Experiments should have been loaded." }

        experimentsResult = serverState
        storage.save(serverState)
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
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    internal fun isInExperiment(context: Context, experimentId: String): Boolean {
        return evaluator.evaluate(context, ExperimentDescriptor(experimentId), experimentsResult.experiments) != null
    }

    /**
     * Performs an action if the user is part of the specified experiment
     *
     * @param context context
     * @param experimentId the id of the experiment
     * @param block block of code to be executed if the user is part of the experiment
     */
    fun withExperiment(context: Context, experimentId: String, block: (branch: String) -> Unit) {
        val activeExperiment = evaluator.evaluate(
            context,
            ExperimentDescriptor(experimentId), experimentsResult.experiments
        )
        activeExperiment?.let { block(it.branch) }
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
        return experiments.filter { isInExperiment(context, it.id) }
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
            it.id to
                    isInExperiment(context, it.id)
        }
    }

    /**
     * Overrides a specified experiment asynchronously
     *
     * @param context context
     * @param experimentId the id of the experiment
     * @param active overridden value for the experiment, true to activate it, false to deactivate
     * @param branchName overridden branch name for the experiment
     */
    internal fun setOverride(
        context: Context,
        experimentId: String,
        active: Boolean,
        branchName: String
    ) {
        evaluator.setOverride(context, ExperimentDescriptor(experimentId), active, branchName)
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
    internal fun setOverrideNow(
        context: Context,
        experimentId: String,
        active: Boolean,
        branchName: String
    ) {
        evaluator.setOverrideNow(context, ExperimentDescriptor(experimentId), active, branchName)
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
    }
}

object Experiments : ExperimentsInternalAPI() {
    internal const val SCHEMA_VERSION = 1
}
