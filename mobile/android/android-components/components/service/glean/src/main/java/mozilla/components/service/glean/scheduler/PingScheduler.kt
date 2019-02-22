/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.scheduler

import android.arch.lifecycle.Lifecycle
import android.arch.lifecycle.LifecycleObserver
import android.arch.lifecycle.OnLifecycleEvent
import android.arch.lifecycle.ProcessLifecycleOwner
import android.content.Context
import android.support.annotation.VisibleForTesting
import androidx.work.BackoffPolicy
import androidx.work.Constraints
import androidx.work.ExistingWorkPolicy
import androidx.work.NetworkType
import androidx.work.OneTimeWorkRequest
import androidx.work.OneTimeWorkRequestBuilder
import androidx.work.WorkManager
import mozilla.components.service.glean.Glean
import mozilla.components.service.glean.GleanMetrics.GleanBaseline
import mozilla.components.service.glean.ping.BaselinePing
import mozilla.components.service.glean.ping.PingMaker
import mozilla.components.service.glean.storages.PingStorageEngine
import mozilla.components.service.glean.storages.StorageEngineManager
import mozilla.components.support.base.log.logger.Logger
import java.util.UUID
import java.util.concurrent.TimeUnit

/**
 * This class is responsible for scheduling of glean pings. In order to do this in association
 * with lifecycle events, it implements [LifecycleObserver].  On entering the background
 * [Lifecycle.Event.ON_STOP] state, it will use [WorkManager] to schedule a [PingUploadWorker] task
 * which is responsible for uploading the ping.
 */
internal class PingScheduler(
    applicationContext: Context,
    storageEngineManager: StorageEngineManager,
    pingMaker: PingMaker = PingMaker(storageEngineManager, applicationContext),
    pingStorageEngine: PingStorageEngine = PingStorageEngine(applicationContext),
    metricsPingScheduler: MetricsPingScheduler = MetricsPingScheduler(applicationContext)
) : LifecycleObserver {
    private val logger = Logger("glean/GleanLifecycleObserver")

    // The application id detected by glean to be used as part of the submission
    // endpoint.
    internal var applicationId = sanitizeApplicationId(applicationContext.packageName)

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    val pingMaker: PingMaker = pingMaker
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    val pingStorageEngine = pingStorageEngine
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    val metricsPingScheduler = metricsPingScheduler

    init {
        ProcessLifecycleOwner.get().lifecycle.addObserver(this)
    }

    /**
     * Handles necessary glean tasks upon entering the background state [Lifecycle.Event.ON_STOP]
     */
    @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
    fun onEnterBackground() {
        // We're going to background, so store how much time we spent
        // on foreground.
        GleanBaseline.duration.stopAndSum()

        if (!Glean.isInitialized()) {
            logger.error("Glean not initialized, abort serialize and send ping")
            return
        }

        // Do not send ping if upload is disabled
        if (!Glean.getUploadEnabled()) {
            logger.debug("Glean upload not enabled, abort serialize and send ping.")
            return
        }

        // Schedule the baseline and event pings and save the result to determine whether or not
        // we need to schedule the upload worker
        var availableToSend = assembleAndSerializePing(BaselinePing.STORE_NAME)
        availableToSend = availableToSend || assembleAndSerializePing("events")

        // Check the metrics ping schedule to determine whether it's time to schedule a new
        // metrics ping or not
        if (metricsPingScheduler.canSendPing()) {
            availableToSend = availableToSend || assembleAndSerializePing(MetricsPingScheduler.STORE_NAME)
            metricsPingScheduler.updateSentTimestamp()
        }

        // It should only take a single PingUploadWorker to process all queued pings, so we only
        // want to have one scheduled at a time.
        if (availableToSend) {
            WorkManager.getInstance().enqueueUniqueWork(
                PingUploadWorker.PING_WORKER_TAG,
                ExistingWorkPolicy.KEEP,
                buildWorkRequest())
        }
    }

    /**
     * Updates the [GleanBaseline.duration] metric when entering the foreground.
     * We use [Lifecycle.Event.ON_START] here because we don't want to incorrectly count metrics
     * in [Lifecycle.Event.ON_RESUME] as pause/resume can happen when interacting with things like
     * the navigation shade which could lead to incorrectly recording the start of a duration, etc.
     *
     * https://developer.android.com/reference/android/app/Activity.html#onStart()
     */
    @OnLifecycleEvent(Lifecycle.Event.ON_START)
    fun onEnterForeground() {
        // Note that this is sending the length of the last foreground session
        // because it belongs to the baseline ping and that ping is sent every
        // time the app goes to background.
        GleanBaseline.duration.start()
    }

    /**
     * Collect and assemble the ping and serialize the ping to be read when uploaded, but only if
     * glean is initialized, upload is enabled, and there is ping data to send.
     *
     * @param pingName This is the ping store/name for which to build and schedule the ping
     */
    internal fun assembleAndSerializePing(pingName: String): Boolean {
        // Build and send the ping on an async thread. Since the pingMaker.collect() function
        // returns null if there is nothing to send we can use this to avoid sending an empty ping
        return pingMaker.collect(pingName)?.also { pingContent ->
            // Store the serialized ping to file for PingUploadWorker to read and upload when the
            // schedule is triggered
            val uuid = UUID.randomUUID()
            pingStorageEngine.store(uuid, makePath(pingName, uuid), pingContent)
        } != null
    }

    /**
     * Build the constraints around which the worker can be run, such as whether network
     * connectivity is required.
     *
     * @return [Constraints] object containing the required work constraints
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun buildConstraints(): Constraints = Constraints.Builder()
        .setRequiredNetworkType(NetworkType.CONNECTED)
        .build()

    /**
     * Build the [OneTimeWorkRequest] for enqueueing in the [WorkManager].  This also adds a tag
     * by which [isWorkScheduled] can tell if the worker object has been enqueued.
     *
     * @return [OneTimeWorkRequest] representing the task for the [WorkManager] to enqueue and run
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun buildWorkRequest(): OneTimeWorkRequest = OneTimeWorkRequestBuilder<PingUploadWorker>()
        .addTag(PingUploadWorker.PING_WORKER_TAG)
        .setConstraints(buildConstraints())
        .setBackoffCriteria(
            BackoffPolicy.EXPONENTIAL,
            PingUploadWorker.DEFAULT_BACKOFF_FOR_UPLOAD,
            TimeUnit.SECONDS)
        .build()

    /**
     * Helper function to facilitate building the upload path for the scheduled ping
     *
     * @param docType This is the ping type: baseline, events, or metrics
     * @param uuid This is the unique identifier for the ping and is also the filename used for
     *             serializing the ping to a file.
     * @return String representing the upload path
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun makePath(docType: String, uuid: UUID): String {
        return "/submit/$applicationId/$docType/${Glean.SCHEMA_VERSION}/$uuid"
    }

    /**
     * Sanitizes the application id, generating a pipeline-friendly string that replaces
     * non alphanumeric characters with dashes.
     *
     * @param applicationId the string representing the application id
     *
     * @return the sanitized version of the application id
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun sanitizeApplicationId(applicationId: String): String {
        return applicationId.replace("[^a-zA-Z0-9]+".toRegex(), "-")
    }
}
