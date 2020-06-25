/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.session

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
import androidx.core.content.getSystemService
import mozilla.components.browser.session.SessionManager
import mozilla.components.support.utils.ThreadUtils
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.sessionsOfType

/**
 * Manages notifications for private tabs.
 *
 * Private tab notifications solve two problems for us:
 * 1 - They allow users to interact with us from outside of the app (example: by closing all
 * private tabs).
 * 2 - The notification will keep our process alive, allowing us to keep private tabs in memory.
 *
 * As long as a session is active this service will keep its notification alive.
 */
class SessionNotificationService : Service() {

    private var isStartedFromPrivateShortcut: Boolean = false

    override fun onStartCommand(intent: Intent, flags: Int, startId: Int): Int {
        val action = intent.action ?: return START_NOT_STICKY

        when (action) {
            ACTION_START -> {
                isStartedFromPrivateShortcut = intent.getBooleanExtra(STARTED_FROM_PRIVATE_SHORTCUT, false)
                createNotificationChannelIfNeeded()
                startForeground(NOTIFICATION_ID, buildNotification())
            }

            ACTION_ERASE -> {
                metrics.track(Event.PrivateBrowsingNotificationTapped)

                val homeScreenIntent = Intent(this, HomeActivity::class.java)
                val intentFlags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
                homeScreenIntent.apply {
                    setFlags(intentFlags)
                    putExtra(HomeActivity.PRIVATE_BROWSING_MODE, isStartedFromPrivateShortcut)
                }
                if (VisibilityLifecycleCallback.finishAndRemoveTaskIfInBackground(this)) {
                    // Set start mode to be in background (recents screen)
                    homeScreenIntent.apply {
                        putExtra(HomeActivity.START_IN_RECENTS_SCREEN, true)
                    }
                }
                startActivity(homeScreenIntent)
                components.core.sessionManager.removeAndCloseAllPrivateSessions()
            }

            else -> throw IllegalStateException("Unknown intent: $intent")
        }

        return START_NOT_STICKY
    }

    override fun onTaskRemoved(rootIntent: Intent) {
        components.core.sessionManager.removeAndCloseAllPrivateSessions()

        stopForeground(true)
        stopSelf()
    }

    private fun buildNotification(): Notification {
        return NotificationCompat.Builder(this, NOTIFICATION_CHANNEL_ID)
            .setOngoing(true)
            .setSmallIcon(R.drawable.ic_pbm_notification)
            .setContentTitle(getString(R.string.app_name_private_4, getString(R.string.app_name)))
            .setContentText(getString(R.string.notification_pbm_delete_text_2))
            .setContentIntent(createNotificationIntent())
            .setVisibility(NotificationCompat.VISIBILITY_SECRET)
            .setShowWhen(false)
            .setLocalOnly(true)
            .setColor(ContextCompat.getColor(this, R.color.pbm_notification_color))
            .build()
    }

    private fun createNotificationIntent(): PendingIntent {
        val intent = Intent(this, SessionNotificationService::class.java)
        intent.action = ACTION_ERASE

        return PendingIntent.getService(this, 0, intent, PendingIntent.FLAG_ONE_SHOT)
    }

    private fun createNotificationChannelIfNeeded() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            // Notification channels are only available on Android O or higher.
            return
        }

        val notificationManager = getSystemService<NotificationManager>() ?: return

        val notificationChannelName = getString(R.string.notification_pbm_channel_name)

        val channel = NotificationChannel(
            NOTIFICATION_CHANNEL_ID, notificationChannelName, NotificationManager.IMPORTANCE_MIN
        )
        channel.importance = NotificationManager.IMPORTANCE_LOW
        channel.enableLights(false)
        channel.enableVibration(false)
        channel.setShowBadge(false)

        notificationManager.createNotificationChannel(channel)
    }

    private fun SessionManager.removeAndCloseAllPrivateSessions() {
        sessionsOfType(private = true).forEach { remove(it) }
    }

    override fun onBind(intent: Intent): IBinder? {
        return null
    }

    companion object {
        private const val NOTIFICATION_ID = 83
        private const val NOTIFICATION_CHANNEL_ID = "browsing-session"
        private const val STARTED_FROM_PRIVATE_SHORTCUT = "STARTED_FROM_PRIVATE_SHORTCUT"

        private const val ACTION_START = "start"
        private const val ACTION_ERASE = "erase"

        internal fun start(
            context: Context,
            startedFromPrivateShortcut: Boolean
        ) {
            val intent = Intent(context, SessionNotificationService::class.java)
            intent.action = ACTION_START
            intent.putExtra(STARTED_FROM_PRIVATE_SHORTCUT, startedFromPrivateShortcut)

            // From Focus #2901: The application is crashing due to the service not calling `startForeground`
            // before it times out. This is a speculative fix to decrease the time between these two
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
