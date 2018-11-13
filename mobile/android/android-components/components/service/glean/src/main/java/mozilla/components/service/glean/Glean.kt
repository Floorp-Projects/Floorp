/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import android.content.Context
import android.support.annotation.VisibleForTesting

import java.util.UUID

import mozilla.components.service.glean.config.Configuration
import mozilla.components.service.glean.net.HttpPingUploader
import mozilla.components.service.glean.ping.PingMaker
import mozilla.components.service.glean.storages.ExperimentsStorageEngine
import mozilla.components.service.glean.storages.StorageEngineManager
import mozilla.components.support.base.log.logger.Logger

object Glean {
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

    private val logger = Logger("glean/Glean")

    internal const val SCHEMA_VERSION = 1

    // Include our singletons of StorageEngineManager and PingMaker
    private lateinit var storageEngineManager: StorageEngineManager
    private lateinit var pingMaker: PingMaker
    private lateinit var configuration: Configuration
    // `internal` so this can be modified for testing
    internal lateinit var httpPingUploader: HttpPingUploader

    private var initialized = false
    private var metricsEnabled = true

    /**
     * Initialize glean.
     */
    fun initialize(applicationContext: Context, configuration: Configuration) {
        storageEngineManager = StorageEngineManager(applicationContext = applicationContext)
        pingMaker = PingMaker(storageEngineManager)
        this.configuration = configuration
        httpPingUploader = HttpPingUploader(configuration)
        initialized = true
    }

    /**
     * Enable or disable metric collection.
     *
     * Metric collection is emabled by default.
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
        return "/submit/glean/$docType/$SCHEMA_VERSION/$uuid"
    }

    private fun sendPing(store: String, docType: String) {
        val pingContent = pingMaker.collect(store)
        val uuid = UUID.randomUUID()
        val path = makePath(docType, uuid)
        this.httpPingUploader.upload(pingContent, path)
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
    fun handleEvent(pingEvent: PingEvent) {
        when (pingEvent) {
            PingEvent.Background -> sendPing("baseline", "baseline")
            PingEvent.Default -> sendPing("metrics", "metrics")
        }
    }
}
