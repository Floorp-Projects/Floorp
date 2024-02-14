/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.feature.addons.migration

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.work.Constraints
import androidx.work.CoroutineWorker
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.NetworkType
import androidx.work.PeriodicWorkRequest
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkManager
import androidx.work.WorkerParameters
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.feature.addons.update.GlobalAddonDependencyProvider
import mozilla.components.feature.addons.worker.shouldReport
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.worker.Frequency
import java.util.concurrent.TimeUnit

/**
 * Contract to define the behavior for a periodic checker for newly supported add-ons.
 */
interface SupportedAddonsChecker {
    /**
     * Registers for periodic checks for new available add-ons.
     */
    fun registerForChecks()

    /**
     * Unregisters for periodic checks for new available add-ons.
     */
    fun unregisterForChecks()
}

/**
 * An implementation of [SupportedAddonsChecker] that uses the work manager api for scheduling checks.
 * @property applicationContext The application context.
 * @param frequency (Optional) indicates how often checks should be performed, defaults
 * to once per day.
 */
@Suppress("LargeClass")
class DefaultSupportedAddonsChecker(
    private val applicationContext: Context,
    private val frequency: Frequency = Frequency(1, TimeUnit.DAYS),
) : SupportedAddonsChecker {
    private val logger = Logger("DefaultSupportedAddonsChecker")

    /**
     * See [SupportedAddonsChecker.registerForChecks]
     */
    override fun registerForChecks() {
        WorkManager.getInstance(applicationContext).enqueueUniquePeriodicWork(
            CHECKER_UNIQUE_PERIODIC_WORK_NAME,
            ExistingPeriodicWorkPolicy.UPDATE,
            createPeriodicWorkerRequest(),
        )
        logger.info("Register check for new supported add-ons")
    }

    /**
     * See [SupportedAddonsChecker.unregisterForChecks]
     */
    override fun unregisterForChecks() {
        WorkManager.getInstance(applicationContext)
            .cancelUniqueWork(CHECKER_UNIQUE_PERIODIC_WORK_NAME)
        logger.info("Unregister check for new supported add-ons")
    }

    @VisibleForTesting
    internal fun createPeriodicWorkerRequest(): PeriodicWorkRequest {
        return PeriodicWorkRequestBuilder<SupportedAddonsWorker>(
            frequency.repeatInterval,
            frequency.repeatIntervalTimeUnit,
        ).apply {
            setConstraints(getWorkerConstrains())
            addTag(WORK_TAG_PERIODIC)
        }.build()
    }

    @VisibleForTesting
    internal fun getWorkerConstrains() = Constraints.Builder()
        .setRequiredNetworkType(NetworkType.CONNECTED)
        .build()

    companion object {
        private const val IDENTIFIER_PREFIX = "mozilla.components.feature.addons.migration"

        @VisibleForTesting
        internal const val CHECKER_UNIQUE_PERIODIC_WORK_NAME =
            "$IDENTIFIER_PREFIX.DefaultSupportedAddonsChecker.periodicWork"

        /**
         * Identifies all the workers that periodically check for new add-ons.
         */
        @VisibleForTesting
        internal const val WORK_TAG_PERIODIC =
            "$IDENTIFIER_PREFIX.DefaultSupportedAddonsChecker.periodicWork"
    }
}

/**
 * A implementation which uses WorkManager APIs to check for newly available supported add-ons.
 */
internal class SupportedAddonsWorker(
    val context: Context,
    params: WorkerParameters,
) : CoroutineWorker(context, params) {
    private val logger = Logger("SupportedAddonsWorker")

    @Suppress("TooGenericExceptionCaught")
    override suspend fun doWork(): Result = withContext(Dispatchers.IO) {
        try {
            logger.info("Trying to check for new supported add-ons")

            val addonManager = GlobalAddonDependencyProvider.requireAddonManager()
            val addonUpdater = GlobalAddonDependencyProvider.requireAddonUpdater()

            addonManager.getAddons().filter { addon ->
                addon.isSupported() && addon.isDisabledAsUnsupported()
            }.let { addons ->
                if (addons.isNotEmpty()) {
                    addons.forEach { addon ->
                        addonUpdater.registerForFutureUpdates(addon.id)
                    }
                    val extIds = addons.joinToString { addon -> addon.id }
                    logger.info("New supported add-ons available $extIds")
                }
            }
        } catch (exception: Exception) {
            logger.error(
                "An exception happened trying to check for new supported add-ons, re-schedule ${exception.message}",
                exception,
            )
            if (exception.shouldReport()) {
                GlobalAddonDependencyProvider.onCrash?.invoke(exception)
            }
        }
        Result.success()
    }
}
