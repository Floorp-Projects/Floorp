/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import androidx.annotation.VisibleForTesting
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import androidx.core.app.NotificationManagerCompat.IMPORTANCE_NONE
import androidx.core.content.ContextCompat
import androidx.core.content.getSystemService
import mozilla.components.browser.state.state.content.DownloadState.Status.CANCELLED
import mozilla.components.browser.state.state.content.DownloadState.Status.COMPLETED
import mozilla.components.browser.state.state.content.DownloadState.Status.DOWNLOADING
import mozilla.components.browser.state.state.content.DownloadState.Status.FAILED
import mozilla.components.browser.state.state.content.DownloadState.Status.INITIATED
import mozilla.components.browser.state.state.content.DownloadState.Status.PAUSED
import mozilla.components.feature.downloads.AbstractFetchDownloadService.Companion.ACTION_CANCEL
import mozilla.components.feature.downloads.AbstractFetchDownloadService.Companion.ACTION_DISMISS
import mozilla.components.feature.downloads.AbstractFetchDownloadService.Companion.ACTION_OPEN
import mozilla.components.feature.downloads.AbstractFetchDownloadService.Companion.ACTION_PAUSE
import mozilla.components.feature.downloads.AbstractFetchDownloadService.Companion.ACTION_RESUME
import mozilla.components.feature.downloads.AbstractFetchDownloadService.Companion.ACTION_TRY_AGAIN
import mozilla.components.feature.downloads.AbstractFetchDownloadService.DownloadJobState
import mozilla.components.support.utils.PendingIntentUtils
import kotlin.random.Random

@Suppress("LargeClass")
internal object DownloadNotification {

    private const val NOTIFICATION_CHANNEL_ID = "mozac.feature.downloads.generic"
    private const val NOTIFICATION_GROUP_KEY = "mozac.feature.downloads.group"
    internal const val NOTIFICATION_DOWNLOAD_GROUP_ID = 100
    private const val LEGACY_NOTIFICATION_CHANNEL_ID = "Downloads"
    internal const val PERCENTAGE_MULTIPLIER = 100

    internal const val EXTRA_DOWNLOAD_ID = "downloadId"

    @VisibleForTesting
    internal fun createDownloadGroupNotification(
        context: Context,
        notifications: List<DownloadJobState>,
        notificationAccentColor: Int,
    ): Notification {
        val allDownloadsHaveFinished = notifications.all { it.status != DOWNLOADING }
        val icon = if (allDownloadsHaveFinished) {
            R.drawable.mozac_feature_download_ic_download_complete
        } else {
            R.drawable.mozac_feature_download_ic_ongoing_download
        }
        val summaryList = getSummaryList(context, notifications)
        val summaryLine1 = summaryList.first()
        val summaryLine2 = if (summaryList.size == 2) summaryList[1] else ""

        return NotificationCompat.Builder(context, ensureChannelExists(context))
            .setSmallIcon(icon)
            .setColor(ContextCompat.getColor(context, notificationAccentColor))
            .setContentTitle(
                context.applicationContext.getString(R.string.mozac_feature_downloads_notification_channel),
            )
            .setContentText(summaryList.joinToString("\n"))
            .setStyle(NotificationCompat.InboxStyle().addLine(summaryLine1).addLine(summaryLine2))
            .setGroup(NOTIFICATION_GROUP_KEY)
            .setGroupSummary(true)
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .build()
    }

    /**
     * Build the notification to be displayed while the download service is active.
     */
    fun createOngoingDownloadNotification(
        context: Context,
        downloadJobState: DownloadJobState,
        notificationAccentColor: Int,
    ): Notification {
        val downloadState = downloadJobState.state
        val bytesCopied = downloadJobState.currentBytesCopied
        val channelId = ensureChannelExists(context)
        val isIndeterminate = downloadJobState.isIndeterminate()

        return NotificationCompat.Builder(context, channelId)
            .setSmallIcon(R.drawable.mozac_feature_download_ic_ongoing_download)
            .setContentTitle(downloadState.fileName)
            .setContentText(downloadJobState.getProgress())
            .setColor(ContextCompat.getColor(context, notificationAccentColor))
            .setCategory(NotificationCompat.CATEGORY_PROGRESS)
            .setProgress(downloadState.contentLength?.toInt() ?: 0, bytesCopied.toInt(), isIndeterminate)
            .setOngoing(true)
            .setWhen(downloadJobState.createdTime)
            .setOnlyAlertOnce(true)
            .addAction(getPauseAction(context, downloadState.id))
            .addAction(getCancelAction(context, downloadState.id))
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .setCompatGroup(NOTIFICATION_GROUP_KEY)
            .build()
    }

    /**
     * Build the notification to be displayed while the download service is paused.
     */
    fun createPausedDownloadNotification(
        context: Context,
        downloadJobState: DownloadJobState,
        notificationAccentColor: Int,
    ): Notification {
        val channelId = ensureChannelExists(context)

        val downloadState = downloadJobState.state
        return NotificationCompat.Builder(context, channelId)
            .setSmallIcon(R.drawable.mozac_feature_download_ic_download)
            .setContentTitle(downloadState.fileName)
            .setContentText(
                context.applicationContext.getString(R.string.mozac_feature_downloads_paused_notification_text),
            )
            .setColor(ContextCompat.getColor(context, notificationAccentColor))
            .setCategory(NotificationCompat.CATEGORY_PROGRESS)
            .setOngoing(true)
            .setWhen(downloadJobState.createdTime)
            .setOnlyAlertOnce(true)
            .addAction(getResumeAction(context, downloadState.id))
            .addAction(getCancelAction(context, downloadState.id))
            .setDeleteIntent(createDismissPendingIntent(context, downloadState.id))
            .setCompatGroup(NOTIFICATION_GROUP_KEY)
            .build()
    }

    /**
     * Build the notification to be displayed when a download finishes.
     */
    fun createDownloadCompletedNotification(
        context: Context,
        downloadJobState: DownloadJobState,
        notificationAccentColor: Int,
    ): Notification {
        val channelId = ensureChannelExists(context)
        val downloadState = downloadJobState.state

        return NotificationCompat.Builder(context, channelId)
            .setSmallIcon(R.drawable.mozac_feature_download_ic_download_complete)
            .setContentTitle(downloadState.fileName)
            .setWhen(downloadJobState.createdTime)
            .setOnlyAlertOnce(true)
            .setContentText(
                context.applicationContext.getString(R.string.mozac_feature_downloads_completed_notification_text2),
            )
            .setColor(ContextCompat.getColor(context, notificationAccentColor))
            .setContentIntent(createPendingIntent(context, ACTION_OPEN, downloadState.id))
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .setDeleteIntent(createDismissPendingIntent(context, downloadState.id))
            .setCompatGroup(NOTIFICATION_GROUP_KEY)
            .build()
    }

    /**
     * Build the notification to be displayed when a download fails to finish.
     */
    fun createDownloadFailedNotification(
        context: Context,
        downloadJobState: DownloadJobState,
        notificationAccentColor: Int,
    ): Notification {
        val channelId = ensureChannelExists(context)
        val downloadState = downloadJobState.state

        return NotificationCompat.Builder(context, channelId)
            .setSmallIcon(R.drawable.mozac_feature_download_ic_download_failed)
            .setContentTitle(downloadState.fileName)
            .setContentText(
                context.applicationContext.getString(R.string.mozac_feature_downloads_failed_notification_text2),
            )
            .setColor(ContextCompat.getColor(context, notificationAccentColor))
            .setCategory(NotificationCompat.CATEGORY_ERROR)
            .addAction(getTryAgainAction(context, downloadState.id))
            .addAction(getCancelAction(context, downloadState.id))
            .setWhen(downloadJobState.createdTime)
            .setOnlyAlertOnce(true)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .setDeleteIntent(createDismissPendingIntent(context, downloadState.id))
            .setCompatGroup(NOTIFICATION_GROUP_KEY)
            .build()
    }

    @VisibleForTesting
    internal fun getSummaryList(context: Context, notifications: List<DownloadJobState>): List<String> {
        return notifications.take(2).map { downloadState ->
            "${downloadState.state.fileName} ${downloadState.getStatusDescription(context)}"
        }
    }

    /**
     * Check if notifications from the download channel are enabled.
     * Verifies that app notifications, channel notifications, and group notifications are enabled.
     */
    fun isChannelEnabled(context: Context): Boolean {
        return if (SDK_INT >= Build.VERSION_CODES.O) {
            val notificationManager: NotificationManager = context.getSystemService()!!
            if (!notificationManager.areNotificationsEnabled()) return false

            val channelId = ensureChannelExists(context)
            val channel = notificationManager.getNotificationChannel(channelId)
            if (channel.importance == IMPORTANCE_NONE) return false

            true
        } else {
            NotificationManagerCompat.from(context).areNotificationsEnabled()
        }
    }

    /**
     * Make sure a notification channel for download notification exists.
     *
     * Returns the channel id to be used for download notifications.
     */
    private fun ensureChannelExists(context: Context): String {
        if (SDK_INT >= Build.VERSION_CODES.O) {
            val notificationManager: NotificationManager = context.getSystemService()!!

            val channel = NotificationChannel(
                NOTIFICATION_CHANNEL_ID,
                context.applicationContext.getString(R.string.mozac_feature_downloads_notification_channel),
                NotificationManager.IMPORTANCE_LOW,
            )

            notificationManager.createNotificationChannel(channel)

            notificationManager.deleteNotificationChannel(LEGACY_NOTIFICATION_CHANNEL_ID)
        }

        return NOTIFICATION_CHANNEL_ID
    }

    private fun getPauseAction(context: Context, downloadStateId: String): NotificationCompat.Action {
        val pauseIntent = createPendingIntent(context, ACTION_PAUSE, downloadStateId)

        return NotificationCompat.Action.Builder(
            0,
            context.applicationContext.getString(R.string.mozac_feature_downloads_button_pause),
            pauseIntent,
        ).build()
    }

    private fun getResumeAction(context: Context, downloadStateId: String): NotificationCompat.Action {
        val resumeIntent = createPendingIntent(context, ACTION_RESUME, downloadStateId)

        return NotificationCompat.Action.Builder(
            0,
            context.applicationContext.getString(R.string.mozac_feature_downloads_button_resume),
            resumeIntent,
        ).build()
    }

    private fun getCancelAction(context: Context, downloadStateId: String): NotificationCompat.Action {
        val cancelIntent = createPendingIntent(context, ACTION_CANCEL, downloadStateId)

        return NotificationCompat.Action.Builder(
            0,
            context.applicationContext.getString(R.string.mozac_feature_downloads_button_cancel),
            cancelIntent,
        ).build()
    }

    private fun getTryAgainAction(context: Context, downloadStateId: String): NotificationCompat.Action {
        val tryAgainIntent = createPendingIntent(context, ACTION_TRY_AGAIN, downloadStateId)

        return NotificationCompat.Action.Builder(
            0,
            context.applicationContext.getString(R.string.mozac_feature_downloads_button_try_again),
            tryAgainIntent,
        ).build()
    }

    private fun createDismissPendingIntent(context: Context, downloadStateId: String): PendingIntent {
        return createPendingIntent(context, ACTION_DISMISS, downloadStateId)
    }

    private fun createPendingIntent(context: Context, action: String, downloadStateId: String): PendingIntent {
        val intent = Intent(action)
        intent.setPackage(context.applicationContext.packageName)
        intent.putExtra(EXTRA_DOWNLOAD_ID, downloadStateId)

        // We generate a random requestCode in order to generate a distinct PendingIntent:
        // https://developer.android.com/reference/android/app/PendingIntent.html
        return PendingIntent.getBroadcast(
            context.applicationContext,
            Random.nextInt(),
            intent,
            PendingIntentUtils.defaultFlags,
        )
    }
}

@VisibleForTesting
internal fun NotificationCompat.Builder.setCompatGroup(groupKey: String): NotificationCompat.Builder {
    return if (SDK_INT >= Build.VERSION_CODES.N) {
        setGroup(groupKey)
    } else {
        this
    }
}

@VisibleForTesting
internal fun DownloadJobState.getProgress(): String {
    val bytesCopied = currentBytesCopied
    return if (isIndeterminate()) {
        ""
    } else {
        "${DownloadNotification.PERCENTAGE_MULTIPLIER * bytesCopied / state.contentLength!!}%"
    }
}

private fun DownloadJobState.isIndeterminate(): Boolean {
    val bytesCopied = currentBytesCopied
    return state.contentLength == null || bytesCopied == 0L || state.contentLength == 0L
}

@VisibleForTesting
internal fun DownloadJobState.getStatusDescription(context: Context): String {
    return when (this.status) {
        DOWNLOADING -> {
            getProgress()
        }

        PAUSED -> {
            context.applicationContext.getString(R.string.mozac_feature_downloads_paused_notification_text)
        }

        COMPLETED -> {
            context.applicationContext.getString(R.string.mozac_feature_downloads_completed_notification_text2)
        }

        FAILED -> {
            context.applicationContext.getString(R.string.mozac_feature_downloads_failed_notification_text2)
        }

        CANCELLED, INITIATED -> ""
    }
}
