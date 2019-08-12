/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import android.annotation.SuppressLint
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import androidx.annotation.VisibleForTesting
import androidx.lifecycle.ProcessLifecycleOwner
import kotlinx.coroutines.Job
import kotlinx.coroutines.joinAll
import mozilla.components.service.glean.GleanMetrics.GleanBaseline
import java.io.File
import java.util.UUID

import mozilla.components.service.glean.config.Configuration
import mozilla.components.service.glean.GleanMetrics.GleanInternalMetrics
import mozilla.components.service.glean.GleanMetrics.Pings
import mozilla.components.service.glean.ping.PingMaker
import mozilla.components.service.glean.private.PingType
import mozilla.components.service.glean.scheduler.GleanLifecycleObserver
import mozilla.components.service.glean.scheduler.MetricsPingScheduler
import mozilla.components.service.glean.scheduler.PingUploadWorker
import mozilla.components.service.glean.storages.StorageEngineManager
import mozilla.components.service.glean.storages.PingStorageEngine
import mozilla.components.service.glean.storages.ExperimentsStorageEngine
import mozilla.components.service.glean.storages.UuidsStorageEngine
import mozilla.components.service.glean.storages.DatetimesStorageEngine
import mozilla.components.service.glean.storages.EventsStorageEngine
import mozilla.components.service.glean.storages.RecordedExperimentData
import mozilla.components.service.glean.storages.StringsStorageEngine
import mozilla.components.service.glean.utils.ensureDirectoryExists
import mozilla.components.service.glean.utils.getLocaleTag
import mozilla.components.service.glean.utils.parseISOTimeString
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.content.isMainProcess

@Suppress("TooManyFunctions", "LargeClass")
open class GleanInternalAPI internal constructor () {
    private val logger = Logger("glean/Glean")

    private var applicationContext: Context? = null

    // Include our singletons of StorageEngineManager and PingMaker
    private lateinit var storageEngineManager: StorageEngineManager
    internal lateinit var pingMaker: PingMaker
    internal lateinit var configuration: Configuration

    private val gleanLifecycleObserver by lazy { GleanLifecycleObserver() }

    // `internal` so this can be modified for testing
    internal var initialized = false
    internal var uploadEnabled = true

    // The application id detected by Glean to be used as part of the submission
    // endpoint.
    internal lateinit var applicationId: String

    // This object holds data related to any persistent information about the metrics ping,
    // such as the last time it was sent and the store name
    internal lateinit var metricsPingScheduler: MetricsPingScheduler

    internal lateinit var pingStorageEngine: PingStorageEngine

    companion object {
        internal val KNOWN_CLIENT_ID = UUID.fromString(
            "c0ffeec0-ffee-c0ff-eec0-ffeec0ffeec0"
        )
    }

    /**
     * Initialize Glean.
     *
     * This should only be initialized once by the application, and not by
     * libraries using Glean. A message is logged to error and no changes are made
     * to the state if initialize is called a more than once.
     *
     * A LifecycleObserver will be added to send pings when the application goes
     * into the background.
     *
     * @param applicationContext [Context] to access application features, such
     * as shared preferences
     * @param configuration A Glean [Configuration] object with global settings.
     */
    fun initialize(
        applicationContext: Context,
        configuration: Configuration = Configuration()
    ) {
        // In certain situations Glean.initialize may be called from a process other than the main
        // process.  In this case we want initialize to be a no-op and just return.
        if (!applicationContext.isMainProcess()) {
            logger.error("Attempted to initialize Glean on a process other than the main process")
            return
        }

        if (isInitialized()) {
            logger.error("Glean should not be initialized multiple times")
            return
        }

        registerPings(Pings)

        this.applicationContext = applicationContext

        storageEngineManager = StorageEngineManager(applicationContext = applicationContext)
        pingMaker = PingMaker(storageEngineManager, applicationContext)
        this.configuration = configuration
        applicationId = sanitizeApplicationId(applicationContext.packageName)
        pingStorageEngine = PingStorageEngine(applicationContext)

        // Core metrics are initialized using the engines, without calling the async
        // API. For this reason we're safe to set `initialized = true` right after it.
        onChangeUploadEnabled(uploadEnabled)

        // This must be set before anything that might trigger the sending of pings.
        initialized = true

        // Deal with any pending events so we can start recording new ones
        EventsStorageEngine.onReadyToSendPings(applicationContext)

        // Set up information and scheduling for Glean owned pings. Ideally, the "metrics"
        // ping startup check should be performed before any other ping, since it relies
        // on being dispatched to the API context before any other metric.
        metricsPingScheduler = MetricsPingScheduler(applicationContext)

        // If Glean is being initialized from a test, then we want to skip running the
        // startupCheck() as it can cause some intermittent test failures due to multi-
        // threaded race conditions.
        @Suppress("EXPERIMENTAL_API_USAGE")
        if (!Dispatchers.API.testingMode) {
            metricsPingScheduler.schedule()
        }

        // Signal Dispatcher that init is complete
        @Suppress("EXPERIMENTAL_API_USAGE")
        Dispatchers.API.flushQueuedInitialTasks()

        // At this point, all metrics and events can be recorded.
        ProcessLifecycleOwner.get().lifecycle.addObserver(gleanLifecycleObserver)
    }

    /**
     * Returns true if the Glean library has been initialized.
     */
    fun isInitialized(): Boolean {
        return initialized
    }

    /**
     * Register the pings generated from `pings.yaml` with Glean.
     *
     * @param pings The `Pings` object generated for your library or application
     * by Glean.
     */
    fun registerPings(pings: Any) {
        // Instantiating the Pings object to send this function is enough to
        // call the constructor and have it registered in [PingType.pingRegistry].
        logger.info("Registering pings for ${pings.javaClass.canonicalName}")
    }

    /**
     * Enable or disable Glean collection and upload.
     *
     * Metric collection is enabled by default.
     *
     * When uploading is disabled, metrics aren't recorded at all and no data
     * is uploaded.
     *
     * When disabling, all pending metrics, events and queued pings are cleared.
     *
     * When enabling, the core Glean metrics are recreated.
     *
     * If the value of this flag is not actually changed, this is a no-op.
     *
     * @param enabled When true, enable metric collection.
     */
    fun setUploadEnabled(enabled: Boolean) {
        logger.info("Metrics enabled: $enabled")
        val origUploadEnabled = uploadEnabled
        uploadEnabled = enabled
        if (isInitialized() && origUploadEnabled != enabled) {
            onChangeUploadEnabled(enabled)
        }
    }

    /**
     * Get whether or not Glean is allowed to record and upload data.
     */
    fun getUploadEnabled(): Boolean {
        return uploadEnabled
    }

    /**
     * Handles the changing of state when uploadEnabled changes.
     *
     * When disabling, all pending metrics, events and queued pings are cleared.
     *
     * When enabling, the core Glean metrics are recreated.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun onChangeUploadEnabled(enabled: Boolean) {
        if (enabled) {
            initializeCoreMetrics(applicationContext!!)
        } else {
            cancelPingWorkers()
            clearMetrics()
        }
    }

    /**
     * Cancel any pending [PingUploadWorker] objects that have been enqueued so that we don't
     * accidentally upload or collect data after the upload has been disabled.
     */
    private fun cancelPingWorkers() {
        MetricsPingScheduler.cancel()
        PingUploadWorker.cancel()
    }

    /**
     * Clear any pending metrics when telemetry is disabled.
     */
    @Suppress("EXPERIMENTAL_API_USAGE")
    private fun clearMetrics() = Dispatchers.API.launch {
        // There is only one metric that we want to survive after clearing all metrics:
        // firstRunDate. Here, we store its value so we can restore it after clearing
        // the metrics.
        val firstRunDateMetric = GleanInternalMetrics.firstRunDate
        val existingFirstRunDate = DatetimesStorageEngine.getSnapshot(
            firstRunDateMetric.sendInPings[0], false)?.get(firstRunDateMetric.identifier)?.let {
            parseISOTimeString(it)
        }

        pingStorageEngine.clearPendingPings()
        storageEngineManager.clearAllStores()
        pingMaker.resetPingSequenceNumbers()

        // This does not clear the experiments store (which isn't managed by the
        // StorageEngineManager), since doing so would mean we would have to have the
        // application tell us again which experiments are active if telemetry is
        // re-enabled.

        // Store a "dummy" KNOWN_CLIENT_ID in clientId.  This will make it easier to detect if
        // pings were unintentionally sent after uploading is disabled.
        UuidsStorageEngine.record(GleanInternalMetrics.clientId, KNOWN_CLIENT_ID)

        // Restore the firstRunDate
        existingFirstRunDate?.let {
            DatetimesStorageEngine.set(firstRunDateMetric, it)
        }
    }

    /**
     * Indicate that an experiment is running. Glean will then add an
     * experiment annotation to the environment which is sent with pings. This
     * information is not persisted between runs.
     *
     * @param experimentId The id of the active experiment (maximum
     *     30 bytes)
     * @param branch The experiment branch (maximum 30 bytes)
     * @param extra Optional metadata to output with the ping
     */
    fun setExperimentActive(
        experimentId: String,
        branch: String,
        extra: Map<String, String>? = null
    ) {
        ExperimentsStorageEngine.setExperimentActive(experimentId, branch, extra)
    }

    /**
     * Indicate that an experiment is no longer running.
     *
     * @param experimentId The id of the experiment to deactivate.
     */
    fun setExperimentInactive(experimentId: String) {
        ExperimentsStorageEngine.setExperimentInactive(experimentId)
    }

    /**
     * Tests whether an experiment is active, for testing purposes only.
     *
     * @param experimentId the id of the experiment to look for.
     * @return true if the experiment is active and reported in pings, otherwise false
     */
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    fun testIsExperimentActive(experimentId: String): Boolean {
        return ExperimentsStorageEngine.getSnapshot()[experimentId] != null
    }

    /**
     * Returns the stored data for the requested active experiment, for testing purposes only.
     *
     * @param experimentId the id of the experiment to look for.
     * @return the [RecordedExperimentData] for the experiment
     * @throws [NullPointerException] if the requested experiment is not active
     */
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    fun testGetExperimentData(experimentId: String): RecordedExperimentData {
        return ExperimentsStorageEngine.getSnapshot().getValue(experimentId)
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun makePath(docType: String, uuid: UUID): String {
        return "/submit/$applicationId/$docType/${Glean.SCHEMA_VERSION}/$uuid"
    }

    /**
     * Fixes some legacy metrics.
     *
     * This is a BACKWARD COMPATIBILITY HACK.
     * See  1539480: The implementation of 1528787 moved the client_id and
     * first_run_date metrics from the ping_info to the client_info
     * sections. This introduced a bug that the client_id would not be
     * picked up from the old location on devices that had already run
     * the application. Missing a client_id is particularly problematic
     * because these pings will be rejected by the pipeline. This fix
     * looks for these metrics at their old locations, and if found,
     * copies them to the new location. If they already exist in the
     * new location, this shouldn't override them.
     */
    private fun fixLegacyPingInfoMetrics() {
        val uuidClientInfoSnapshot = UuidsStorageEngine.getSnapshot("glean_client_info", false)
        val newClientId = uuidClientInfoSnapshot?.get("client_id")
        if (newClientId == null) {
            val uuidPingInfoSnapshot = UuidsStorageEngine.getSnapshot("glean_ping_info", false)
            val legacyClientId = uuidPingInfoSnapshot?.get("client_id")
            if (legacyClientId != null) {
                UuidsStorageEngine.record(GleanInternalMetrics.clientId, legacyClientId)
            } else {
                // Apparently, this is a very old build that was already run
                // at least once and has a different name for the underlying
                // UUID storage engine persistence (i.e. UuidStorageEngine.xml
                // vs using the full class name with the namespace).
                // This should be mostly devs and super early adopters, so just regenerate
                // the data.
                UuidsStorageEngine.record(GleanInternalMetrics.clientId, UUID.randomUUID())
            }
        }

        val datetimeClientInfoSnapshot = DatetimesStorageEngine.getSnapshot("glean_client_info", false)
        val newFirstRunDate = datetimeClientInfoSnapshot?.get("first_run_date")
        if (newFirstRunDate == null) {
            val datetimePingInfoSnapshot = DatetimesStorageEngine.getSnapshot("glean_ping_info", false)
            var firstRunDateSet = false
            datetimePingInfoSnapshot?.get("first_run_date")?.let {
                parseISOTimeString(it)?.let { parsedDate ->
                    DatetimesStorageEngine.set(GleanInternalMetrics.firstRunDate, parsedDate)
                    firstRunDateSet = true
                }
            }

            if (!firstRunDateSet) {
                // Apparently, this is a very old build that was already run
                // at least once and has a different name for the underlying
                // Datetime storage engine persistence (i.e. DatetimeStorageEngine.xml
                // vs using the full class name with the namespace).
                // This should be mostly devs and super early adopters, so just regenerate
                // the data.
                DatetimesStorageEngine.set(GleanInternalMetrics.firstRunDate)
            }
        }
    }

    /**
     * Initialize the core metrics internally managed by Glean (e.g. client id).
     */
    private fun initializeCoreMetrics(applicationContext: Context) {
        // Since all of the ping_info properties are required, we can't
        // use the normal metrics API to set them, since those work
        // asynchronously, and there is a race condition between when they
        // are set and the possible sending of the first ping upon startup.
        // Therefore, this uses the lower-level internal storage engine API
        // to set these metrics, which is synchronous.

        val gleanDataDir = File(applicationContext.applicationInfo.dataDir, Glean.GLEAN_DATA_DIR)

        ensureDirectoryExists(gleanDataDir)

        // The first time Glean runs, we set the client id and other internal
        // one-time only metrics.
        fixLegacyPingInfoMetrics()

        val clientIdMetric = GleanInternalMetrics.clientId
        val existingClientId = UuidsStorageEngine.getSnapshot(
            clientIdMetric.sendInPings[0], false)?.get(clientIdMetric.identifier)
        if (existingClientId == null || existingClientId == KNOWN_CLIENT_ID) {
            val uuid = UUID.randomUUID()
            UuidsStorageEngine.record(clientIdMetric, uuid)
        }

        val firstRunDateMetric = GleanInternalMetrics.firstRunDate
        val existingFirstRunDate = DatetimesStorageEngine.getSnapshot(
            firstRunDateMetric.sendInPings[0], false)?.get(firstRunDateMetric.identifier)
        if (existingFirstRunDate == null) {
            DatetimesStorageEngine.set(firstRunDateMetric)
        }

        // Set a few more metrics that will be sent as part of every ping.
        StringsStorageEngine.record(GleanBaseline.locale, getLocaleTag())
        StringsStorageEngine.record(GleanInternalMetrics.os, "Android")
        // https://developer.android.com/reference/android/os/Build.VERSION
        StringsStorageEngine.record(GleanInternalMetrics.androidSdkVersion, Build.VERSION.SDK_INT.toString())
        StringsStorageEngine.record(GleanInternalMetrics.osVersion, Build.VERSION.RELEASE)
        // https://developer.android.com/reference/android/os/Build
        StringsStorageEngine.record(GleanInternalMetrics.deviceManufacturer, Build.MANUFACTURER)
        StringsStorageEngine.record(GleanInternalMetrics.deviceModel, Build.MODEL)
        StringsStorageEngine.record(GleanInternalMetrics.architecture, Build.SUPPORTED_ABIS[0])

        configuration.channel?.let {
            StringsStorageEngine.record(GleanInternalMetrics.appChannel, it)
        }

        try {
            val packageInfo = applicationContext.packageManager.getPackageInfo(
                applicationContext.packageName, 0
            )
            @Suppress("DEPRECATION")
            StringsStorageEngine.record(
                GleanInternalMetrics.appBuild,
                packageInfo.versionCode.toString()
            )
            StringsStorageEngine.record(
                GleanInternalMetrics.appDisplayVersion,
                packageInfo.versionName?.let { it } ?: "Unknown"
            )
        } catch (e: PackageManager.NameNotFoundException) {
            logger.error("Could not get own package info, unable to report build id and display version")
            throw AssertionError("Could not get own package info, aborting init")
        }
    }

    /**
     * Sanitizes the application id, generating a pipeline-friendly string that replaces
     * non alphanumeric characters with dashes.
     *
     * @param applicationId the string representing the application id
     *
     * @return the sanitized version of the application id
     */
    internal fun sanitizeApplicationId(applicationId: String): String {
        return applicationId.replace("[^a-zA-Z0-9]+".toRegex(), "-")
    }

    /**
     * Handle the background event and send the appropriate pings.
     */
    fun handleBackgroundEvent() {
        // Schedule the baseline and event pings
        sendPings(listOf(Pings.baseline, Pings.events))
    }

    /**
     * Send a list of pings.
     *
     * The ping content is assembled as soon as possible, but upload is not
     * guaranteed to happen immediately, as that depends on the upload
     * policies.
     *
     * If the ping currently contains no content, it will not be sent.
     *
     * @param pings List of pings to send.
     * @return The async Job performing the work of assembling the ping
     */
    @Suppress("EXPERIMENTAL_API_USAGE")
    internal fun sendPings(pings: List<PingType>) = Dispatchers.API.launch {
        if (!isInitialized()) {
            logger.error("Glean must be initialized before sending pings.")
            return@launch
        }

        if (!uploadEnabled) {
            logger.error("Glean must be enabled before sending pings.")
            return@launch
        }

        val pingSerializationTasks = mutableListOf<Job>()
        for (ping in pings) {
            assembleAndSerializePing(ping)?.let {
                pingSerializationTasks.add(it)
            } ?: logger.debug("No content for ping '$ping.name', therefore no ping queued.")
        }

        // If any ping is being serialized to disk, wait for the to finish before spinning up
        // the WorkManager upload job.
        if (pingSerializationTasks.any()) {
            // Await the serialization tasks. Once the serialization tasks have all completed,
            // we can then safely enqueue the PingUploadWorker.
            pingSerializationTasks.joinAll()
            PingUploadWorker.enqueueWorker()
        }
    }

    /**
     * Send a list of pings by name.
     *
     * Each ping will be looked up in the known instances of [PingType]. If the
     * ping isn't known, an error is logged and the ping isn't queued for uploading.
     *
     * The ping content is assembled as soon as possible, but upload is not
     * guaranteed to happen immediately, as that depends on the upload
     * policies.
     *
     * If the ping currently contains no content, it will not be sent.
     *
     * @param pingNames List of ping names to send.
     * @return The async Job performing the work of assembling the ping
     */
    internal fun sendPingsByName(pingNames: List<String>): Job? {
        val pings = pingNames.mapNotNull { pingName ->
            PingType.pingRegistry.get(pingName)?.let {
                it
            } ?: run {
                logger.error("Attempted to send unknown ping '$pingName'")
                null
            }
        }
        return sendPings(pings)
    }

    /**
     * Collect and assemble the ping and serialize the ping to be read when uploaded, but only if
     * Glean is initialized, upload is enabled, and there is ping data to send.
     *
     * @param ping This is the object describing the ping
     */
    internal fun assembleAndSerializePing(ping: PingType): Job? {
        // Since the pingMaker.collect() function returns null if there is nothing to send we can
        // use this to avoid sending an empty ping
        return pingMaker.collect(ping)?.let { pingContent ->
            // Store the serialized ping to file for PingUploadWorker to read and upload when the
            // schedule is triggered
            val pingId = UUID.randomUUID()
            pingStorageEngine.store(pingId, makePath(ping.name, pingId), pingContent)
        }
    }

    /**
     * TEST ONLY FUNCTION.
     * This is called by the GleanTestRule, to enable test mode.
     *
     * This makes all asynchronous work synchronous so we can test the results of the
     * API synchronously.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    internal fun enableTestingMode() {
        @Suppress("EXPERIMENTAL_API_USAGE")
        Dispatchers.API.setTestingMode(enabled = true)
    }

    /**
     * TEST ONLY FUNCTION.
     * Resets the Glean state and trigger init again.
     *
     * @param context the application context to init Glean with
     * @param config the [Configuration] to init Glean with
     * @param clearStores if true, clear the contents of all stores
     */
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    internal fun resetGlean(
        context: Context,
        config: Configuration,
        clearStores: Boolean
    ) {
        Glean.enableTestingMode()

        if (clearStores) {
            // Clear all the stored data.
            val storageManager = StorageEngineManager(applicationContext = context)
            storageManager.clearAllStores()
            // The experiments storage engine needs to be cleared manually as it's not listed
            // in the `StorageEngineManager`.
            ExperimentsStorageEngine.clearAllStores()
        }

        // Init Glean.
        Glean.initialized = false
        Glean.setUploadEnabled(true)
        Glean.initialize(context, config)
    }
}

@SuppressLint("StaticFieldLeak")
object Glean : GleanInternalAPI() {
    internal const val SCHEMA_VERSION = 1

    /**
     * The name of the directory, inside the application's directory,
     * in which Glean files are stored.
     */
    internal const val GLEAN_DATA_DIR = "glean_data"
}
