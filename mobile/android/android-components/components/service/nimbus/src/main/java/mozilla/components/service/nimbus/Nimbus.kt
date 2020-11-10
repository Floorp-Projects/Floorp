/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.nimbus

import android.content.Context
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import android.os.Build
import androidx.annotation.VisibleForTesting
import androidx.core.content.pm.PackageInfoCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.launch
import mozilla.components.service.glean.Glean
import mozilla.components.support.base.log.Log
import mozilla.components.support.locale.getLocaleTag
import org.mozilla.experiments.nimbus.AppContext
import org.mozilla.experiments.nimbus.AvailableRandomizationUnits
import org.mozilla.experiments.nimbus.EnrolledExperiment
import org.mozilla.experiments.nimbus.NimbusClient
import org.mozilla.experiments.nimbus.RemoteSettingsConfig
import java.io.File
import java.util.Locale
import java.util.concurrent.Executors

/**
 * This is the main experiments API, which is exposed through the global [Nimbus] object.
 */
internal interface NimbusApi {
    /**
     * Get the list of currently enrolled experiments
     *
     * @return A list of [EnrolledExperiment]s
     */
    fun getActiveExperiments(): List<EnrolledExperiment>

    /**
     * Get the currently enrolled branch for the given experiment
     *
     * @param experimentId The string experiment-id or "slug" for which to retrieve the branch
     *
     * @return A String representing the branch-id or "slug"
     */
    fun getExperimentBranch(experimentId: String): String?

    /**
     * Refreshes the experiments from the endpoint
     */
    fun updateExperiments()

    /**
     * Opt out of a specific experiment
     *
     * @param experimentId The string experiment-id or "slug" for which to opt out of
     */
    fun optOut(experimentId: String)

    /**
     * Set global opt-in/out for all experiments
     *
     * @param active Set `true` to enable experiment enrollment or `false` to unenroll from all
     */
    fun setGlobalUserParticipation(active: Boolean)

    /**
     * Get current global opt-in/out state for all experiments
     *
     * @return the current state of the global participation, or null if not initialized
     */
    fun getGlobalUserParticipation(): Boolean?
}

/**
 * A singleton implementation of the [NimbusApi] interface backed by the Nimbus SDK.
 */
object Nimbus : NimbusApi {
    private const val LOG_TAG = "service/Nimbus"
    private const val EXPERIMENT_BUCKET_NAME = "main"
    private const val EXPERIMENT_COLLECTION_NAME = "nimbus-mobile-experiments"
    private const val NIMBUS_DATA_DIR: String = "nimbus_data"

    // Using a single threaded executor here to enforce synchronization where needed.
    private val scope: CoroutineScope =
        CoroutineScope(Executors.newSingleThreadExecutor().asCoroutineDispatcher())

    private lateinit var nimbus: NimbusClient
    private lateinit var dataDir: File
    private var onExperimentUpdated: ((List<EnrolledExperiment>) -> Unit)? = null

    private var isInitialized = false

    /**
     * Initialize the Nimbus SDK library.
     *
     * This should only be initialized once by the application.
     *
     * @param context [Context] to access application and device parameters.  As we cannot enforce
     * through the compiler that the context pass to the initialize function is a Application
     * Context, there could potentially be a memory leak if the initializing application doesn't
     * comply.
     *
     * @param onExperimentUpdated callback that will be executed when the list of experiments has
     * been updated from the experiments endpoint and evaluated by the Nimbus-SDK. This is meant to
     * be used for consuming applications to perform any actions as a result of enrollment in an
     * experiment so that the application is not required to await the network request. The
     * callback will be supplied with the list of active experiments (if any) for which the client
     * is enrolled.
     */
    fun init(
        context: Context,
        onExperimentUpdated: ((activeExperiments: List<EnrolledExperiment>) -> Unit)? = null
    ) {
        // Set the name of the native library so that we use
        // the appservices megazord for compiled code.
        System.setProperty(
            "uniffi.component.nimbus.libraryOverride",
            System.getProperty("mozilla.appservices.megazord.library", "megazord")
        )

        this.onExperimentUpdated = onExperimentUpdated

        // Do initialization off of the main thread
        scope.launch {
            // Build Nimbus AppContext object to pass into initialize
            val experimentContext = buildExperimentContext(context)

            // Build a File object to represent the data directory for Nimbus data
            dataDir = File(context.applicationInfo.dataDir, NIMBUS_DATA_DIR)

            // Initialize Nimbus
            nimbus = NimbusClient(
                experimentContext,
                dataDir.path,
                RemoteSettingsConfig(
                    serverUrl = context.resources.getString(R.string.nimbus_default_endpoint),
                    bucketName = EXPERIMENT_BUCKET_NAME,
                    collectionName = EXPERIMENT_COLLECTION_NAME
                ),
                // The "dummy" field here is required for obscure reasons when generating code on desktop,
                // so we just automatically set it to a dummy value.
                AvailableRandomizationUnits(clientId = null, dummy = 0)
            )

            // Get experiments
            val activeExperiments = nimbus.getActiveExperiments()

            // Record enrollments in telemetry
            recordExperimentTelemetry(activeExperiments)

            isInitialized = true

            // Invoke the callback with the list of active experiments for the consuming app to
            // process.
            onExperimentUpdated?.invoke(activeExperiments)
        }
    }

    override fun getActiveExperiments(): List<EnrolledExperiment> =
        if (isInitialized) { nimbus.getActiveExperiments() } else { emptyList() }

    override fun getExperimentBranch(experimentId: String): String? =
        if (isInitialized) { nimbus.getExperimentBranch(experimentId) } else { null }

    override fun updateExperiments() {
        if (!isInitialized) return
        nimbus.updateExperiments()
        onExperimentUpdated?.invoke(nimbus.getActiveExperiments())
    }

    override fun optOut(experimentId: String) {
        if (!isInitialized) return
        nimbus.optOut(experimentId)
    }

    override fun setGlobalUserParticipation(active: Boolean) {
        if (!isInitialized) return
        nimbus.setGlobalUserParticipation(active)
    }

    override fun getGlobalUserParticipation(): Boolean? {
        if (!isInitialized) return null
        return nimbus.getGlobalUserParticipation()
    }

    // This function shouldn't be exposed to the public API, but is meant for testing purposes to
    // force an experiment/branch enrollment.
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    internal fun optInWithBranch(experiment: String, branch: String) {
        if (!isInitialized) return
        nimbus.optInWithBranch(experiment, branch)
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun recordExperimentTelemetry(experiments: List<EnrolledExperiment>) {
        // Call Glean.setExperimentActive() for each active experiment.
        experiments.forEach {
            // For now, we will just record the experiment id and the branch id. Once we can call
            // Glean from Rust, this will move to the nimbus-sdk Rust core.
            Glean.setExperimentActive(it.slug, it.branchSlug)
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun buildExperimentContext(context: Context): AppContext {
        val packageInfo: PackageInfo? = try {
            context.packageManager.getPackageInfo(
                context.packageName, 0
            )
        } catch (e: PackageManager.NameNotFoundException) {
            Log.log(Log.Priority.ERROR,
                LOG_TAG,
                message = "Could not retrieve package info for appBuild and appVersion"
            )
            null
        }

        return AppContext(
                appId = context.packageName,
                androidSdkVersion = Build.VERSION.SDK_INT.toString(),
                appBuild = packageInfo?.let { PackageInfoCompat.getLongVersionCode(it).toString() },
                appVersion = packageInfo?.versionName,
                architecture = Build.SUPPORTED_ABIS[0],
                debugTag = null,
                deviceManufacturer = Build.MANUFACTURER,
                deviceModel = Build.MODEL,
                locale = Locale.getDefault().getLocaleTag(),
                os = "Android",
                osVersion = Build.VERSION.RELEASE)
    }
}
