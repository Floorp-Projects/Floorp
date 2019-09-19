/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

import android.annotation.SuppressLint
import android.content.Context
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.service.glean.Glean
import androidx.annotation.VisibleForTesting
import java.io.File

/**
 * This is the main experiments API, which is exposed through the global [Experiments] object.
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

    private lateinit var context: Context

    /**
     * Initialize the experiments library.
     *
     * This should only be initialized once by the application.
     *
     * @param applicationContext [Context] to access application features, such
     * as shared preferences.  As we cannot enforce through the compiler that the context pass to
     * the initialize function is a applicationContext, there could potentially be a memory leak
     * if the initializing application doesn't comply.
     *
     * @param configuration [Configuration] containing information about the experiments endpoint.
     */
    fun initialize(
        applicationContext: Context,
        configuration: Configuration = Configuration()
    ) {
        if (isInitialized) {
            logger.error("Experiments library should not be initialized multiple times")
            return
        }

        this.configuration = configuration

        experimentsResult = ExperimentsSnapshot(listOf(), null)
        experimentsLoaded = false
        context = applicationContext
        storage = getExperimentsStorage(applicationContext)

        isInitialized = true

        // Load cached experiments from storage. After this, experiments status is available.
        loadExperiments()

        // Load the active experiment from cache, if any.
        activeExperiment = loadActiveExperiment(applicationContext, experiments)

        // If no active experiment was loaded from cache, check the cached experiments list for any
        // that should be launched now.
        if (activeExperiment == null) {
            findAndStartActiveExperiment()
        }

        // We now have the last known experiment state loaded for product code
        // that needs to check it early in startup.
        // Next we need to update the experiments list from the server async,
        // without blocking the app launch and schedule future periodic
        // updates.
        updater = getExperimentsUpdater(applicationContext)
        updater.initialize(configuration)
    }

    /**
     * Returns the [ExperimentsUpdater] for the given [Context].
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun getExperimentsUpdater(context: Context): ExperimentsUpdater {
        return ExperimentsUpdater(context, this)
    }

    /**
     * Returns the [FlatFileExperimentStorage] for the given [Context]
     */
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

    /**
     * Handles the required tasks when new experiments have been fetched from the server.
     *
     * This includes:
     * - Storing the experiments that have been freshly retrieved from the server.
     * - Updating the active experiment
     */
    @Synchronized
    internal fun onExperimentsUpdated(serverState: ExperimentsSnapshot) {
        assert(experimentsLoaded) { "Experiments should have been loaded." }

        experimentsResult = serverState
        storage.save(serverState)

        // Choices here:
        // 1) There currently is an active experiment.
        // 1a) Should it stop? E.g. because it was deleted. If so, continue with 2.
        // 1b) Should it continue? Then nothing else happens here.
        // 2) There is no currently active experiment. Find one in the list, if any.
        // 2a) If there is one...
        // 2b) If there is none, nothing else happens.

        activeExperiment?.let { active ->
            if (experimentsResult.experiments.any { it.id == active.experiment.id }) {
                // This covers 1b) - the active experiment should continue, no action needed.
                logger.info("onExperimentsUpdated - currently active experiment will stay active")
                return
            } else {
                // This covers 1a) - the experiment was removed.
                // Afterwards, fall through to 2) below, which possibly starts a new experiment.
                logger.info("onExperimentsUpdated - currently active experiment will be stopped")
                stopActiveExperiment()
            }
        }

        // This covers 2) - no experiment is currently active, so activate one if any match.
        if (activeExperiment == null) {
            logger.info("onExperimentsUpdated - no experiment currently active, looking for match")
            findAndStartActiveExperiment()
        }
    }

    /**
     * Evaluates the current [experimentsResult] to determine enrollment in any experiments,
     * including reporting enrollment in an experiment in Glean.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun findAndStartActiveExperiment() {
        assert(activeExperiment == null) { "Should not have an active experiment" }

        evaluator.findActiveExperiment(context, experimentsResult.experiments)?.let {
            logger.info("""Activating experiment - id="${it.experiment.id}", branch="${it.branch}"""")
            activeExperiment = it
            it.save(context)
            Glean.setExperimentActive(it.experiment.id, it.branch)
        }
    }

    /**
     * Performs the necessary tasks to stop the active experiment, including reporting this to
     * telemetry via Glean.
     */
    private fun stopActiveExperiment() {
        assert(activeExperiment != null) { "Should have an active experiment" }

        activeExperiment?.let {
            Glean.setExperimentInactive(it.experiment.id)
        }

        ActiveExperiment.clear(context)
        activeExperiment = null
    }

    /**
     * This function finds and returns any active experiments from persisted storage.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun loadActiveExperiment(
        context: Context,
        experiments: List<Experiment>
    ): ActiveExperiment? {
        assert(activeExperiment == null) { "Should not have an active experiment" }

        val activeExperiment = ActiveExperiment.load(context, experiments)

        activeExperiment?.let {
            Glean.setExperimentActive(it.experiment.id, it.branch)
            logger.info("""Loaded active experiment from cache - id="${it.experiment.id}", branch="${it.branch}"""")
        } ?: logger.info("No active experiment in cache")

        return activeExperiment
    }

    /**
     * Checks if the user is part of
     * the specified experiment
     *
     * @param experimentId the id of the experiment
     * @param branchName the name of the branch for the experiment
     *
     * @return true if the user is part of the specified experiment, false otherwise
     */
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    internal fun isInExperiment(experimentId: String, branchName: String): Boolean {
        return activeExperiment?.let {
            (it.experiment.id == experimentId) &&
            (it.branch == branchName)
        } ?: false
    }

    /**
     * Performs an action if the user is part of the specified experiment
     *
     * @param experimentId the id of the experiment
     * @param block block of code to be executed if the user is part of the experiment
     */
    fun withExperiment(experimentId: String, block: (branch: String) -> Unit) {
        activeExperiment?.let {
            if (it.experiment.id == experimentId) {
                block(it.branch)
            }
        }
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
     * Helper function to perform the tasks necessary to override the experiment once it has been
     * set using [setOverride] or [setOverrideNow].
     */
    private fun overrideActiveExperiment() {
        evaluator.findActiveExperiment(context, experimentsResult.experiments)?.let {
            logger.info("""Setting override experiment - id="${it.experiment.id}", branch="${it.branch}"""")
            activeExperiment = it
            Glean.setExperimentActive(it.experiment.id, it.branch)
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
        overrideActiveExperiment()
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
        overrideActiveExperiment()
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
        activeExperiment = null
        findAndStartActiveExperiment()
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
        activeExperiment = null
        findAndStartActiveExperiment()
    }

    /**
     * Clears all experiment overrides asynchronously
     *
     * @param context context
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun clearAllOverrides(context: Context) {
        evaluator.clearAllOverrides(context)
        activeExperiment = null
        findAndStartActiveExperiment()
    }

    /**
     * Clears all experiment overrides as a blocking operation
     *
     * @exception IllegalArgumentException when called from the main thread
     * @param context context
     */
    internal fun clearAllOverridesNow(context: Context) {
        evaluator.clearAllOverridesNow(context)
        activeExperiment = null
        findAndStartActiveExperiment()
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

/**
 * The main Experiments object.
 *
 * This is a global object that must be initialized by the application by calling the [initialize]
 * function before the experiments library can fetch updates from the server or be used to determine
 * experiment enrollment.
 *
 * ```
 * Experiments.initialize(applicationContext)
 * ```
 */
@SuppressLint("StaticFieldLeak")
object Experiments : ExperimentsInternalAPI() {
    internal const val SCHEMA_VERSION = 1
}
