/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import android.arch.lifecycle.ProcessLifecycleOwner
import android.content.Context
import android.content.pm.PackageManager
import android.support.annotation.VisibleForTesting
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch

import java.util.UUID

import mozilla.components.service.glean.config.Configuration
import mozilla.components.service.glean.firstrun.FileFirstRunDetector
import mozilla.components.service.glean.GleanMetrics.GleanInternalMetrics
import mozilla.components.service.glean.net.HttpPingUploader
import mozilla.components.service.glean.ping.PingMaker
import mozilla.components.service.glean.scheduler.GleanLifecycleObserver
import mozilla.components.service.glean.storages.ExperimentsStorageEngine
import mozilla.components.service.glean.storages.StorageEngineManager
import mozilla.components.service.glean.ping.BaselinePing
import mozilla.components.service.glean.scheduler.MetricsPingScheduler
import mozilla.components.support.base.log.logger.Logger
import java.io.File

@Suppress("TooManyFunctions")
open class GleanInternalAPI internal constructor () {
    private val logger = Logger("glean/Glean")

    // Include our singletons of StorageEngineManager and PingMaker
    private lateinit var storageEngineManager: StorageEngineManager
    private lateinit var pingMaker: PingMaker
    internal lateinit var configuration: Configuration
    // `internal` so this can be modified for testing
    internal lateinit var httpPingUploader: HttpPingUploader

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
        httpPingUploader = HttpPingUploader(configuration)
        initialized = true
        applicationId = sanitizeApplicationId(applicationContext.packageName)

        initializeCoreMetrics(applicationContext)

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

    @VisibleForTesting
    internal fun clearExperiments() {
        ExperimentsStorageEngine.clearAllStores()
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun makePath(docType: String, uuid: UUID): String {
        return "/submit/$applicationId/$docType/${Glean.SCHEMA_VERSION}/$uuid"
    }

    /**
     * Collect and assemble the ping. Asynchronously submits the assembled
     * payload to the designated server using [httpPingUploader] but only
     * if upload is enabled, and there is ping data to send.
     */
    internal fun sendPing(store: String, docType: String) {
        if (!isInitialized()) {
            logger.error("Attempted to send ping before Glean was initialized.")
            return
        }

        // Do not send ping if upload is disabled
        if (!getUploadEnabled()) {
            logger.debug("Attempt to send ping \"$store\" but metrics disabled, aborting send.")
            return
        }

        // Build and send the ping on an async thread. Since the pingMaker.collect() function
        // returns null if there is nothing to send we can use this to avoid sending an empty ping
        pingMaker.collect(store)?.let { pingContent ->
            val uuid = UUID.randomUUID()
            val path = makePath(docType, uuid)
            // Asynchronously perform the HTTP upload off the main thread.
            GlobalScope.launch(Dispatchers.IO) {
                httpPingUploader.upload(path = path, data = pingContent)
            }
        }
    }

    /**
     * Initialize the core metrics internally managed by Glean (e.g. client id).
     */
    private fun initializeCoreMetrics(applicationContext: Context) {
        val gleanDataDir = File(applicationContext.applicationInfo.dataDir, Glean.GLEAN_DATA_DIR)

        // Make sure the data directory exists and is writable.
        if (!gleanDataDir.exists() && !gleanDataDir.mkdirs()) {
            logger.error("Failed to create Glean's data dir ${gleanDataDir.absolutePath}")
            return
        }

        if (!gleanDataDir.isDirectory || !gleanDataDir.canWrite()) {
            logger.error("Glean's data directory is not a writable directory ${gleanDataDir.absolutePath}")
            return
        }

        // The first time Glean runs, we set the client id and other internal
        // one-time only metrics.
        val firstRunDetector = FileFirstRunDetector(gleanDataDir)
        if (firstRunDetector.isFirstRun()) {
            GleanInternalMetrics.clientId.generateAndSet()
        }

        try {
            val packageInfo = applicationContext.packageManager.getPackageInfo(
                applicationContext.packageName, 0
            )
            GleanInternalMetrics.appBuild.set(packageInfo.versionCode.toString())
            packageInfo.versionName?.let {
                GleanInternalMetrics.appDisplayVersion.set(it)
            }
        } catch (e: PackageManager.NameNotFoundException) {
            logger.error("Could not get own package info, unable to report build id and display version")
            throw AssertionError("Could not get own package info, aborting init")
        }

        // Set up information and scheduling for glean owned pings
        metricsPingScheduler = MetricsPingScheduler(applicationContext)
        baselinePing = BaselinePing(applicationContext)
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
     * Handle an event and send the appropriate pings.
     *
     * Valid events are:
     *
     *   - Background: When the application goes to the background.
     *       This triggers sending of the baseline ping.
     *   - Default: Event that triggers the default pings.
     *
     * @param pingEvent The type of the event.
     */
    fun handleEvent(pingEvent: Glean.PingEvent) {
        if (!isInitialized()) {
            logger.error("Glean must be initialized before handling events.")
            return
        }

        when (pingEvent) {
            Glean.PingEvent.Background -> {
                sendPing(BaselinePing.STORE_NAME, BaselinePing.STORE_NAME)
                sendPing("events", "events")
            }
            Glean.PingEvent.Default -> {
                if (metricsPingScheduler.canSendPing()) {
                    sendPing(MetricsPingScheduler.STORE_NAME, MetricsPingScheduler.STORE_NAME)
                    metricsPingScheduler.updateSentTimestamp()
                }
            }
        }
    }
}

object Glean : GleanInternalAPI() {
    /**
    * Enumeration of different metric lifetimes.
    */
    enum class PingEvent {
        /**
        * When the application goes into the background
        */
        Background,
        /**
        * A periodic event to send the default ping
        */
        Default
    }

    internal const val SCHEMA_VERSION = 1

    /**
    * The name of the directory, inside the application's directory,
    * in which Glean files are stored.
    */
    internal const val GLEAN_DATA_DIR = "glean_data"
}
