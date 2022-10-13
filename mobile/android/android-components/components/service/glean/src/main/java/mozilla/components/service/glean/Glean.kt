/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import android.content.Context
import androidx.annotation.MainThread
import androidx.annotation.VisibleForTesting
import mozilla.components.service.glean.config.Configuration
import mozilla.components.service.glean.private.RecordedExperiment
import mozilla.telemetry.glean.Glean as GleanCore

typealias BuildInfo = mozilla.telemetry.glean.BuildInfo

/**
 * In contrast with other glean-ac classes (i.e. Configuration), we can't
 * use typealias to export mozilla.telemetry.glean.Glean, as we need to provide
 * a different default [Configuration]. Moreover, we can't simply delegate other
 * methods or inherit, since that doesn't work for `object` in Kotlin.
 */
object Glean {
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
     * @param uploadEnabled A [Boolean] that determines the initial state of the uploader
     * @param configuration A Glean [Configuration] object with global settings.
     * @param buildInfo A Glean [BuildInfo] object with build-time metadata. This
     *     object is generated at build time by glean_parser at the import path
     *     ${YOUR_PACKAGE_ROOT}.GleanMetrics.GleanBuildInfo.buildInfo
     */
    @MainThread
    fun initialize(
        applicationContext: Context,
        uploadEnabled: Boolean,
        configuration: Configuration,
        buildInfo: BuildInfo,
    ) {
        GleanCore.initialize(
            applicationContext = applicationContext,
            uploadEnabled = uploadEnabled,
            configuration = configuration.toWrappedConfiguration(),
            buildInfo = buildInfo,
        )
    }

    /**
     * Register the pings generated from `pings.yaml` with Glean.
     *
     * @param pings The `Pings` object generated for your library or application
     * by Glean.
     */
    fun registerPings(pings: Any) {
        GleanCore.registerPings(pings)
    }

    /**
     * Enable or disable Glean collection and upload.
     *
     * Metric collection is enabled by default.
     *
     * When disabled, metrics aren't recorded at all and no data
     * is uploaded.
     *
     * @param enabled When true, enable metric collection.
     */
    fun setUploadEnabled(enabled: Boolean) {
        GleanCore.setUploadEnabled(enabled)
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
    @JvmOverloads
    fun setExperimentActive(
        experimentId: String,
        branch: String,
        extra: Map<String, String>? = null,
    ) {
        GleanCore.setExperimentActive(
            experimentId = experimentId,
            branch = branch,
            extra = extra,
        )
    }

    /**
     * Indicate that an experiment is no longer running.
     *
     * @param experimentId The id of the experiment to deactivate.
     */
    fun setExperimentInactive(experimentId: String) {
        GleanCore.setExperimentInactive(experimentId = experimentId)
    }

    /**
     * Tests whether an experiment is active, for testing purposes only.
     *
     * @param experimentId the id of the experiment to look for.
     * @return true if the experiment is active and reported in pings, otherwise false
     */
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    fun testIsExperimentActive(experimentId: String): Boolean {
        return GleanCore.testIsExperimentActive(experimentId)
    }

    /**
     * Returns the stored data for the requested active experiment, for testing purposes only.
     *
     * @param experimentId the id of the experiment to look for.
     * @return the [RecordedExperiment] for the experiment
     * @throws [NullPointerException] if the requested experiment is not active or data is corrupt.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    fun testGetExperimentData(experimentId: String): RecordedExperiment {
        return GleanCore.testGetExperimentData(experimentId)
    }
}
