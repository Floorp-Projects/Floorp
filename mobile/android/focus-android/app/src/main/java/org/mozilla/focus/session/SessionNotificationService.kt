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
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import mozilla.components.support.utils.ThreadUtils

import org.mozilla.focus.R
import org.mozilla.focus.activity.MainActivity
import org.mozilla.focus.ext.components
import org.mozilla.focus.telemetry.TelemetryWrapper

/**
 * As long as a session is active this service will keep the notification (and our process) alive.
 */
class SessionNotificationService : Service() {

    private var shouldSendTaskRemovedTelemetry = true

    override fun onStartCommand(intent: Intent, flags: Int, startId: Int): Int {
        val action = intent.action ?: return Service.START_NOT_STICKY

        when (action) {
            ACTION_START -> {
                createNotificationChannelIfNeeded()
                startForeground(NOTIFICATION_ID, buildNotification())
            }

            ACTION_ERASE -> {
                TelemetryWrapper.eraseNotificationEvent()
                shouldSendTaskRemovedTelemetry = false

                if (VisibilityLifeCycleCallback.isInBackground(this)) {
                    VisibilityLifeCycleCallback.finishAndRemoveTaskIfInBackground(this)
                } else {
                    val eraseIntent = Intent(this, MainActivity::class.java)

                    eraseIntent.action = MainActivity.ACTION_ERASE
                    eraseIntent.putExtra(MainActivity.EXTRA_NOTIFICATION, true)
                    eraseIntent.flags = Intent.FLAG_ACTIVITY_NEW_TASK

                    startActivity(eraseIntent)
                }
            }

            else -> throw IllegalStateException("Unknown intent: $intent")
        }

        return Service.START_NOT_STICKY
    }

    override fun onTaskRemoved(rootIntent: Intent) {
        // Do not double send telemetry for notification erase event
        if (shouldSendTaskRemovedTelemetry) {
            TelemetryWrapper.eraseTaskRemoved()
        }

        components.sessionManager.removeSessions()

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
        channel.setShowBadge(false)

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

            // For #2901: The application is crashing due to the service not calling `startForeground`
            // before it times out. so this is a speculative fix to decrease the time between these two
            // calls by running this after potentially expensive calls in FocusApplication.onCreate and
            // BrowserFragment.inflateView by posting it to the end of the main thread.
            ThreadUtils.postToMainThread(Runnable {
                context.startService(intent)
            })
        }

        internal fun stop(context: Context) {
            val intent = Intent(context, SessionNotificationService::class.java)

            // We want to make sure we always call stop after start. So we're
            // putting these actions on the same sequential run queue.
            ThreadUtils.postToMainThread(Runnable {
                context.stopService(intent)
            })
        }
    }
}
