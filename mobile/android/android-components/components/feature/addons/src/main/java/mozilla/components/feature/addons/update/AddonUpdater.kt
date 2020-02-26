/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.feature.addons.update

import android.app.IntentService
import android.app.Notification
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import androidx.annotation.VisibleForTesting
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import androidx.work.Constraints
import androidx.work.CoroutineWorker
import androidx.work.Data
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.ExistingWorkPolicy
import androidx.work.NetworkType
import androidx.work.OneTimeWorkRequest
import androidx.work.OneTimeWorkRequestBuilder
import androidx.work.PeriodicWorkRequest
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkManager
import androidx.work.WorkerParameters
import kotlinx.coroutines.Dispatchers
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.R
import mozilla.components.feature.addons.update.AddonUpdater.Frequency
import mozilla.components.support.base.ids.SharedIdsHelper
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.notification.ChannelData
import mozilla.components.support.ktx.android.notification.ensureNotificationChannelExists
import java.lang.Exception
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
     * Try to perform an update on the given [addonId].
     * @param addonId The unique id of the addon.
     */
    fun update(addonId: String)

    /**
     * Invoked when a web extension has changed its permissions while trying to update to a
     * new version. This requires user interaction as the updated extension will not be installed,
     * until the user grants the new permissions.
     *
     * @param current The current [WebExtension].
     * @param updated The updated [WebExtension] that requires extra permissions.
     * @param newPermissions Contains a list of all the new permissions.
     * @param onPermissionsGranted A callback to indicate if the new permissions from the [updated] extension
     * are granted or not.
     */
    fun onUpdatePermissionRequest(
        current: WebExtension,
        updated: WebExtension,
        newPermissions: List<String>,
        onPermissionsGranted: ((Boolean) -> Unit)
    )

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
@Suppress("TooManyFunctions", "LargeClass")
class DefaultAddonUpdater(
    private val applicationContext: Context,
    private val frequency: Frequency = Frequency(1, TimeUnit.DAYS)
) : AddonUpdater {
    private val logger = Logger("DefaultAddonUpdater")

    @VisibleForTesting
    internal val updateStatusStorage = UpdateStatusStorage()

    /**
     * See [AddonUpdater.registerForFutureUpdates]
     */
    override fun registerForFutureUpdates(addonId: String) {
        WorkManager.getInstance(applicationContext).enqueueUniquePeriodicWork(
            getUniquePeriodicWorkName(addonId),
            ExistingPeriodicWorkPolicy.KEEP,
            createPeriodicWorkerRequest(addonId)
        )
        logger.info("registerForFutureUpdates $addonId")
    }

    /**
     * See [AddonUpdater.unregisterForFutureUpdates]
     */
    override fun unregisterForFutureUpdates(addonId: String) {
        WorkManager.getInstance(applicationContext)
            .cancelUniqueWork(getUniquePeriodicWorkName(addonId))
        logger.info("unregisterForFutureUpdates $addonId")
    }

    /**
     * See [AddonUpdater.update]
     */
    override fun update(addonId: String) {
        WorkManager.getInstance(applicationContext).beginUniqueWork(
            getUniqueImmediateWorkName(addonId),
            ExistingWorkPolicy.KEEP,
            createImmediateWorkerRequest(addonId)
        ).enqueue()
        logger.info("update $addonId")
    }

    /**
     * See [AddonUpdater.onUpdatePermissionRequest]
     */
    override fun onUpdatePermissionRequest(
        current: WebExtension,
        updated: WebExtension,
        newPermissions: List<String>,
        onPermissionsGranted: (Boolean) -> Unit
    ) {
        logger.info("onUpdatePermissionRequest $current")

        val wasPreviouslyAllowed =
            updateStatusStorage.isPreviouslyAllowed(applicationContext, updated.id)

        onPermissionsGranted(wasPreviouslyAllowed)

        if (!wasPreviouslyAllowed) {
            createNotification(updated, newPermissions)
        } else {
            updateStatusStorage.markAsUnallowed(applicationContext, updated.id)
        }
    }

    @VisibleForTesting
    internal fun createImmediateWorkerRequest(addonId: String): OneTimeWorkRequest {
        val data = AddonUpdaterWorker.createWorkerData(addonId)
        val constraints = getWorkerConstrains()

        return OneTimeWorkRequestBuilder<AddonUpdaterWorker>()
            .setConstraints(constraints)
            .setInputData(data)
            .addTag(getUniqueImmediateWorkName(addonId))
            .addTag(WORK_TAG_IMMEDIATE)
            .build()
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
        .setRequiredNetworkType(NetworkType.CONNECTED)
        .build()

    @VisibleForTesting
    internal fun getUniquePeriodicWorkName(addonId: String) =
        "$IDENTIFIER_PREFIX$addonId.periodicWork"

    @VisibleForTesting
    internal fun getUniqueImmediateWorkName(extensionId: String) =
        "$IDENTIFIER_PREFIX$extensionId.immediateWork"

    @VisibleForTesting
    internal fun createNotification(
        extension: WebExtension,
        newPermissions: List<String>
    ): Notification {
        val notificationId = SharedIdsHelper.getIdForTag(applicationContext, NOTIFICATION_TAG)

        val channel = ChannelData(
            NOTIFICATION_CHANNEL_ID,
            R.string.mozac_feature_addons_updater_notification_channel,
            NotificationManagerCompat.IMPORTANCE_LOW
        )
        val channelId = ensureNotificationChannelExists(applicationContext, channel)
        val text = createContentText(newPermissions)

        logger.info("Created update notification for add-on ${extension.id}")
        return NotificationCompat.Builder(applicationContext, channelId)
            .setSmallIcon(mozilla.components.ui.icons.R.drawable.mozac_ic_extensions)
            .setContentTitle(getNotificationTitle(extension))
            .setContentText(text)
            .setPriority(NotificationCompat.PRIORITY_MAX)
            .setStyle(
                NotificationCompat.BigTextStyle()
                    .bigText(text)
            )
            .setContentIntent(createContentIntent())
            .addAction(createAllowAction(extension))
            .addAction(createDenyAction())
            .setAutoCancel(true)
            .build().also {
                NotificationManagerCompat.from(applicationContext).notify(notificationId, it)
            }
    }

    private fun getNotificationTitle(extension: WebExtension): String {
        return applicationContext.getString(
            R.string.mozac_feature_addons_updater_notification_title,
            extension.getMetadata()?.name
        )
    }

    private fun createContentText(newPermissions: List<String>): String {
        val contentText = applicationContext.getString(
            R.string.mozac_feature_addons_updater_notification_content, newPermissions.size
        )
        var permissionIndex = 1
        val permissionsText =
            Addon.localizePermissions(newPermissions).joinToString(separator = "\n") {
                "${permissionIndex++}-${applicationContext.getString(it)}"
            }
        return "$contentText:\n $permissionsText"
    }

    private fun createContentIntent(): PendingIntent {
        val intent =
            applicationContext.packageManager.getLaunchIntentForPackage(applicationContext.packageName)?.apply {
                flags = Intent.FLAG_ACTIVITY_NEW_TASK
            } ?: throw IllegalStateException("Package has no launcher intent")

        return PendingIntent.getActivity(
            applicationContext, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT
        )
    }

    private fun createAllowAction(ext: WebExtension): NotificationCompat.Action {
        val allowIntent = Intent(applicationContext, NotificationHandlerService::class.java).apply {
            action = NOTIFICATION_ACTION_ALLOW
            putExtra(NOTIFICATION_EXTRA_ADDON_ID, ext.id)
        }

        val allowPendingIntent = PendingIntent.getService(applicationContext, 0, allowIntent, 0)

        val allowText =
            applicationContext.getString(R.string.mozac_feature_addons_updater_notification_allow_button)

        return NotificationCompat.Action.Builder(
            mozilla.components.ui.icons.R.drawable.mozac_ic_extensions,
            allowText,
            allowPendingIntent
        ).build()
    }

    private fun createDenyAction(): NotificationCompat.Action {
        val allowIntent = Intent(applicationContext, NotificationHandlerService::class.java).apply {
            action = NOTIFICATION_ACTION_DENY
        }

        val denyPendingIntent = PendingIntent.getService(applicationContext, 0, allowIntent, 0)

        val denyText =
            applicationContext.getString(R.string.mozac_feature_addons_updater_notification_deny_button)

        return NotificationCompat.Action.Builder(
            mozilla.components.ui.icons.R.drawable.mozac_ic_extensions,
            denyText,
            denyPendingIntent
        ).build()
    }

    companion object {
        private const val NOTIFICATION_CHANNEL_ID =
            "mozilla.components.feature.addons.update.generic.channel"

        @VisibleForTesting
        internal const val NOTIFICATION_EXTRA_ADDON_ID =
            "mozilla.components.feature.addons.update.extra.extensionId"

        @VisibleForTesting
        internal const val NOTIFICATION_TAG = "mozilla.components.feature.addons.update.addonUpdater"

        @VisibleForTesting
        internal const val NOTIFICATION_ACTION_DENY =
            "mozilla.components.feature.addons.update.NOTIFICATION_ACTION_DENY"

        @VisibleForTesting
        internal const val NOTIFICATION_ACTION_ALLOW =
            "mozilla.components.feature.addons.update.NOTIFICATION_ACTION_ALLOW"
        private const val IDENTIFIER_PREFIX = "mozilla.components.feature.addons.update."

        /**
         * Identifies all the workers that periodically check for new updates.
         */
        @VisibleForTesting
        internal const val WORK_TAG_PERIODIC =
            "mozilla.components.feature.addons.update.addonUpdater.periodicWork"

        /**
         * Identifies all the workers that immediately check for new updates.
         */
        @VisibleForTesting
        internal const val WORK_TAG_IMMEDIATE =
            "mozilla.components.feature.addons.update.addonUpdater.immediateWork"
    }

    /**
     * Handles notification actions related to addons that require additional permissions
     * to be updated.
     */
    /** @suppress */
    class NotificationHandlerService : IntentService("NotificationHandlerService") {

        private val logger = Logger("NotificationHandlerService")

        @VisibleForTesting
        internal var context: Context = this

        public override fun onHandleIntent(intent: Intent?) {
            if (intent == null) return

            when (intent.action) {
                NOTIFICATION_ACTION_ALLOW -> {
                    handleAllowAction(intent)
                }
                NOTIFICATION_ACTION_DENY -> {
                    removeNotification()
                }
            }
        }

        @VisibleForTesting
        internal fun removeNotification() {
            val notificationId =
                SharedIdsHelper.getIdForTag(context, NOTIFICATION_TAG)
            NotificationManagerCompat.from(context).cancel(notificationId)
        }

        @VisibleForTesting
        internal fun handleAllowAction(intent: Intent) {
            val addonId = intent.getStringExtra(NOTIFICATION_EXTRA_ADDON_ID)!!
            val storage = UpdateStatusStorage()
            logger.info("Addon $addonId permissions were granted")
            storage.markAsAllowed(context, addonId)
            GlobalAddonDependencyProvider.requireAddonUpdater().update(addonId)
            removeNotification()
        }
    }

    /**
     * Stores the status of an addon update.
     */
    internal class UpdateStatusStorage {

        fun isPreviouslyAllowed(context: Context, addonId: String) =
            getData(context).contains(addonId)

        @Synchronized
        fun markAsAllowed(context: Context, addonId: String) {
            val allowSet = getData(context)
            allowSet.add(addonId)
            setData(context, allowSet)
        }

        @Synchronized
        fun markAsUnallowed(context: Context, addonId: String) {
            val allowSet = getData(context)
            allowSet.remove(addonId)
            setData(context, allowSet)
        }

        fun clear(context: Context) {
            val settings = getSharedPreferences(context)
            settings.edit().clear().apply()
        }

        private fun getSettings(context: Context) = getSharedPreferences(context)

        private fun setData(context: Context, allowSet: MutableSet<String>) {
            getSettings(context)
                    .edit()
                    .putStringSet(KEY_ALLOWED_SET, allowSet)
                    .apply()
        }

        private fun getData(context: Context): MutableSet<String> {
            val settings = getSharedPreferences(context)
            return requireNotNull(settings.getStringSet(KEY_ALLOWED_SET, mutableSetOf()))
        }

        private fun getSharedPreferences(context: Context): SharedPreferences {
            return context.getSharedPreferences(PREFERENCE_FILE, Context.MODE_PRIVATE)
        }

        companion object {
            private const val PREFERENCE_FILE =
                "mozilla.components.feature.addons.update.addons_updates_status_preference"
            private const val KEY_ALLOWED_SET =
                "mozilla.components.feature.addons.update.KEY_ALLOWED_SET"
        }
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

    @Suppress("OverridingDeprecatedMember")
    override val coroutineContext = Dispatchers.Main

    @Suppress("TooGenericExceptionCaught", "MaxLineLength")
    override suspend fun doWork(): Result {
        val extensionId = params.inputData.getString(KEY_DATA_EXTENSIONS_ID) ?: ""
        logger.info("Trying to update extension $extensionId")

        return suspendCoroutine { continuation ->
            try {
                val manager = GlobalAddonDependencyProvider.requireAddonManager()

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
            } catch (exception: Exception) {
                logger.error(
                        "Unable to update extension $extensionId, re-schedule ${exception.message}",
                        exception
                )
                GlobalAddonDependencyProvider.onCrash?.invoke(exception)
                continuation.resume(Result.retry())
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
