/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.service

import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.IBinder
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.Companion.PRIVATE
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.R
import mozilla.components.lib.crash.notification.CrashNotification
import mozilla.components.support.base.ids.SharedIdsHelper

private const val NOTIFICATION_TAG = "mozac.lib.crash.sendcrash"
private const val NOTIFICATION_ID = 1

@VisibleForTesting(otherwise = PRIVATE)
internal const val NOTIFICATION_TAG_KEY = "mozac.lib.crash.notification.tag"

@VisibleForTesting(otherwise = PRIVATE)
internal const val NOTIFICATION_ID_KEY = "mozac.lib.crash.notification.id"

class SendCrashReportService : Service() {
    private val crashReporter: CrashReporter by lazy { CrashReporter.requireInstance }

    override fun onStartCommand(intent: Intent, flags: Int, startId: Int): Int {
        intent.getStringExtra(NOTIFICATION_TAG_KEY)?.apply {
            NotificationManagerCompat.from(applicationContext)
                .cancel(this, intent.getIntExtra(NOTIFICATION_ID_KEY, 0))
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = CrashNotification.ensureChannelExists(this)
            val notification = NotificationCompat.Builder(this, channel)
                .setContentTitle(
                    getString(
                        R.string.mozac_lib_send_crash_report_in_progress,
                        crashReporter.promptConfiguration.organizationName,
                    ),
                )
                .setSmallIcon(R.drawable.mozac_lib_crash_notification)
                .setPriority(NotificationCompat.PRIORITY_DEFAULT)
                .setCategory(NotificationCompat.CATEGORY_ERROR)
                .setAutoCancel(true)
                .setProgress(0, 0, true)
                .build()

            val notificationId = SharedIdsHelper.getIdForTag(this, NOTIFICATION_TAG)
            startForeground(notificationId, notification)
        }

        NotificationManagerCompat.from(this).cancel(NOTIFICATION_TAG, NOTIFICATION_ID)
        val crash = Crash.fromIntent(intent)
        crashReporter.submitReport(crash) {
            stopSelf()
        }

        return START_NOT_STICKY
    }

    override fun onBind(intent: Intent): IBinder? {
        // We don't provide binding, so return null
        return null
    }

    companion object {
        fun createReportIntent(
            context: Context,
            crash: Crash,
            notificationTag: String? = null,
            notificationId: Int = 0,
        ): Intent {
            val intent = Intent(context, SendCrashReportService::class.java)

            notificationTag?.apply {
                intent.putExtra(NOTIFICATION_TAG_KEY, notificationTag)
                intent.putExtra(NOTIFICATION_ID_KEY, notificationId)
            }

            crash.fillIn(intent)

            return intent
        }
    }
}
