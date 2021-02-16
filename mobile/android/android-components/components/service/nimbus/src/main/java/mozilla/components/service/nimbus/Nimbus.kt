/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.nimbus

import android.content.Context
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import androidx.annotation.AnyThread
import androidx.annotation.RawRes
import androidx.annotation.VisibleForTesting
import androidx.annotation.WorkerThread
import androidx.core.content.pm.PackageInfoCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.launch
import mozilla.components.service.glean.Glean
import mozilla.components.service.nimbus.GleanMetrics.NimbusEvents
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import mozilla.components.support.base.utils.NamedThreadFactory
import mozilla.components.support.locale.getLocaleTag
import org.mozilla.experiments.nimbus.AppContext
import org.mozilla.experiments.nimbus.AvailableRandomizationUnits
import org.mozilla.experiments.nimbus.EnrolledExperiment
import org.mozilla.experiments.nimbus.EnrollmentChangeEvent
import org.mozilla.experiments.nimbus.EnrollmentChangeEventType
import org.mozilla.experiments.nimbus.NimbusErrorException
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
    fun getActiveExperiments(): List<EnrolledExperiment> = listOf()

    /**
     * Get the currently enrolled branch for the given experiment
     *
     * @param experimentId The string experiment-id or "slug" for which to retrieve the branch
     *
     * @return A String representing the branch-id or "slug"
     */
    @AnyThread
    fun getExperimentBranch(experimentId: String): String? = null

    /**
     * Refreshes the experiments from the endpoint. Should be called at least once after
     * initialization
     */
    @Deprecated("Use fetchExperiments() and applyPendingExperiments()")
    fun updateExperiments() = Unit

    /**
     * Open the database and populate the SDK so as make it usable by feature developers.
     *
     * This performs the minimum amount of I/O needed to ensure `getExperimentBranch()` is usable.
     *
     * It will not take in to consideration previously fetched experiments: `applyPendingExperiments()`
     * is more suitable for that use case.
     *
     * This method uses the single threaded worker scope, so callers can safely sequence calls to
     * `initialize` and `setExperimentsLocally`, `applyPendingExperiments`.
     */
    fun initialize() = Unit

    /**
     * Fetches experiments from the RemoteSettings server.
     *
     * This is performed on a background thread.
     *
     * Notifies `onExperimentsFetched` to observers once the experiments has been fetched from the
     * server.
     *
     * Notes:
     * * this does not affect experiment enrolment, until `applyPendingExperiments` is called.
     * * this will overwrite pending experiments previously fetched with this method, or set with
     *   `setExperimentsLocally`.
     */
    fun fetchExperiments() = Unit

    /**
     * Calculates the experiment enrolment from experiments from the last `fetchExperiments` or
     * `setExperimentsLocally`, and then informs Glean of new experiment enrolment.
     *
     * Notifies `onUpdatesApplied` once enrolments are recalculated.
     */
    fun applyPendingExperiments() = Unit

    /**
     * Set the experiments as the passed string, just as `fetchExperiments` gets the string from
     * the server. Like `fetchExperiments`, this requires `applyPendingExperiments` to be called
     * before enrolments are affected.
     *
     * The string should be in the same JSON format that is delivered from the server.
     *
     * This is performed on a background thread.
     */
    fun setExperimentsLocally(payload: String) = Unit

    /**
     * A utility method to load a file from resources and pass it to `setExperimentsLocally(String)`.
     */
    fun setExperimentsLocally(@RawRes file: Int) = Unit

    /**
     * Opt out of a specific experiment
     *
     * @param experimentId The string experiment-id or "slug" for which to opt out of
     */
    fun optOut(experimentId: String) = Unit

    /**
     *  Reset internal state in response to application-level telemetry reset.
     *  Consumers should call this method when the user resets the telemetry state of the
     *  consuming application, such as by opting out of (or in to) submitting telemetry.
     */
    fun resetTelemetryIdentifiers() = Unit

    /**
     * Control the opt out for all experiments at once. This is likely a user action.
     */
    var globalUserParticipation: Boolean
        get() = false
        set(_) = Unit

    /**
     * Interface to be implemented by classes that want to observe experiment updates
     */
    interface Observer {
        /**
         * Event to indicate that the experiments have been fetched from the endpoint
         */
        fun onExperimentsFetched() = Unit

        /**
         * Event to indicate that the experiment enrollments have been applied. Multiple calls to
         * get the active experiments will return the same value so this has limited usefulness for
         * most feature developers
         */
        fun onUpdatesApplied(updated: List<EnrolledExperiment>) = Unit
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

private val logger = Logger(LOG_TAG)
private typealias ErrorReporter = (e: Throwable) -> Unit

private val loggingErrorReporter: ErrorReporter = { e: Throwable ->
    logger.error("Error calling rust", e)
}

/**
 * A implementation of the [NimbusApi] interface backed by the Nimbus SDK.
 */
@Suppress("TooManyFunctions", "LargeClass")
class Nimbus(
    private val context: Context,
    server: NimbusServerSettings?,
    private val delegate: Observable<NimbusApi.Observer> = ObserverRegistry(),
    private val errorReporter: ErrorReporter = loggingErrorReporter
) : NimbusApi, Observable<NimbusApi.Observer> by delegate {

    // Using two single threaded executors here to enforce synchronization where needed:
    // An I/O scope is used for reading or writing from the Nimbus's RKV database.
    private val dbScope: CoroutineScope = CoroutineScope(Executors.newSingleThreadExecutor(
        NamedThreadFactory("NimbusDBScope")
    ).asCoroutineDispatcher())

    // An I/O scope is used for getting experiments from the network.
    private val fetchScope: CoroutineScope = CoroutineScope(Executors.newSingleThreadExecutor(
        NamedThreadFactory("NimbusFetchScope")
    ).asCoroutineDispatcher())

    private val nimbus: NimbusClientInterface

    override var globalUserParticipation: Boolean
        get() = nimbus.getGlobalUserParticipation()
        set(active) {
            dbScope.launch {
                setGlobalUserParticipationOnThisThread(active)
            }
        }

    init {
        // Set the name of the native library so that we use
        // the appservices megazord for compiled code.
        System.setProperty(
            "uniffi.component.nimbus.libraryOverride",
            System.getProperty("mozilla.appservices.megazord.library", "megazord")
        )
        // Build a File object to represent the data directory for Nimbus data
        val dataDir = File(context.applicationInfo.dataDir, NIMBUS_DATA_DIR)

        // Build Nimbus AppContext object to pass into initialize
        val experimentContext = buildExperimentContext(context)

        // Initialize Nimbus
        val remoteSettingsConfig = server?.let {
            RemoteSettingsConfig(
                serverUrl = server.url.toString(),
                bucketName = EXPERIMENT_BUCKET_NAME,
                collectionName = EXPERIMENT_COLLECTION_NAME
            )
        }

        nimbus = NimbusClient(
            experimentContext,
            dataDir.path,
            remoteSettingsConfig,
            // The "dummy" field here is required for obscure reasons when generating code on desktop,
            // so we just automatically set it to a dummy value.
            AvailableRandomizationUnits(clientId = null, dummy = 0)
        )
    }

    // This is currently not available from the main thread.
    // see https://jira.mozilla.com/browse/SDK-191
    @WorkerThread
    override fun getActiveExperiments(): List<EnrolledExperiment> =
        nimbus.getActiveExperiments()

    override fun getExperimentBranch(experimentId: String): String? {
        recordExposure(experimentId)
        return nimbus.getExperimentBranch(experimentId)
    }

    override fun updateExperiments() {
        fetchScope.launch {
            fetchExperimentsOnThisThread()
            applyPendingExperimentsOnThisThread()
        }
    }

    // Method and apparatus to catch any uncaught exceptions
    @SuppressWarnings("TooGenericExceptionCaught")
    private fun <R> withCatchAll(thunk: () -> R) =
        try {
            thunk()
        } catch (e: Throwable) {
            try {
                errorReporter(e)
            } catch (e1: Throwable) {
                logger.error("Exception calling rust", e)
                logger.error("Exception reporting the exception", e1)
            }
            null
        }

    override fun initialize() {
        dbScope.launch {
            initializeOnThisThread()
        }
    }

    @WorkerThread
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun initializeOnThisThread() = withCatchAll {
        nimbus.initialize()
    }

    override fun fetchExperiments() {
        fetchScope.launch {
            fetchExperimentsOnThisThread()
        }
    }

    @WorkerThread
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun fetchExperimentsOnThisThread() = withCatchAll {
        try {
            nimbus.fetchExperiments()
            notifyObservers { onExperimentsFetched() }
        } catch (e: NimbusErrorException.RequestError) {
            logger.info("Error fetching experiments from endpoint: $e")
        } catch (e: NimbusErrorException.ResponseError) {
            logger.info("Error fetching experiments from endpoint: $e")
        }
    }

    override fun applyPendingExperiments() {
        dbScope.launch {
            applyPendingExperimentsOnThisThread()
        }
    }

    @WorkerThread
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun applyPendingExperimentsOnThisThread() = withCatchAll {
        try {
            nimbus.applyPendingExperiments().also(::recordExperimentTelemetryEvents)
            // Get the experiments to record in telemetry
            postEnrolmentCalculation()
        } catch (e: NimbusErrorException.InvalidExperimentFormat) {
            logger.info("Invalid experiment format: $e")
        }
    }

    @WorkerThread
    private fun postEnrolmentCalculation() {
        nimbus.getActiveExperiments().let {
            if (it.any()) {
                recordExperimentTelemetry(it)
                notifyObservers { onUpdatesApplied(it) }
            }
        }
    }

    override fun setExperimentsLocally(@RawRes file: Int) {
        dbScope.launch {
            withCatchAll {
                context.resources.openRawResource(file).use {
                    it.bufferedReader().readText()
                }
            }?.let { payload ->
                setExperimentsLocallyOnThisThread(payload)
            }
        }
    }

    override fun setExperimentsLocally(payload: String) {
        dbScope.launch {
            setExperimentsLocallyOnThisThread(payload)
        }
    }

    @WorkerThread
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun setExperimentsLocallyOnThisThread(payload: String) = withCatchAll {
        nimbus.setExperimentsLocally(payload)
    }

    @WorkerThread
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun setGlobalUserParticipationOnThisThread(active: Boolean) = withCatchAll {
        val enrolmentChanges = nimbus.setGlobalUserParticipation(active)
        if (enrolmentChanges.isNotEmpty()) {
            postEnrolmentCalculation()
        }
    }

    override fun optOut(experimentId: String) {
        dbScope.launch {
            withCatchAll {
                nimbus.optOut(experimentId).also(::recordExperimentTelemetryEvents)
            }
        }
    }

    override fun resetTelemetryIdentifiers() {
        // The "dummy" field here is required for obscure reasons when generating code on desktop,
        // so we just automatically set it to a dummy value.
        val aru = AvailableRandomizationUnits(clientId = null, dummy = 0)
        dbScope.launch {
            withCatchAll {
                nimbus.resetTelemetryIdentifiers(aru).also { enrollmentChangeEvents ->
                    recordExperimentTelemetryEvents(enrollmentChangeEvents)
                }
            }
        }
    }

    // This function shouldn't be exposed to the public API, but is meant for testing purposes to
    // force an experiment/branch enrollment.
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    internal fun optInWithBranch(experiment: String, branch: String) {
        dbScope.launch {
            withCatchAll {
                nimbus.optInWithBranch(experiment, branch).also(::recordExperimentTelemetryEvents)
            }
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun recordExperimentTelemetry(experiments: List<EnrolledExperiment>) {
        // Call Glean.setExperimentActive() for each active experiment.
        experiments.forEach { experiment ->
            // For now, we will just record the experiment id and the branch id. Once we can call
            // Glean from Rust, this will move to the nimbus-sdk Rust core.
            Glean.setExperimentActive(experiment.slug, experiment.branchSlug)
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun recordExperimentTelemetryEvents(enrollmentChangeEvents: List<EnrollmentChangeEvent>) {
        enrollmentChangeEvents.forEach { event ->
            when (event.change) {
                EnrollmentChangeEventType.ENROLLMENT -> {
                    NimbusEvents.enrollment.record(mapOf(
                        NimbusEvents.enrollmentKeys.experiment to event.experimentSlug,
                        NimbusEvents.enrollmentKeys.branch to event.branchSlug,
                        NimbusEvents.enrollmentKeys.enrollmentId to event.enrollmentId
                    ))
                }
                EnrollmentChangeEventType.DISQUALIFICATION -> {
                    NimbusEvents.disqualification.record(mapOf(
                        NimbusEvents.disqualificationKeys.experiment to event.experimentSlug,
                        NimbusEvents.disqualificationKeys.branch to event.branchSlug,
                        NimbusEvents.disqualificationKeys.enrollmentId to event.enrollmentId
                    ))
                }
                EnrollmentChangeEventType.UNENROLLMENT -> {
                    NimbusEvents.unenrollment.record(mapOf(
                        NimbusEvents.unenrollmentKeys.experiment to event.experimentSlug,
                        NimbusEvents.unenrollmentKeys.branch to event.branchSlug,
                        NimbusEvents.unenrollmentKeys.enrollmentId to event.enrollmentId
                    ))
                }
            }
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun recordExposure(experimentId: String) {
        dbScope.launch {
            recordExposureOnThisThread(experimentId)
        }
    }

    // The exposure event should be recorded when the expected treatment (or no-treatment, such as
    // for a "control" branch) is applied or shown to the user.
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @WorkerThread
    internal fun recordExposureOnThisThread(experimentId: String) = withCatchAll {
        val activeExperiments = getActiveExperiments()
        activeExperiments.find { it.slug == experimentId }?.also { experiment ->
            NimbusEvents.exposure.record(mapOf(
                NimbusEvents.exposureKeys.experiment to experiment.slug,
                NimbusEvents.exposureKeys.branch to experiment.branchSlug,
                NimbusEvents.exposureKeys.enrollmentId to experiment.enrollmentId
            ))
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

/**
 * An empty implementation of the `NimbusApi` to allow clients who have not enabled Nimbus (either
 * by feature flags, or by not using a server endpoint.
 *
 * Any implementations using this class will report that the user has not been enrolled into any
 * experiments, and will not report anything to Glean. Importantly, any calls to
 * `getExperimentBranch(slug)` will return `null`, i.e. as if the user is not enrolled into the
 * experiment.
 */
class NimbusDisabled(
    private val delegate: Observable<NimbusApi.Observer> = ObserverRegistry()
) : NimbusApi, Observable<NimbusApi.Observer> by delegate
