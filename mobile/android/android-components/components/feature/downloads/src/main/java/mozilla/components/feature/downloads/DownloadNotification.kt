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
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import androidx.core.app.NotificationManagerCompat.IMPORTANCE_NONE
import androidx.core.content.ContextCompat
import androidx.core.content.getSystemService
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.feature.downloads.AbstractFetchDownloadService.Companion.ACTION_CANCEL
import mozilla.components.feature.downloads.AbstractFetchDownloadService.Companion.ACTION_OPEN
import mozilla.components.feature.downloads.AbstractFetchDownloadService.Companion.ACTION_PAUSE
import mozilla.components.feature.downloads.AbstractFetchDownloadService.Companion.ACTION_RESUME
import mozilla.components.feature.downloads.AbstractFetchDownloadService.Companion.ACTION_TRY_AGAIN
import kotlin.random.Random

@Suppress("TooManyFunctions")
internal object DownloadNotification {

    private const val NOTIFICATION_CHANNEL_ID = "Downloads"

    internal const val EXTRA_DOWNLOAD_ID = "downloadId"

    /**
     * Build the notification to be displayed while the download service is active.
     */
    fun createOngoingDownloadNotification(context: Context, downloadState: DownloadState): Notification {
        val channelId = ensureChannelExists(context)
        val fileSizeText = (downloadState.contentLength?.toMegabyteString() ?: "")

        return NotificationCompat.Builder(context, channelId)
            .setSmallIcon(R.drawable.mozac_feature_download_ic_ongoing_download)
            .setContentTitle(downloadState.fileName)
            .setContentText(fileSizeText)
            .setColor(ContextCompat.getColor(context, R.color.mozac_feature_downloads_notification))
            .setCategory(NotificationCompat.CATEGORY_PROGRESS)
            .setProgress(1, 0, true)
            .setOngoing(true)
            .addAction(getPauseAction(context, downloadState.id))
            .addAction(getCancelAction(context, downloadState.id))
            .build()
    }

    /**
     * Build the notification to be displayed while the download service is paused.
     */
    fun createPausedDownloadNotification(context: Context, downloadState: DownloadState): Notification {
        val channelId = ensureChannelExists(context)

        return NotificationCompat.Builder(context, channelId)
            .setSmallIcon(R.drawable.mozac_feature_download_ic_download)
            .setContentTitle(downloadState.fileName)
            .setContentText(context.getString(R.string.mozac_feature_downloads_paused_notification_text))
            .setColor(ContextCompat.getColor(context, R.color.mozac_feature_downloads_notification))
            .setCategory(NotificationCompat.CATEGORY_PROGRESS)
            .setOngoing(true)
            .addAction(getResumeAction(context, downloadState.id))
            .addAction(getCancelAction(context, downloadState.id))
            .build()
    }

    /**
     * Build the notification to be displayed when a download finishes.
     */
    fun createDownloadCompletedNotification(context: Context, downloadState: DownloadState): Notification {
        val channelId = ensureChannelExists(context)

        return NotificationCompat.Builder(context, channelId)
            .setSmallIcon(R.drawable.mozac_feature_download_ic_download_complete)
            .setContentTitle(downloadState.fileName)
            .setContentText(context.getString(R.string.mozac_feature_downloads_completed_notification_text2))
            .setColor(ContextCompat.getColor(context, R.color.mozac_feature_downloads_notification))
            .setContentIntent(createPendingIntent(context, ACTION_OPEN, downloadState.id))
            .build()
    }

    /**
     * Build the notification to be displayed when a download fails to finish.
     */
    fun createDownloadFailedNotification(context: Context, downloadState: DownloadState): Notification {
        val channelId = ensureChannelExists(context)

        return NotificationCompat.Builder(context, channelId)
            .setSmallIcon(R.drawable.mozac_feature_download_ic_download_failed)
            .setContentTitle(downloadState.fileName)
            .setContentText(context.getString(R.string.mozac_feature_downloads_failed_notification_text2))
            .setColor(ContextCompat.getColor(context, R.color.mozac_feature_downloads_notification))
            .setCategory(NotificationCompat.CATEGORY_ERROR)
            .addAction(getTryAgainAction(context, downloadState.id))
            .addAction(getCancelAction(context, downloadState.id))
            .build()
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
                context.getString(R.string.mozac_feature_downloads_notification_channel),
                NotificationManager.IMPORTANCE_DEFAULT
            )

            notificationManager.createNotificationChannel(channel)
        }

        return NOTIFICATION_CHANNEL_ID
    }

    private fun getPauseAction(context: Context, downloadStateId: Long): NotificationCompat.Action {
        val pauseIntent = createPendingIntent(context, ACTION_PAUSE, downloadStateId)

        return NotificationCompat.Action.Builder(
            0,
            context.getString(R.string.mozac_feature_downloads_button_pause),
            pauseIntent
        ).build()
    }

    private fun getResumeAction(context: Context, downloadStateId: Long): NotificationCompat.Action {
        val resumeIntent = createPendingIntent(context, ACTION_RESUME, downloadStateId)

        return NotificationCompat.Action.Builder(
            0,
            context.getString(R.string.mozac_feature_downloads_button_resume),
            resumeIntent
        ).build()
    }

    private fun getCancelAction(context: Context, downloadStateId: Long): NotificationCompat.Action {
        val cancelIntent = createPendingIntent(context, ACTION_CANCEL, downloadStateId)

        return NotificationCompat.Action.Builder(
            0,
            context.getString(R.string.mozac_feature_downloads_button_cancel),
            cancelIntent
        ).build()
    }

    private fun getTryAgainAction(context: Context, downloadStateId: Long): NotificationCompat.Action {
        val tryAgainIntent = createPendingIntent(context, ACTION_TRY_AGAIN, downloadStateId)

        return NotificationCompat.Action.Builder(
            0,
            context.getString(R.string.mozac_feature_downloads_button_try_again),
            tryAgainIntent
        ).build()
    }

    private fun createPendingIntent(context: Context, action: String, downloadStateId: Long): PendingIntent {
        val intent = Intent(action)
        intent.setPackage(context.applicationContext.packageName)
        intent.putExtra(EXTRA_DOWNLOAD_ID, downloadStateId)

        // We generate a random requestCode in order to generate a distinct PendingIntent:
        // https://developer.android.com/reference/android/app/PendingIntent.html
        return PendingIntent.getBroadcast(context.applicationContext, Random.nextInt(), intent, 0)
    }
}
