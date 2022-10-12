/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.update

import android.app.Notification
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.os.IBinder
import androidx.annotation.VisibleForTesting
import androidx.annotation.WorkerThread
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
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.concept.engine.webextension.WebExtensionException
import mozilla.components.concept.engine.webextension.isUnsupported
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.R
import mozilla.components.feature.addons.update.db.UpdateAttemptsDatabase
import mozilla.components.feature.addons.update.db.toEntity
import mozilla.components.feature.addons.worker.shouldReport
import mozilla.components.support.base.ids.SharedIdsHelper
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.worker.Frequency
import mozilla.components.support.ktx.android.notification.ChannelData
import mozilla.components.support.ktx.android.notification.ensureNotificationChannelExists
import mozilla.components.support.utils.PendingIntentUtils
import mozilla.components.support.webextensions.WebExtensionSupport
import java.lang.Exception
import java.util.Date
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
        onPermissionsGranted: ((Boolean) -> Unit),
    )

    /**
     * Registers the [extensions] for periodic updates, if applicable. Built-in and
     * unsupported extensions will not update automatically.
     *
     * @param extensions The extensions to be registered for updates.
     */
    fun registerForFutureUpdates(extensions: List<WebExtension>) {
        extensions
            .filter { extension ->
                !extension.isBuiltIn() && !extension.isUnsupported()
            }
            .forEach { extension ->
                registerForFutureUpdates(extension.id)
            }
    }

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
        data class Error(val message: String, val exception: Throwable) : Status()
    }

    /**
     * Represents an attempt to update an add-on.
     */
    data class UpdateAttempt(val addonId: String, val date: Date, val status: Status?)
}

/**
 * An implementation of [AddonUpdater] that uses the work manager api for scheduling new updates.
 * @property applicationContext The application context.
 * @param frequency (Optional) indicates how often updates should be performed, defaults
 * to one day.
 */
@Suppress("LargeClass")
class DefaultAddonUpdater(
    private val applicationContext: Context,
    private val frequency: Frequency = Frequency(1, TimeUnit.DAYS),
) : AddonUpdater {
    private val logger = Logger("DefaultAddonUpdater")

    @VisibleForTesting
    internal var scope = CoroutineScope(Dispatchers.IO)

    @VisibleForTesting
    internal val updateStatusStorage = UpdateStatusStorage()
    internal var updateAttempStorage = UpdateAttemptStorage(applicationContext)

    /**
     * See [AddonUpdater.registerForFutureUpdates]. If an add-on is already registered nothing will happen.
     */
    override fun registerForFutureUpdates(addonId: String) {
        WorkManager.getInstance(applicationContext).enqueueUniquePeriodicWork(
            getUniquePeriodicWorkName(addonId),
            ExistingPeriodicWorkPolicy.KEEP,
            createPeriodicWorkerRequest(addonId),
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
        scope.launch {
            updateAttempStorage.remove(addonId)
        }
    }

    /**
     * See [AddonUpdater.update]
     */
    override fun update(addonId: String) {
        WorkManager.getInstance(applicationContext).beginUniqueWork(
            getUniqueImmediateWorkName(addonId),
            ExistingWorkPolicy.KEEP,
            createImmediateWorkerRequest(addonId),
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
        onPermissionsGranted: (Boolean) -> Unit,
    ) {
        logger.info("onUpdatePermissionRequest $current")

        val shouldGrantWithoutPrompt = Addon.localizePermissions(newPermissions, applicationContext).isEmpty()
        val shouldShowNotification =
            updateStatusStorage.isPreviouslyAllowed(applicationContext, updated.id) || shouldGrantWithoutPrompt

        onPermissionsGranted(shouldShowNotification)

        if (!shouldShowNotification) {
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
            frequency.repeatIntervalTimeUnit,
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
        newPermissions: List<String>,
    ): Notification {
        val notificationId = NotificationHandlerService.getNotificationId(applicationContext, extension.id)

        val channel = ChannelData(
            NOTIFICATION_CHANNEL_ID,
            R.string.mozac_feature_addons_updater_notification_channel,
            NotificationManagerCompat.IMPORTANCE_LOW,
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
                    .bigText(text),
            )
            .setContentIntent(createContentIntent())
            .addAction(createAllowAction(extension, notificationId))
            .addAction(createDenyAction(extension, notificationId))
            .setAutoCancel(true)
            .build().also {
                NotificationManagerCompat.from(applicationContext).notify(notificationId, it)
            }
    }

    private fun getNotificationTitle(extension: WebExtension): String {
        return applicationContext.getString(
            R.string.mozac_feature_addons_updater_notification_title,
            extension.getMetadata()?.name,
        )
    }

    @VisibleForTesting
    internal fun createContentText(newPermissions: List<String>): String {
        val validNewPermissions = Addon.localizePermissions(newPermissions, applicationContext)

        val string = if (validNewPermissions.size == 1) {
            R.string.mozac_feature_addons_updater_notification_content_singular
        } else {
            R.string.mozac_feature_addons_updater_notification_content
        }
        val contentText = applicationContext.getString(string, validNewPermissions.size)
        var permissionIndex = 1
        val permissionsText =
            validNewPermissions.joinToString(separator = "\n") {
                "${permissionIndex++}-$it"
            }
        return "$contentText:\n $permissionsText"
    }

    private fun createContentIntent(): PendingIntent {
        val intent =
            applicationContext.packageManager.getLaunchIntentForPackage(applicationContext.packageName)?.apply {
                flags = Intent.FLAG_ACTIVITY_NEW_TASK
            } ?: throw IllegalStateException("Package has no launcher intent")
        return PendingIntent.getActivity(
            applicationContext,
            0,
            intent,
            PendingIntentUtils.defaultFlags or PendingIntent.FLAG_UPDATE_CURRENT,
        )
    }

    @VisibleForTesting
    internal fun createNotificationIntent(extId: String, actionString: String): Intent {
        return Intent(applicationContext, NotificationHandlerService::class.java).apply {
            action = actionString
            putExtra(NOTIFICATION_EXTRA_ADDON_ID, extId)
        }
    }

    @VisibleForTesting
    internal fun createAllowAction(ext: WebExtension, requestCode: Int): NotificationCompat.Action {
        val allowIntent = createNotificationIntent(ext.id, NOTIFICATION_ACTION_ALLOW)
        val allowPendingIntent = PendingIntent.getService(
            applicationContext,
            requestCode,
            allowIntent,
            PendingIntentUtils.defaultFlags,
        )

        val allowText =
            applicationContext.getString(R.string.mozac_feature_addons_updater_notification_allow_button)

        return NotificationCompat.Action.Builder(
            mozilla.components.ui.icons.R.drawable.mozac_ic_extensions,
            allowText,
            allowPendingIntent,
        ).build()
    }

    @VisibleForTesting
    internal fun createDenyAction(ext: WebExtension, requestCode: Int): NotificationCompat.Action {
        val denyIntent = createNotificationIntent(ext.id, NOTIFICATION_ACTION_DENY)
        val denyPendingIntent = PendingIntent.getService(
            applicationContext,
            requestCode,
            denyIntent,
            PendingIntentUtils.defaultFlags,
        )

        val denyText =
            applicationContext.getString(R.string.mozac_feature_addons_updater_notification_deny_button)

        return NotificationCompat.Action.Builder(
            mozilla.components.ui.icons.R.drawable.mozac_ic_extensions,
            denyText,
            denyPendingIntent,
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
    class NotificationHandlerService : Service() {

        private val logger = Logger("NotificationHandlerService")

        @VisibleForTesting
        internal var context: Context = this

        internal fun onHandleIntent(intent: Intent?) {
            val addonId = intent?.getStringExtra(NOTIFICATION_EXTRA_ADDON_ID) ?: return

            when (intent.action) {
                NOTIFICATION_ACTION_ALLOW -> {
                    handleAllowAction(addonId)
                }
                NOTIFICATION_ACTION_DENY -> {
                    removeNotification(addonId)
                }
            }
        }

        @VisibleForTesting
        internal fun removeNotification(addonId: String) {
            val notificationId = getNotificationId(context, addonId)
            NotificationManagerCompat.from(context).cancel(notificationId)
        }

        @VisibleForTesting
        internal fun handleAllowAction(addonId: String) {
            val storage = UpdateStatusStorage()
            logger.info("Addon $addonId permissions were granted")
            storage.markAsAllowed(context, addonId)
            GlobalAddonDependencyProvider.requireAddonUpdater().update(addonId)
            removeNotification(addonId)
        }

        override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
            onHandleIntent(intent)
            return super.onStartCommand(intent, flags, startId)
        }

        override fun onBind(intent: Intent?): IBinder? = null

        companion object {
            @VisibleForTesting
            internal fun getNotificationId(context: Context, addonId: String): Int {
                return SharedIdsHelper.getIdForTag(context, "$NOTIFICATION_TAG.$addonId")
            }
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

    /**
     * A storage implementation to persist [AddonUpdater.UpdateAttempt]s.
     */
    class UpdateAttemptStorage(context: Context) {
        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        internal var databaseInitializer = {
            UpdateAttemptsDatabase.get(context)
        }
        private val database by lazy { databaseInitializer() }

        /**
         * Persists the [AddonUpdater.UpdateAttempt] provided as a parameter.
         * @param updateAttempt the [AddonUpdater.UpdateAttempt] to be stored.
         */
        @WorkerThread
        internal fun saveOrUpdate(updateAttempt: AddonUpdater.UpdateAttempt) {
            database.updateAttemptDao().insertOrUpdate(updateAttempt.toEntity())
        }

        /**
         * Finds the [AddonUpdater.UpdateAttempt] that match the [addonId] otherwise returns null.
         * @param addonId the id to be used as filter in the search.
         */
        @WorkerThread
        fun findUpdateAttemptBy(addonId: String): AddonUpdater.UpdateAttempt? {
            return database
                .updateAttemptDao()
                .getUpdateAttemptFor(addonId)
                ?.toUpdateAttempt()
        }

        /**
         * Deletes the [AddonUpdater.UpdateAttempt] that match the [addonId] provided as a parameter.
         * @param addonId the id of the [AddonUpdater.UpdateAttempt] to be deleted from the storage.
         */
        @WorkerThread
        internal fun remove(addonId: String) {
            database.updateAttemptDao().deleteUpdateAttempt(addonId)
        }
    }
}

/**
 * A implementation which uses WorkManager APIs to perform addon updates.
 */
internal class AddonUpdaterWorker(
    context: Context,
    private val params: WorkerParameters,
) : CoroutineWorker(context, params) {
    private val logger = Logger("AddonUpdaterWorker")
    internal var updateAttemptStorage = DefaultAddonUpdater.UpdateAttemptStorage(applicationContext)

    @VisibleForTesting
    internal var attemptScope = CoroutineScope(Dispatchers.IO)

    @Suppress("TooGenericExceptionCaught", "MaxLineLength")
    override suspend fun doWork(): Result = withContext(Dispatchers.Main) {
        val extensionId = params.inputData.getString(KEY_DATA_EXTENSIONS_ID) ?: ""
        logger.info("Trying to update extension $extensionId")
        // We need to guarantee that we are not trying to update without all the required state being initialized first.
        WebExtensionSupport.awaitInitialization()

        return@withContext suspendCoroutine { continuation ->
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
                                status.exception,
                            )
                            retryIfRecoverable(status.exception)
                        }
                    }
                    saveUpdateAttempt(extensionId, status)
                    continuation.resume(result)
                }
            } catch (exception: Exception) {
                logger.error(
                    "Unable to update extension $extensionId, re-schedule ${exception.message}",
                    exception,
                )
                saveUpdateAttempt(extensionId, AddonUpdater.Status.Error(exception.message ?: "", exception))
                if (exception.shouldReport()) {
                    GlobalAddonDependencyProvider.onCrash?.invoke(exception)
                }
                continuation.resume(retryIfRecoverable(exception))
            }
        }
    }

    @VisibleForTesting
    // We want to ensure, we are only retrying when the throwable isRecoverable,
    // this could cause side effects as described on:
    // https://github.com/mozilla-mobile/android-components/issues/8681
    internal fun retryIfRecoverable(throwable: Throwable): Result {
        return if (throwable is WebExtensionException && throwable.isRecoverable) {
            Result.retry()
        } else {
            Result.success()
        }
    }

    @VisibleForTesting
    internal fun saveUpdateAttempt(extensionId: String, status: AddonUpdater.Status) {
        attemptScope.launch {
            updateAttemptStorage.saveOrUpdate(AddonUpdater.UpdateAttempt(extensionId, Date(), status))
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
