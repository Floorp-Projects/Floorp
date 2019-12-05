/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.feature.addons.update

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.work.Constraints
import androidx.work.CoroutineWorker
import androidx.work.Data
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.NetworkType
import androidx.work.PeriodicWorkRequest
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkManager
import androidx.work.WorkerParameters
import mozilla.components.feature.addons.update.AddonUpdater.Frequency
import mozilla.components.support.base.log.logger.Logger
import java.util.concurrent.TimeUnit
import kotlin.coroutines.resume
import kotlin.coroutines.suspendCoroutine

/**
 * Contract to define the behavior for updating addons.
 */
interface AddonUpdater {
    /**
     * Registers the given [addonId] for periodically check for new updates.
     * @param addonId The unique id of the addon.
     */
    fun registerForFutureUpdates(addonId: String)

    /**
     * Unregisters the given [addonId] for periodically checking for new updates.
     * @param addonId The unique id of the addon.
     */
    fun unregisterForFutureUpdates(addonId: String)

    /**
     * Indicates the status of a request for updating an addon.
     */
    sealed class Status {
        /**
         * The addon is not part of the installed list.
         */
        object NotInstalled : Status()

        /**
         * The addon was successfully updated.
         */
        object SuccessfullyUpdated : Status()

        /**
         * The addon has not been updated since the last update.
         */
        object NoUpdateAvailable : Status()

        /**
         * An error has happened while trying to update.
         * @property message A string message describing what has happened.
         * @property exception The exception of the error.
         */
        class Error(val message: String, val exception: Throwable) : Status()
    }

    /**
     * Indicates how often an extension should be updated.
     * @property repeatInterval Integer indicating how often the update should happen.
     * @property repeatIntervalTimeUnit The time unit of the [repeatInterval].
     */
    class Frequency(val repeatInterval: Long, val repeatIntervalTimeUnit: TimeUnit)
}

/**
 * An implementation of [AddonUpdater] that uses the work manager api for scheduling new updates.
 * @property applicationContext The application context.
 * @param frequency (Optional) indicates how often updates should be performed, defaults
 * to one day.
 */
class DefaultAddonUpdater(
    private val applicationContext: Context,
    private val frequency: Frequency = Frequency(1, TimeUnit.DAYS)
) : AddonUpdater {
    /**
     * See [AddonUpdater.registerForFutureUpdates]
     */
    override fun registerForFutureUpdates(addonId: String) {
        WorkManager.getInstance(applicationContext).enqueueUniquePeriodicWork(
            getUniquePeriodicWorkName(addonId),
            ExistingPeriodicWorkPolicy.KEEP,
            createPeriodicWorkerRequest(addonId)
        )
    }

    /**
     * See [AddonUpdater.unregisterForFutureUpdates]
     */
    override fun unregisterForFutureUpdates(addonId: String) {
        WorkManager.getInstance(applicationContext)
            .cancelUniqueWork(getUniquePeriodicWorkName(addonId))
    }

    @VisibleForTesting
    internal fun createPeriodicWorkerRequest(addonId: String): PeriodicWorkRequest {
        val data = AddonUpdaterWorker.createWorkerData(addonId)
        val constraints = getWorkerConstrains()

        return PeriodicWorkRequestBuilder<AddonUpdaterWorker>(
            frequency.repeatInterval,
            frequency.repeatIntervalTimeUnit
        )
            .setConstraints(constraints)
            .setInputData(data)
            .addTag(getUniquePeriodicWorkName(addonId))
            .addTag(WORK_TAG_PERIODIC)
            .build()
    }

    @VisibleForTesting
    internal fun getWorkerConstrains() = Constraints.Builder()
        .setRequiresStorageNotLow(true)
        .setRequiresDeviceIdle(true)
        .setRequiredNetworkType(NetworkType.CONNECTED)
        .build()

    @VisibleForTesting
    internal fun getUniquePeriodicWorkName(addonId: String) =
        "$IDENTIFIER_PREFIX$addonId.periodicWork"

    companion object {
        private const val IDENTIFIER_PREFIX = "mozilla.components.feature.addons.update."

        /**
         * Identifies all the workers that periodically check for new updates.
         */
        @VisibleForTesting
        internal const val WORK_TAG_PERIODIC =
            "mozilla.components.feature.addons.update.addonUpdater.periodicWork"
    }
}

/**
 * A implementation which uses WorkManager APIs to perform addon updates.
 */
internal class AddonUpdaterWorker(
    context: Context,
    private val params: WorkerParameters
) : CoroutineWorker(context, params) {
    private val logger = Logger("AddonUpdaterWorker")

    override suspend fun doWork(): Result {
        val extensionId = params.inputData.getString(KEY_DATA_EXTENSIONS_ID) ?: ""
        logger.info("Trying to update extension $extensionId")

        return suspendCoroutine { continuation ->
            val manager = GlobalAddonManagerProvider.requireAddonManager()

            manager.updateAddon(extensionId) { status ->
                val result = when (status) {
                    AddonUpdater.Status.NotInstalled -> {
                        logger.error("Not installed extension with id $extensionId removing from the updating queue")
                        Result.failure()
                    }
                    AddonUpdater.Status.NoUpdateAvailable -> {
                        logger.info("There is no new updates for the $extensionId")
                        Result.success()
                    }
                    AddonUpdater.Status.SuccessfullyUpdated -> {
                        logger.info("Extension $extensionId SuccessFullyUpdated")
                        Result.success()
                    }
                    is AddonUpdater.Status.Error -> {
                        logger.error(
                            "Unable to update extension $extensionId, re-schedule ${status.message}",
                            status.exception
                        )
                        Result.retry()
                    }
                }
                continuation.resume(result)
            }
        }
    }

    companion object {

        @VisibleForTesting
        internal const val KEY_DATA_EXTENSIONS_ID =
            "mozilla.components.feature.addons.update.KEY_DATA_EXTENSIONS_ID"

        @VisibleForTesting
        internal fun createWorkerData(extensionId: String) = Data.Builder()
            .putString(KEY_DATA_EXTENSIONS_ID, extensionId)
            .build()
    }
}
