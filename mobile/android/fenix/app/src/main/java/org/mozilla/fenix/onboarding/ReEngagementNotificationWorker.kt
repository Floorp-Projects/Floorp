/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import android.app.Notification
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import androidx.annotation.VisibleForTesting
import androidx.core.app.NotificationManagerCompat
import androidx.work.ExistingWorkPolicy
import androidx.work.OneTimeWorkRequest
import androidx.work.WorkManager
import androidx.work.Worker
import androidx.work.WorkerParameters
import mozilla.components.support.base.ids.SharedIdsHelper
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.nimbus.FxNimbus
import org.mozilla.fenix.utils.IntentUtils
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.utils.createBaseNotification
import java.util.concurrent.TimeUnit

/**
 * Worker that builds and schedules the re-engagement notification
 */
class ReEngagementNotificationWorker(
    context: Context,
    workerParameters: WorkerParameters,
) : Worker(context, workerParameters) {

    override fun doWork(): Result {
        val settings = applicationContext.settings()

        if (isActiveUser(settings) || !settings.shouldShowReEngagementNotification()) {
            return Result.success()
        }

        // Recording the exposure event here to capture all users who met all criteria to receive
        // the re-engagement notification
        FxNimbus.features.reEngagementNotification.recordExposure()

        if (!settings.reEngagementNotificationEnabled) {
            return Result.success()
        }

        val channelId = ensureMarketingChannelExists(applicationContext)
        NotificationManagerCompat.from(applicationContext)
            .notify(
                NOTIFICATION_TAG,
                RE_ENGAGEMENT_NOTIFICATION_ID,
                buildNotification(channelId),
            )

        // re-engagement notification should only be shown once
        settings.reEngagementNotificationShown = true

        Events.reEngagementNotifShown.record(NoExtras())

        return Result.success()
    }

    private fun buildNotification(channelId: String): Notification {
        val intent = Intent(applicationContext, HomeActivity::class.java)
        intent.putExtra(INTENT_RE_ENGAGEMENT_NOTIFICATION, true)

        val pendingIntent = PendingIntent.getActivity(
            applicationContext,
            SharedIdsHelper.getNextIdForTag(applicationContext, NOTIFICATION_PENDING_INTENT_TAG),
            intent,
            IntentUtils.defaultIntentPendingFlags,
        )

        with(applicationContext) {
            val title = when (settings().reEngagementNotificationType) {
                NOTIFICATION_TYPE_A -> getString(R.string.notification_re_engagement_A_title)
                NOTIFICATION_TYPE_B -> getString(R.string.notification_re_engagement_B_title)
                else -> getString(R.string.notification_re_engagement_title)
            }

            val text = when (settings().reEngagementNotificationType) {
                NOTIFICATION_TYPE_A ->
                    getString(R.string.notification_re_engagement_A_text, getString(R.string.app_name))
                NOTIFICATION_TYPE_B -> getString(R.string.notification_re_engagement_B_text)
                else -> getString(R.string.notification_re_engagement_text, getString(R.string.app_name))
            }

            return createBaseNotification(this, channelId, title, text, pendingIntent)
        }
    }

    companion object {
        const val NOTIFICATION_TARGET_URL = "https://www.mozilla.org/firefox/privacy/"
        const val NOTIFICATION_TYPE_A = 1
        const val NOTIFICATION_TYPE_B = 2

        private const val NOTIFICATION_PENDING_INTENT_TAG = "org.mozilla.fenix.re-engagement"
        private const val INTENT_RE_ENGAGEMENT_NOTIFICATION = "org.mozilla.fenix.re-engagement.intent"
        private const val NOTIFICATION_TAG = "org.mozilla.fenix.re-engagement.tag"
        private const val NOTIFICATION_WORK_NAME = "org.mozilla.fenix.re-engagement.work"
        private const val NOTIFICATION_DELAY = Settings.TWO_DAYS_MS

        // We are trying to reach the users that are inactive after the initial 24 hours
        private const val INACTIVE_USER_THRESHOLD = NOTIFICATION_DELAY - Settings.ONE_DAY_MS

        /**
         * Check if the intent is from the re-engagement notification
         */
        fun isReEngagementNotificationIntent(intent: Intent) =
            intent.extras?.containsKey(INTENT_RE_ENGAGEMENT_NOTIFICATION) ?: false

        /**
         * Schedules the re-engagement notification if needed.
         */
        fun setReEngagementNotificationIfNeeded(context: Context) {
            val instanceWorkManager = WorkManager.getInstance(context)

            if (!context.settings().shouldSetReEngagementNotification()) {
                return
            }

            val notificationWork = OneTimeWorkRequest.Builder(ReEngagementNotificationWorker::class.java)
                .setInitialDelay(NOTIFICATION_DELAY, TimeUnit.MILLISECONDS)
                .build()

            instanceWorkManager.beginUniqueWork(
                NOTIFICATION_WORK_NAME,
                ExistingWorkPolicy.KEEP,
                notificationWork,
            ).enqueue()
        }

        @VisibleForTesting
        internal fun isActiveUser(settings: Settings): Boolean {
            if (System.currentTimeMillis() - settings.lastBrowseActivity > INACTIVE_USER_THRESHOLD) {
                return false
            }

            return true
        }
    }
}
