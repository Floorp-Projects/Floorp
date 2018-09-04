/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.IBinder
import android.support.v4.app.NotificationCompat
import android.support.v4.content.ContextCompat

import org.mozilla.focus.R
import org.mozilla.focus.activity.MainActivity
import org.mozilla.focus.telemetry.TelemetryWrapper

/**
 * As long as a session is active this service will keep the notification (and our process) alive.
 */
class SessionNotificationService : Service() {
    override fun onStartCommand(intent: Intent, flags: Int, startId: Int): Int {
        val action = intent.action ?: return Service.START_NOT_STICKY

        when (action) {
            ACTION_START -> {
                createNotificationChannelIfNeeded()
                startForeground(NOTIFICATION_ID, buildNotification())
            }

            ACTION_ERASE -> {
                TelemetryWrapper.eraseNotificationEvent()

                SessionManager.getInstance().removeAllSessions()

                VisibilityLifeCycleCallback.finishAndRemoveTaskIfInBackground(this)
            }

            else -> throw IllegalStateException("Unknown intent: $intent")
        }

        return Service.START_NOT_STICKY
    }

    override fun onTaskRemoved(rootIntent: Intent) {
        TelemetryWrapper.eraseTaskRemoved()

        SessionManager.getInstance().removeAllSessions()

        stopForeground(true)
        stopSelf()
    }

    private fun buildNotification(): Notification {
        return NotificationCompat.Builder(this, NOTIFICATION_CHANNEL_ID)
            .setOngoing(true)
            .setSmallIcon(R.drawable.ic_notification)
            .setContentTitle(getString(R.string.app_name))
            .setContentText(getString(R.string.notification_erase_text))
            .setContentIntent(createNotificationIntent())
            .setVisibility(NotificationCompat.VISIBILITY_SECRET)
            .setShowWhen(false)
            .setLocalOnly(true)
            .setColor(ContextCompat.getColor(this, R.color.colorErase))
            .addAction(
                NotificationCompat.Action(
                    R.drawable.ic_notification,
                    getString(R.string.notification_action_open),
                    createOpenActionIntent()
                )
            )
            .addAction(
                NotificationCompat.Action(
                    R.drawable.ic_delete,
                    getString(R.string.notification_action_erase_and_open),
                    createOpenAndEraseActionIntent()
                )
            )
            .build()
    }

    private fun createNotificationIntent(): PendingIntent {
        val intent = Intent(this, SessionNotificationService::class.java)
        intent.action = ACTION_ERASE

        return PendingIntent.getService(this, 0, intent, PendingIntent.FLAG_ONE_SHOT)
    }

    private fun createOpenActionIntent(): PendingIntent {
        val intent = Intent(this, MainActivity::class.java)
        intent.action = MainActivity.ACTION_OPEN

        return PendingIntent.getActivity(this, 1, intent, PendingIntent.FLAG_UPDATE_CURRENT)
    }

    private fun createOpenAndEraseActionIntent(): PendingIntent {
        val intent = Intent(this, MainActivity::class.java)

        intent.action = MainActivity.ACTION_ERASE
        intent.putExtra(MainActivity.EXTRA_NOTIFICATION, true)
        intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK

        return PendingIntent.getActivity(this, 2, intent, PendingIntent.FLAG_UPDATE_CURRENT)
    }

    private fun createNotificationChannelIfNeeded() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            // Notification channels are only available on Android O or higher.
            return
        }

        val notificationManager = getSystemService(NotificationManager::class.java) ?: return

        val notificationChannelName = getString(R.string.notification_browsing_session_channel_name)
        val notificationChannelDescription = getString(
            R.string.notification_browsing_session_channel_description,
            getString(R.string.app_name)
        )

        val channel = NotificationChannel(
            NOTIFICATION_CHANNEL_ID, notificationChannelName, NotificationManager.IMPORTANCE_MIN
        )
        channel.importance = NotificationManager.IMPORTANCE_LOW
        channel.description = notificationChannelDescription
        channel.enableLights(false)
        channel.enableVibration(false)
        channel.setShowBadge(true)

        notificationManager.createNotificationChannel(channel)
    }

    override fun onBind(intent: Intent): IBinder? {
        return null
    }

    companion object {
        private const val NOTIFICATION_ID = 83
        private const val NOTIFICATION_CHANNEL_ID = "browsing-session"

        private const val ACTION_START = "start"
        private const val ACTION_ERASE = "erase"

        internal fun start(context: Context) {
            val intent = Intent(context, SessionNotificationService::class.java)
            intent.action = ACTION_START

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                context.startForegroundService(intent)
            } else {
                context.startService(intent)
            }
        }

        internal fun stop(context: Context) {
            val intent = Intent(context, SessionNotificationService::class.java)

            context.stopService(intent)
        }
    }
}
