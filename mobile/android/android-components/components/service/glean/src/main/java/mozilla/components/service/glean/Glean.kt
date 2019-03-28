/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import android.arch.lifecycle.ProcessLifecycleOwner
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.support.annotation.VisibleForTesting
import java.io.File
import java.util.UUID

import mozilla.components.service.glean.config.Configuration
import mozilla.components.service.glean.firstrun.FileFirstRunDetector
import mozilla.components.service.glean.GleanMetrics.GleanInternalMetrics
import mozilla.components.service.glean.ping.PingMaker
import mozilla.components.service.glean.scheduler.GleanLifecycleObserver
import mozilla.components.service.glean.ping.BaselinePing
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
import mozilla.components.service.glean.utils.parseISOTimeString
import mozilla.components.support.base.log.logger.Logger

@Suppress("TooManyFunctions")
open class GleanInternalAPI internal constructor () {
    private val logger = Logger("glean/Glean")

    // Include our singletons of StorageEngineManager and PingMaker
    private lateinit var storageEngineManager: StorageEngineManager
    private lateinit var pingMaker: PingMaker
    internal lateinit var configuration: Configuration

    private val gleanLifecycleObserver by lazy { GleanLifecycleObserver() }

    // `internal` so this can be modified for testing
    internal var initialized = false
    private var uploadEnabled = true

    // The application id detected by glean to be used as part of the submission
    // endpoint.
    internal lateinit var applicationId: String

    // This object holds data related to any persistent information about the metrics ping,
    // such as the last time it was sent and the store name
    internal lateinit var metricsPingScheduler: MetricsPingScheduler

    // This object encapsulates initialization and information related to the baseline ping
    internal lateinit var baselinePing: BaselinePing

    internal lateinit var pingStorageEngine: PingStorageEngine

    /**
     * Initialize glean.
     *
     * This should only be initialized once by the application, and not by
     * libraries using glean. A message is logged to error and no changes are made
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
        if (isInitialized()) {
            logger.error("Glean should not be initialized multiple times")
            return
        }

        storageEngineManager = StorageEngineManager(applicationContext = applicationContext)
        pingMaker = PingMaker(storageEngineManager, applicationContext)
        this.configuration = configuration
        applicationId = sanitizeApplicationId(applicationContext.packageName)
        pingStorageEngine = PingStorageEngine(applicationContext)

        // Core metrics are initialized using the engines, without calling the async
        // API. For this reason we're safe to set `initialized = true` right after it.
        initializeCoreMetrics(applicationContext)

        // This must be set before anything that might trigger the sending of pings.
        initialized = true

        // Deal with any pending events so we can start recording new ones
        EventsStorageEngine.onReadyToSendPings(applicationContext)

        // Set up information and scheduling for glean owned pings. Ideally, the "metrics"
        // ping startup check should be performed before any other ping, since it relies
        // on being dispatched to the API context before any other metric.
        metricsPingScheduler = MetricsPingScheduler(applicationContext)
        metricsPingScheduler.startupCheck()

        // Other pings might set some other metrics (i.e. the baseline metrics),
        // so we need to be initialized by now.
        baselinePing = BaselinePing()

        // At this point, all metrics and events can be recorded.
        ProcessLifecycleOwner.get().lifecycle.addObserver(gleanLifecycleObserver)
    }

    /**
     * Returns true if the Glean library has been initialized.
     */
    internal fun isInitialized(): Boolean {
        return initialized
    }

    /**
     * Enable or disable glean collection and upload.
     *
     * Metric collection is enabled by default.
     *
     * When disabled, metrics aren't recorded at all and no data
     * is uploaded.
     *
     * @param enabled When true, enable metric collection.
     */
    fun setUploadEnabled(enabled: Boolean) {
        logger.info("Metrics enabled: $enabled")
        uploadEnabled = enabled
    }

    /**
     * Get whether or not glean is allowed to record and upload data.
     */
    fun getUploadEnabled(): Boolean {
        return uploadEnabled
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
            uuidPingInfoSnapshot?.get("client_id")?.let {
                UuidsStorageEngine.record(GleanInternalMetrics.clientId, it)
            }
        }

        val datetimeClientInfoSnapshot = DatetimesStorageEngine.getSnapshot("glean_client_info", false)
        val newFirstRunDate = datetimeClientInfoSnapshot?.get("first_run_date")
        if (newFirstRunDate == null) {
            val datetimePingInfoSnapshot = DatetimesStorageEngine.getSnapshot("glean_ping_info", false)
            datetimePingInfoSnapshot?.get("first_run_date")?.let {
                parseISOTimeString(it)?.let {
                    DatetimesStorageEngine.set(GleanInternalMetrics.firstRunDate, it)
                }
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
        val firstRunDetector = FileFirstRunDetector(gleanDataDir)
        if (firstRunDetector.isFirstRun()) {
            val uuid = UUID.randomUUID()
            UuidsStorageEngine.record(GleanInternalMetrics.clientId, uuid)
            DatetimesStorageEngine.set(GleanInternalMetrics.firstRunDate)
        } else {
            fixLegacyPingInfoMetrics()
        }

        // Set a few more metrics that will be sent as part of every ping.
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
        sendPings(listOf(BaselinePing.STORE_NAME, "events"))
    }

    /**
     * Send a list of pings by name.
     *
     * Both the ping collection and ping uploading happens asyncronously.
     * If the ping currently contains no content, it will not be sent.
     *
     * @param pingNames List of pings to send.
     * @return true if any pings were actually sent.
     */
    internal fun sendPings(pingNames: List<String>): Boolean {
        if (!isInitialized()) {
            logger.error("Glean must be initialized before sending pings.")
            return false
        }

        if (!uploadEnabled) {
            logger.error("Glean must be enabled before sending pings.")
            return false
        }

        var hasPingContent = false
        for (pingName in pingNames) {
            val hasContent = assembleAndSerializePing(pingName)
            hasPingContent = hasPingContent || hasContent
        }

        // It should only take a single PingUploadWorker to process all queued pings, so we only
        // want to have one scheduled at a time.
        if (hasPingContent) {
            PingUploadWorker.enqueueWorker()
        }

        return hasPingContent
    }

    /**
     * Collect and assemble the ping and serialize the ping to be read when uploaded, but only if
     * glean is initialized, upload is enabled, and there is ping data to send.
     *
     * @param pingName This is the ping store/name for which to build and schedule the ping
     */
    internal fun assembleAndSerializePing(pingName: String): Boolean {
        // Since the pingMaker.collect() function returns null if there is nothing to send we can
        // use this to avoid sending an empty ping
        return pingMaker.collect(pingName)?.also { pingContent ->
            // Store the serialized ping to file for PingUploadWorker to read and upload when the
            // schedule is triggered
            val pingId = UUID.randomUUID()
            pingStorageEngine.store(pingId, makePath(pingName, pingId), pingContent)
        } != null
    }

    /**
     * Should be called from all users of the glean testing API.
     *
     * This makes all asynchronous work synchronous so we can test the results of the
     * API synchronously.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    fun enableTestingMode() {
        Dispatchers.API.enableTestingMode()
    }
}

object Glean : GleanInternalAPI() {
    internal const val SCHEMA_VERSION = 1

    /**
     * The name of the directory, inside the application's directory,
     * in which Glean files are stored.
     */
    internal const val GLEAN_DATA_DIR = "glean_data"
}
