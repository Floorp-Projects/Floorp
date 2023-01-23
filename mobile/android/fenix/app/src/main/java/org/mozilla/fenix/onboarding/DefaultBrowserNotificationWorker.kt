/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import android.app.Notification
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import androidx.core.app.NotificationManagerCompat
import androidx.work.ExistingWorkPolicy
import androidx.work.OneTimeWorkRequest
import androidx.work.WorkManager
import androidx.work.Worker
import androidx.work.WorkerParameters
import mozilla.components.service.glean.private.NoExtras
import mozilla.components.support.base.ids.SharedIdsHelper
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.utils.IntentUtils
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.utils.createBaseNotification
import java.util.concurrent.TimeUnit

class DefaultBrowserNotificationWorker(
    context: Context,
    workerParameters: WorkerParameters,
) : Worker(context, workerParameters) {

    override fun doWork(): Result {
        val channelId = ensureMarketingChannelExists(applicationContext)

        NotificationManagerCompat.from(applicationContext)
            .notify(
                NOTIFICATION_TAG,
                DEFAULT_BROWSER_NOTIFICATION_ID,
                buildNotification(channelId),
            )

        Events.defaultBrowserNotifShown.record(NoExtras())

        // default browser notification should only happen once
        applicationContext.settings().defaultBrowserNotificationDisplayed = true

        return Result.success()
    }

    /**
     * Build the default browser notification.
     */
    private fun buildNotification(channelId: String): Notification {
        val intent = Intent(applicationContext, HomeActivity::class.java)
        intent.putExtra(INTENT_DEFAULT_BROWSER_NOTIFICATION, true)

        val pendingIntent = PendingIntent.getActivity(
            applicationContext,
            SharedIdsHelper.getNextIdForTag(applicationContext, NOTIFICATION_PENDING_INTENT_TAG),
            intent,
            IntentUtils.defaultIntentPendingFlags,
        )

        with(applicationContext) {
            val appName = getString(R.string.app_name)
            return createBaseNotification(
                this,
                channelId,
                getString(R.string.notification_default_browser_title, appName),
                getString(R.string.notification_default_browser_text, appName),
                pendingIntent,
            )
        }
    }

    companion object {
        private const val NOTIFICATION_PENDING_INTENT_TAG = "org.mozilla.fenix.default.browser"
        private const val INTENT_DEFAULT_BROWSER_NOTIFICATION = "org.mozilla.fenix.default.browser.intent"
        private const val NOTIFICATION_TAG = "org.mozilla.fenix.default.browser.tag"
        private const val NOTIFICATION_WORK_NAME = "org.mozilla.fenix.default.browser.work"
        private const val NOTIFICATION_DELAY = Settings.THREE_DAYS_MS

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
                notificationWork,
            ).enqueue()
        }
    }
}
