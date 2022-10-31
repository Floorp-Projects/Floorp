/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.feature.addons.migration

import android.app.Notification
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.Intent.FLAG_ACTIVITY_CLEAR_TASK
import android.content.Intent.FLAG_ACTIVITY_NEW_TASK
import androidx.annotation.VisibleForTesting
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
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
import mozilla.components.concept.engine.webextension.EnableSource
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.R
import mozilla.components.feature.addons.ui.translateName
import mozilla.components.feature.addons.update.GlobalAddonDependencyProvider
import mozilla.components.feature.addons.worker.shouldReport
import mozilla.components.support.base.ids.SharedIdsHelper
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.worker.Frequency
import mozilla.components.support.ktx.android.content.appName
import mozilla.components.support.ktx.android.notification.ChannelData
import mozilla.components.support.ktx.android.notification.ensureNotificationChannelExists
import mozilla.components.support.utils.PendingIntentUtils
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
 * @param onNotificationClickIntent (Optional) Indicates which intent should be passed to the notification
 * when new supported add-ons are available.
 */
@Suppress("LargeClass")
class DefaultSupportedAddonsChecker(
    private val applicationContext: Context,
    private val frequency: Frequency = Frequency(1, TimeUnit.DAYS),
    onNotificationClickIntent: Intent = createDefaultNotificationIntent(applicationContext),
) : SupportedAddonsChecker {
    private val logger = Logger("DefaultSupportedAddonsChecker")

    init {
        SupportedAddonsWorker.onNotificationClickIntent = onNotificationClickIntent
    }

    /**
     * See [SupportedAddonsChecker.registerForChecks]
     */
    override fun registerForChecks() {
        WorkManager.getInstance(applicationContext).enqueueUniquePeriodicWork(
            CHECKER_UNIQUE_PERIODIC_WORK_NAME,
            ExistingPeriodicWorkPolicy.REPLACE,
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

        internal fun createDefaultNotificationIntent(content: Context): Intent {
            return content.packageManager.getLaunchIntentForPackage(content.packageName)?.apply {
                flags = FLAG_ACTIVITY_NEW_TASK or FLAG_ACTIVITY_CLEAR_TASK
            } ?: throw IllegalStateException("Package has no launcher intent")
        }
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

    @Suppress("TooGenericExceptionCaught", "MaxLineLength")
    override suspend fun doWork(): Result = withContext(Dispatchers.IO) {
        try {
            logger.info("Trying to check for new supported add-ons")

            val addonManager = GlobalAddonDependencyProvider.requireAddonManager()

            val newSupportedAddons = addonManager.getAddons().filter { addon ->
                addon.isSupported() && addon.isDisabledAsUnsupported()
            }

            withContext(Dispatchers.Main) {
                newSupportedAddons.forEach {
                    addonManager.enableAddon(
                        it,
                        source = EnableSource.APP_SUPPORT,
                        onError = { error ->
                            GlobalAddonDependencyProvider.onCrash?.invoke(error)
                        },
                    )
                }
            }

            if (newSupportedAddons.isNotEmpty()) {
                val extIds = newSupportedAddons.joinToString { it.id }
                logger.info("New supported add-ons available $extIds")
                createNotification(newSupportedAddons)
            }
        } catch (exception: Exception) {
            logger.error("An exception happened trying to check for new supported add-ons, re-schedule ${exception.message}", exception)
            if (exception.shouldReport()) {
                GlobalAddonDependencyProvider.onCrash?.invoke(exception)
            }
        }
        Result.success()
    }

    @VisibleForTesting
    internal fun createNotification(newSupportedAddons: List<Addon>): Notification {
        val notificationId = SharedIdsHelper.getIdForTag(context, NOTIFICATION_TAG)

        val channel = ChannelData(
            NOTIFICATION_CHANNEL_ID,
            R.string.mozac_feature_addons_supported_checker_notification_channel,
            NotificationManagerCompat.IMPORTANCE_LOW,
        )
        val channelId = ensureNotificationChannelExists(context, channel)
        return NotificationCompat.Builder(context, channelId)
            .setSmallIcon(mozilla.components.ui.icons.R.drawable.mozac_ic_extensions)
            .setContentTitle(getNotificationTitle(plural = newSupportedAddons.size > 1))
            .setContentText(getNotificationBody(newSupportedAddons, context))
            .setPriority(NotificationCompat.PRIORITY_MAX)
            .setContentIntent(createContentIntent())
            .setAutoCancel(true)
            .build().also {
                NotificationManagerCompat.from(context).notify(notificationId, it)
            }
    }

    @VisibleForTesting
    internal fun getNotificationTitle(plural: Boolean = false): String {
        val stringId = if (plural) {
            R.string.mozac_feature_addons_supported_checker_notification_title_plural
        } else {
            R.string.mozac_feature_addons_supported_checker_notification_title
        }
        return applicationContext.getString(stringId)
    }

    @VisibleForTesting
    internal fun getNotificationBody(newSupportedAddons: List<Addon>, context: Context): String? {
        return when (newSupportedAddons.size) {
            0 -> null
            1 -> {
                val addonName = newSupportedAddons.first().translateName(context)
                applicationContext.getString(
                    R.string.mozac_feature_addons_supported_checker_notification_content_one,
                    addonName,
                    applicationContext.appName,
                )
            }
            2 -> {
                val firstAddonName = newSupportedAddons.first().translateName(context)
                val secondAddonName = newSupportedAddons[1].translateName(context)
                applicationContext.getString(
                    R.string.mozac_feature_addons_supported_checker_notification_content_two,
                    firstAddonName,
                    secondAddonName,
                    applicationContext.appName,
                )
            }
            else -> {
                /* There's a restriction in the amount of characters that,
                   we can put in notification. To be safe we just use two variations,
                   as we don't know how long the name of an add-on could be.*/
                applicationContext.getString(
                    R.string.mozac_feature_addons_supported_checker_notification_content_more_than_two,
                    applicationContext.appName,
                )
            }
        }
    }

    private fun createContentIntent(): PendingIntent {
        return PendingIntent.getActivity(
            context,
            0,
            onNotificationClickIntent,
            PendingIntentUtils.defaultFlags or PendingIntent.FLAG_UPDATE_CURRENT,
        )
    }

    @Suppress("MaxLineLength")
    companion object {
        private const val NOTIFICATION_CHANNEL_ID =
            "mozilla.components.feature.addons.migration.DefaultSupportedAddonsChecker.generic.channel"

        @VisibleForTesting
        internal const val NOTIFICATION_TAG = "mozilla.components.feature.addons.migration.DefaultSupportedAddonsChecker"

        internal var onNotificationClickIntent: Intent = Intent()
    }
}
