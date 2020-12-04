/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.nimbus

import android.content.Context
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import androidx.annotation.VisibleForTesting
import androidx.core.content.pm.PackageInfoCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.launch
import mozilla.components.service.glean.Glean
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import mozilla.components.support.locale.getLocaleTag
import org.mozilla.experiments.nimbus.AppContext
import org.mozilla.experiments.nimbus.AvailableRandomizationUnits
import org.mozilla.experiments.nimbus.EnrolledExperiment
import org.mozilla.experiments.nimbus.ErrorException
import org.mozilla.experiments.nimbus.NimbusClient
import org.mozilla.experiments.nimbus.NimbusClientInterface
import org.mozilla.experiments.nimbus.RemoteSettingsConfig
import java.io.File
import java.util.Locale
import java.util.concurrent.Executors

private const val LOG_TAG = "service/Nimbus"
private const val EXPERIMENT_BUCKET_NAME = "main"
private const val EXPERIMENT_COLLECTION_NAME = "nimbus-mobile-experiments"
private const val NIMBUS_DATA_DIR: String = "nimbus_data"

/**
 * This is the main experiments API, which is exposed through the global [Nimbus] object.
 */
interface NimbusApi : Observable<NimbusApi.Observer> {
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
     * Refreshes the experiments from the endpoint. Should be called at least once after
     * initialization
     */
    fun updateExperiments()

    /**
     * Opt out of a specific experiment
     *
     * @param experimentId The string experiment-id or "slug" for which to opt out of
     */
    fun optOut(experimentId: String)

    /**
     * Control the opt out for all experiments at once. This is likely a user action.
     */
    var globalUserParticipation: Boolean

    /**
     * Interface to be implemented by classes that want to observe experiment updates
     */
    interface Observer {
        /**
         * Event to indicate that the experiments have been fetched from the endpoint
         */
        fun onExperimentsFetched()

        /**
         * Event to indicate that the experiment enrollments have been applied. Multiple calls to
         * get the active experiments will return the same value so this has limited usefulness for
         * most feature developers
         */
        fun onUpdatesApplied(updated: List<EnrolledExperiment>)
    }
}

/**
 * This class allows client apps to configure Nimbus to point to your own server.
 * Client app developers should set up their own Nimbus infrastructure, to avoid different
 * organizations running conflicting experiments or hitting servers with extra network traffic.
 */
data class NimbusServerSettings(
    val url: Uri
)

/**
 * A implementation of the [NimbusApi] interface backed by the Nimbus SDK.
 */
class Nimbus(
    context: Context,
    server: NimbusServerSettings?,
    private val delegate: Observable<NimbusApi.Observer> = ObserverRegistry()
) : NimbusApi, Observable<NimbusApi.Observer> by delegate {

    // Using a single threaded executor here to enforce synchronization where needed.
    private val scope: CoroutineScope =
        CoroutineScope(Executors.newSingleThreadExecutor().asCoroutineDispatcher())

    private var nimbus: NimbusClientInterface
    private var dataDir: File
    private val logger = Logger(LOG_TAG)

    override var globalUserParticipation: Boolean
        get() = nimbus.getGlobalUserParticipation()
        set(active) = nimbus.setGlobalUserParticipation(active)

    init {
        // Set the name of the native library so that we use
        // the appservices megazord for compiled code.
        System.setProperty(
            "uniffi.component.nimbus.libraryOverride",
            System.getProperty("mozilla.appservices.megazord.library", "megazord")
        )
        // Build a File object to represent the data directory for Nimbus data
        dataDir = File(context.applicationInfo.dataDir, NIMBUS_DATA_DIR)

        // Build Nimbus AppContext object to pass into initialize
        val experimentContext = buildExperimentContext(context)

        // Build a File object to represent the data directory for Nimbus data
        val dataDir = File(context.applicationInfo.dataDir, NIMBUS_DATA_DIR)

        // Initialize Nimbus
        val remoteSettingsConfig = server?.let {
            RemoteSettingsConfig(
                serverUrl = server.url.toString(),
                bucketName = EXPERIMENT_BUCKET_NAME,
                collectionName = EXPERIMENT_COLLECTION_NAME
            )
        }
        // We'll temporarily use this until the Nimbus SDK supports null servers
        // https://github.com/mozilla/nimbus-sdk/pull/66 is landed, so this is the next release of
        // Nimbus SDK.
        ?: RemoteSettingsConfig(
            serverUrl = "https://settings.stage.mozaws.net",
            bucketName = EXPERIMENT_BUCKET_NAME,
            collectionName = EXPERIMENT_COLLECTION_NAME
        )

        nimbus = NimbusClient(
            experimentContext,
            dataDir.path,
            remoteSettingsConfig,
            // The "dummy" field here is required for obscure reasons when generating code on desktop,
            // so we just automatically set it to a dummy value.
            AvailableRandomizationUnits(clientId = null, dummy = 0)
        )
    }

    override fun getActiveExperiments(): List<EnrolledExperiment> =
        nimbus.getActiveExperiments()

    override fun getExperimentBranch(experimentId: String): String? =
        nimbus.getExperimentBranch(experimentId)

    override fun updateExperiments() {
        scope.launch {
            try {
                nimbus.updateExperiments()

                // Get the experiments to record in telemetry
                nimbus.getActiveExperiments().let {
                    if (it.any()) {
                        recordExperimentTelemetry(it)
                        // The current plan is to have the nimbus-sdk updateExperiments() function
                        // return a diff of the experiments that have been received, at which point we
                        // can emit the appropriate telemetry events and notify observers of just the
                        // diff
                        notifyObservers { onUpdatesApplied(it) }
                    }
                }
            } catch (e: ErrorException.RequestError) {
                logger.info("Error fetching experiments from endpoint: $e")
            } catch (e: ErrorException.InvalidExperimentResponse) {
                logger.info("Invalid experiment response: $e")
            }
        }
    }

    override fun optOut(experimentId: String) {
        nimbus.optOut(experimentId)
    }

    // This function shouldn't be exposed to the public API, but is meant for testing purposes to
    // force an experiment/branch enrollment.
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    internal fun optInWithBranch(experiment: String, branch: String) {
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
