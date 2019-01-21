/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import android.view.accessibility.AccessibilityManager
import android.accessibilityservice.AccessibilityServiceInfo
import android.arch.lifecycle.ProcessLifecycleOwner
import android.content.Context
import android.os.Build
import android.support.annotation.VisibleForTesting
import android.support.v4.view.accessibility.AccessibilityManagerCompat.getEnabledAccessibilityServiceList
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch

import java.util.UUID

import mozilla.components.service.glean.config.Configuration
import mozilla.components.service.glean.firstrun.FileFirstRunDetector
import mozilla.components.service.glean.metrics.GleanInternalMetrics
import mozilla.components.service.glean.net.HttpPingUploader
import mozilla.components.service.glean.ping.PingMaker
import mozilla.components.service.glean.scheduler.GleanLifecycleObserver
import mozilla.components.service.glean.storages.ExperimentsStorageEngine
import mozilla.components.service.glean.storages.StorageEngineManager
import mozilla.components.service.glean.metrics.Baseline
import mozilla.components.service.glean.utils.StringUtils
import mozilla.components.support.base.log.logger.Logger
import java.io.File

@Suppress("TooManyFunctions")
open class GleanInternalAPI {
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
    private var metricsEnabled = true

    // The application id detected by glean to be used as part of the submission
    // endpoint.
    internal lateinit var applicationId: String

    /**
     * Initialize glean.
     *
     * A LifecycleObserver will be added to send pings when the application goes
     * into the background.
     *
     * @param applicationContext [Context] to access application features, such
     * as shared preferences
     * @param configuration A Glean [Configuration] object with global settings.
     * @raises Exception if called more than once
     */
    fun initialize(
        applicationContext: Context,
        configuration: Configuration = Configuration()
    ) {
        if (isInitialized()) {
            throw IllegalStateException("Glean may not be initialized multiple times")
        }

        storageEngineManager = StorageEngineManager(applicationContext = applicationContext)
        pingMaker = PingMaker(storageEngineManager)
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
     * Enable or disable metric collection.
     *
     * Metric collection is enabled by default.
     *
     * When disabled, metrics aren't recorded at all.
     *
     * @param enabled When true, enable metric collection.
     */
    fun setMetricsEnabled(enabled: Boolean) {
        logger.info("Metrics enabled: $enabled")
        metricsEnabled = enabled
    }

    /**
     * Get the status of metrics enabled.
     */
    fun getMetricsEnabled(): Boolean {
        return metricsEnabled
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
     * if metrics are enabled, and there is ping data to send.
     */
    internal fun sendPing(store: String, docType: String) {
        // Do not send ping if metrics disabled
        if (!getMetricsEnabled()) {
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
     * Records a list of the currently enabled accessibility services.
     *
     * https://developer.android.com/reference/android/view/accessibility/AccessibilityManager.html
     * @param accessibilityManager The system's [AccessibilityManager] as
     * returned from applicationContext.getSystemService
     * @returns services A list of ids of the enabled accessibility services. If
     *     the accessibility manager is disabled, returns null.
     */
    internal fun getEnabledAccessibilityServices(
        accessibilityManager: AccessibilityManager
    ): List<String>? {
        if (!accessibilityManager.isEnabled) {
            logger.info("AccessibilityManager is disabled")
            return null
        }
        return accessibilityManager.getEnabledAccessibilityServiceList(
            AccessibilityServiceInfo.FEEDBACK_ALL_MASK
        ).mapNotNull {
            // Note that any reference in java code can be null, so we'd better
            // check for null values here as well.
            it.id
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

        // Set the OS type
        Baseline.os.set("Android")

        // Set the OS version
        // https://developer.android.com/reference/android/os/Build.VERSION
        Baseline.osVersion.set(Build.VERSION.SDK_INT.toString())

        // Set the device string
        // https://developer.android.com/reference/android/os/Build
        // We limit the device descriptor to 32 characters because it can get long. We give fewer
        // characters to the manufacturer because we're less likely to have manufacturers with
        // similar names than we are for a manufacturer to have two devices with the similar names
        // (e.g. Galaxy S6 vs. Galaxy Note 6).
        @Suppress("MagicNumber")
        Baseline.device.set(
            StringUtils.safeSubstring(Build.MANUFACTURER, 0, 12) +
            '-' +
            StringUtils.safeSubstring(Build.MODEL, 0, 19)
        )

        // Set the CPU architecture
        Baseline.architecture.set(Build.SUPPORTED_ABIS[0])

        // Set the enabled accessibility services
        getEnabledAccessibilityServices(
            applicationContext.getSystemService(Context.ACCESSIBILITY_SERVICE) as AccessibilityManager
        ) ?.let {
            Baseline.a11yServices.set(it)
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
                sendPing("baseline", "baseline")
                sendPing("events", "events")
            }
            Glean.PingEvent.Default -> sendPing("metrics", "metrics")
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
