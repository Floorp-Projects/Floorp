/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.os.Build
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import androidx.core.content.ContextCompat
import androidx.work.ExistingWorkPolicy
import androidx.work.OneTimeWorkRequest
import androidx.work.WorkManager
import androidx.work.Worker
import androidx.work.WorkerParameters
import java.util.concurrent.TimeUnit
import mozilla.components.support.base.ids.SharedIdsHelper
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.utils.Settings

class DefaultBrowserNotificationWorker(
    context: Context,
    workerParameters: WorkerParameters
) : Worker(context, workerParameters) {

    override fun doWork(): Result {
        ensureChannelExists()
        NotificationManagerCompat.from(applicationContext)
            .notify(NOTIFICATION_TAG, NOTIFICATION_ID, buildNotification())

        // default browser notification should only happen once
        applicationContext.settings().defaultBrowserNotificationDisplayed = true

        return Result.success()
    }

    /**
     * Build the default browser notification.
     */
    private fun buildNotification(): Notification {
        val channelId = ensureChannelExists()
        val intent = Intent(applicationContext, HomeActivity::class.java)
        intent.putExtra(INTENT_DEFAULT_BROWSER_NOTIFICATION, true)

        val pendingIntent = PendingIntent.getActivity(
            applicationContext,
            SharedIdsHelper.getNextIdForTag(applicationContext, NOTIFICATION_PENDING_INTENT_TAG),
            intent,
            0
        )

        with(applicationContext) {
            val appName = getString(R.string.app_name)
            return NotificationCompat.Builder(this, channelId)
                .setSmallIcon(R.drawable.ic_status_logo)
                .setContentTitle(
                    applicationContext.getString(R.string.notification_default_browser_title, appName))
                .setContentText(
                    applicationContext.getString(R.string.notification_default_browser_text, appName))
                .setBadgeIconType(NotificationCompat.BADGE_ICON_SMALL)
                .setColor(ContextCompat.getColor(this, R.color.primary_text_light_theme))
                .setPriority(NotificationCompat.PRIORITY_DEFAULT)
                .setShowWhen(false)
                .setContentIntent(pendingIntent)
                .setAutoCancel(true)
                .build()
        }
    }

    /**
     * Make sure a notification channel for default browser notification exists.
     *
     * Returns the channel id to be used for notifications.
     */
    private fun ensureChannelExists(): String {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val notificationManager: NotificationManager =
                applicationContext.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager

            val channel = NotificationChannel(
                NOTIFICATION_CHANNEL_ID,
                applicationContext.getString(R.string.notification_marketing_channel_name),
                NotificationManager.IMPORTANCE_DEFAULT
            )

            notificationManager.createNotificationChannel(channel)
        }

        return NOTIFICATION_CHANNEL_ID
    }

    companion object {
        private const val NOTIFICATION_CHANNEL_ID = "org.mozilla.fenix.default.browser.channel"
        private const val NOTIFICATION_ID = 1
        private const val NOTIFICATION_PENDING_INTENT_TAG = "org.mozilla.fenix.default.browser"
        private const val INTENT_DEFAULT_BROWSER_NOTIFICATION = "org.mozilla.fenix.default.browser.intent"
        private const val NOTIFICATION_TAG = "org.mozilla.fenix.default.browser.tag"
        private const val NOTIFICATION_WORK_NAME = "org.mozilla.fenix.default.browser.work"
        private const val NOTIFICATION_DELAY = Settings.ONE_DAY_MS

        fun isDefaultBrowserNotificationIntent(intent: Intent) =
            intent.extras?.containsKey(INTENT_DEFAULT_BROWSER_NOTIFICATION) ?: false

        fun setDefaultBrowserNotificationIfNeeded(context: Context) {
            val instanceWorkManager = WorkManager.getInstance(context)

            if (!context.settings().shouldShowDefaultBrowserNotification()) {
                // cancel notification work if already default browser
                instanceWorkManager.cancelUniqueWork(NOTIFICATION_WORK_NAME)
                return
            }

            val notificationWork = OneTimeWorkRequest.Builder(DefaultBrowserNotificationWorker::class.java)
                .setInitialDelay(NOTIFICATION_DELAY, TimeUnit.MILLISECONDS)
                .build()

            instanceWorkManager.beginUniqueWork(
                NOTIFICATION_WORK_NAME,
                ExistingWorkPolicy.KEEP,
                notificationWork
            ).enqueue()
        }
    }
}
