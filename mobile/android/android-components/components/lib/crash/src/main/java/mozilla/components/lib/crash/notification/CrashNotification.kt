/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.notification

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.os.Build
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.R
import mozilla.components.lib.crash.prompt.CrashPrompt
import mozilla.components.lib.crash.service.SendCrashReportService
import mozilla.components.support.base.ids.notify

private const val NOTIFICATION_SDK_LEVEL = 29 // On Android Q+ we show a notification instead of a prompt

internal const val NOTIFICATION_CHANNEL_ID = "Crashes"
internal const val NOTIFICATION_TAG = "mozac.lib.crash.CRASH"

internal class CrashNotification(
    private val context: Context,
    private val crash: Crash,
    private val configuration: CrashReporter.PromptConfiguration
) {
    fun show() {
        val pendingIntent = PendingIntent.getActivity(
                context, 0, CrashPrompt.createIntent(context, crash), 0
        )

        val reportPendingIntent = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            PendingIntent.getForegroundService(
                    context, 0, SendCrashReportService.createReportIntent(context, crash), 0
            )
        } else {
            PendingIntent.getService(
                    context, 0, SendCrashReportService.createReportIntent(context, crash), 0
            )
        }

        val channel = ensureChannelExists(context)

        val notification = NotificationCompat.Builder(context, channel)
            .setContentTitle(context.getString(R.string.mozac_lib_crash_dialog_title, configuration.appName))
            .setSmallIcon(R.drawable.mozac_lib_crash_notification)
            .setPriority(NotificationCompat.PRIORITY_DEFAULT)
            .setCategory(NotificationCompat.CATEGORY_ERROR)
            .setContentIntent(pendingIntent)
            .addAction(R.drawable.mozac_lib_crash_notification, context.getString(
                    R.string.mozac_lib_crash_notification_action_report), reportPendingIntent)
            .setAutoCancel(true)
            .build()

        NotificationManagerCompat.from(context)
            .notify(context, NOTIFICATION_TAG, notification)
    }

    companion object {
        /**
         * Whether to show a notification instead of a prompt (activity). Android introduced restrictions on background
         * services launching activities in Q+. On those system we may need to show a notification for the given [crash]
         * and launch the reporter from the notification.
         */
        fun shouldShowNotificationInsteadOfPrompt(crash: Crash, sdkLevel: Int = Build.VERSION.SDK_INT): Boolean {
            return when {
                // We can always launch an activity from a background service pre Android Q.
                sdkLevel < NOTIFICATION_SDK_LEVEL -> false

                // An uncaught exception is crashing the app and we may not be able to launch an activity from here.
                crash is Crash.UncaughtExceptionCrash -> true

                // This is a fatal native crash. We may not be able to launch an activity from here.
                else -> crash is Crash.NativeCodeCrash && crash.isFatal
            }
        }

        fun ensureChannelExists(context: Context): String {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                val notificationManager: NotificationManager = context.getSystemService(
                        Context.NOTIFICATION_SERVICE
                ) as NotificationManager

                val channel = NotificationChannel(
                        NOTIFICATION_CHANNEL_ID,
                        context.getString(R.string.mozac_lib_crash_channel),
                        NotificationManager.IMPORTANCE_DEFAULT
                )

                notificationManager.createNotificationChannel(channel)
            }

            return NOTIFICATION_CHANNEL_ID
        }
    }
}
