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
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.R
import mozilla.components.lib.crash.notification.CrashNotification
import mozilla.components.lib.crash.notification.NOTIFICATION_TAG
import mozilla.components.support.base.ids.NotificationIds
import mozilla.components.support.base.ids.cancel
import kotlin.coroutines.CoroutineContext
import kotlin.coroutines.EmptyCoroutineContext

class SendCrashReportService : Service() {
    private val crashReporter: CrashReporter by lazy { CrashReporter.requireInstance }
    private val logger by lazy { CrashReporter
            .requireInstance
            .logger
    }

    private var reporterCoroutineContext: CoroutineContext = EmptyCoroutineContext

    override fun onStartCommand(intent: Intent, flags: Int, startId: Int): Int {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = CrashNotification.ensureChannelExists(this)
            val notification = NotificationCompat.Builder(this, channel)
                    .setContentTitle(getString(R.string.mozac_lib_send_crash_report_in_progress,
                            crashReporter.promptConfiguration.organizationName))
                    .setSmallIcon(R.drawable.mozac_lib_crash_notification)
                    .setPriority(NotificationCompat.PRIORITY_DEFAULT)
                    .setCategory(NotificationCompat.CATEGORY_ERROR)
                    .setAutoCancel(true)
                    .setProgress(0, 0, true)
                    .build()

            val notificationId = NotificationIds.getIdForTag(this, NOTIFICATION_TAG)
            startForeground(notificationId, notification)
        }

        intent.extras?.let { extras ->
            val crash = Crash.NativeCodeCrash.fromBundle(extras)
            NotificationManagerCompat.from(this).cancel(this, NOTIFICATION_TAG)

            sendCrashReport(crash) {
                stopSelf()
            }
        } ?: logger.error("Received intent with null extras")

        return START_NOT_STICKY
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun sendCrashReport(crash: Crash, then: () -> Unit) {
        GlobalScope.launch(reporterCoroutineContext) {
            crashReporter.submitReport(crash)

            withContext(Dispatchers.Main) {
                then()
            }
        }
    }

    override fun onBind(intent: Intent): IBinder? {
        // We don't provide binding, so return null
        return null
    }

    companion object {
        fun createReportIntent(context: Context, crash: Crash): Intent {
            val intent = Intent(context, SendCrashReportService::class.java)
            crash.fillIn(intent)

            return intent
        }
    }
}
