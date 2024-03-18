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
import android.os.RemoteException
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import mozilla.components.service.glean.private.NoExtras
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.utils.ThreadUtils
import mozilla.components.support.utils.ext.stopForegroundCompat
import org.mozilla.focus.GleanMetrics.Notifications
import org.mozilla.focus.GleanMetrics.RecentApps
import org.mozilla.focus.R
import org.mozilla.focus.activity.MainActivity
import org.mozilla.focus.ext.components
import org.mozilla.focus.utils.IntentUtils

/**
 * As long as a session is active this service will keep the notification (and our process) alive.
 */
class SessionNotificationService : Service() {

    private var shouldSendTaskRemovedTelemetry = true

    private val notificationManager: NotificationManager by lazy {
        applicationContext.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
    }

    override fun onStartCommand(intent: Intent, flags: Int, startId: Int): Int {
        val action = intent.action ?: return START_NOT_STICKY

        when (action) {
            ACTION_START -> {
                // Notifications permission is only needed on Android 13 devices and later.
                val areNotificationsEnabled =
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                        try {
                            notificationManager.areNotificationsEnabled()
                        } catch (e: RemoteException) {
                            Logger.warn("Failed to check notifications state", e)
                            false
                        }
                    } else {
                        true
                    }

                if (areNotificationsEnabled) {
                    createNotificationChannelIfNeeded()
                    startForeground(NOTIFICATION_ID, buildNotification())
                } else {
                    permissionHandler?.invoke()
                }
            }

            ACTION_ERASE -> {
                Notifications.notificationTapped.record(NoExtras())

                shouldSendTaskRemovedTelemetry = false

                if (VisibilityLifeCycleCallback.isInBackground(this)) {
                    VisibilityLifeCycleCallback.finishAndRemoveTaskIfInBackground(this)
                } else {
                    val eraseIntent = Intent(this, MainActivity::class.java)

                    eraseIntent.action = MainActivity.ACTION_ERASE
                    eraseIntent.flags = Intent.FLAG_ACTIVITY_NEW_TASK

                    startActivity(eraseIntent)
                }
            }

            else -> throw IllegalStateException("Unknown intent: $intent")
        }

        return START_NOT_STICKY
    }

    override fun onTaskRemoved(rootIntent: Intent) {
        // Do not double send telemetry for notification erase event
        if (shouldSendTaskRemovedTelemetry) {
            RecentApps.appRemovedFromList.record(NoExtras())
        }

        components.tabsUseCases.removeAllTabs()

        stopForegroundCompat(true)
        stopSelf()
    }

    private fun buildNotification(): Notification {
        val eraseIntent = createEraseIntent()
        val contentTitle = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            getString(R.string.notification_erase_title_android_14)
        } else {
            getString(R.string.app_name)
        }

        val contentText = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            getString(R.string.notification_erase_text_android_14_1)
        } else {
            getString(R.string.notification_erase_text)
        }

        return NotificationCompat.Builder(this, NOTIFICATION_CHANNEL_ID)
            .setOngoing(true)
            .setSmallIcon(R.drawable.ic_notification)
            .setContentTitle(contentTitle)
            .setContentText(contentText)
            .setContentIntent(eraseIntent)
            .setVisibility(NotificationCompat.VISIBILITY_SECRET)
            .setShowWhen(false)
            .setLocalOnly(true)
            .setColor(ContextCompat.getColor(this, R.color.accentBright))
            .addAction(
                NotificationCompat.Action(
                    R.drawable.ic_notification,
                    getString(R.string.notification_action_open),
                    createOpenActionIntent(),
                ),
            )
            .addAction(
                NotificationCompat.Action(
                    R.drawable.mozac_ic_delete_24,
                    getString(R.string.notification_action_erase_and_open),
                    createOpenAndEraseActionIntent(),
                ),
            )
            .apply {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                    setDeleteIntent(eraseIntent)
                }
            }
            .build()
    }

    private fun createEraseIntent(): PendingIntent {
        val notificationIntentFlags =
            IntentUtils.defaultIntentPendingFlags() or PendingIntent.FLAG_ONE_SHOT
        val intent = Intent(this, SessionNotificationService::class.java)
        intent.action = ACTION_ERASE

        return PendingIntent.getService(this, 0, intent, notificationIntentFlags)
    }

    private fun createOpenActionIntent(): PendingIntent {
        val openActionIntentFlags =
            IntentUtils.defaultIntentPendingFlags() or PendingIntent.FLAG_UPDATE_CURRENT
        val intent = Intent(this, MainActivity::class.java)
        intent.action = MainActivity.ACTION_OPEN

        return PendingIntent.getActivity(this, 1, intent, openActionIntentFlags)
    }

    private fun createOpenAndEraseActionIntent(): PendingIntent {
        val openAndEraseActionIntentFlags =
            IntentUtils.defaultIntentPendingFlags() or PendingIntent.FLAG_UPDATE_CURRENT
        val intent = Intent(this, MainActivity::class.java)

        intent.action = MainActivity.ACTION_ERASE
        intent.putExtra(MainActivity.EXTRA_NOTIFICATION, true)
        intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP

        return PendingIntent.getActivity(this, 2, intent, openAndEraseActionIntentFlags)
    }

    private fun createNotificationChannelIfNeeded() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            // Notification channels are only available on Android O or higher.
            return
        }

        val notificationChannelName = getString(R.string.notification_browsing_session_channel_name)
        val notificationChannelDescription = getString(
            R.string.notification_browsing_session_channel_description,
            getString(R.string.app_name),
        )

        val channel = NotificationChannel(
            NOTIFICATION_CHANNEL_ID,
            notificationChannelName,
            NotificationManager.IMPORTANCE_MIN,
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
        private var permissionHandler: (() -> Unit)? = null
        private const val NOTIFICATION_ID = 83
        private const val NOTIFICATION_CHANNEL_ID = "browsing-session"

        private const val ACTION_START = "start"
        private const val ACTION_ERASE = "erase"

        internal fun start(context: Context, permissionHandler: (() -> Unit)) {
            val intent = Intent(context, SessionNotificationService::class.java)
            intent.action = ACTION_START
            this.permissionHandler = permissionHandler

            // For #2901: The application is crashing due to the service not calling `startForeground`
            // before it times out. so this is a speculative fix to decrease the time between these two
            // calls by running this after potentially expensive calls in FocusApplication.onCreate and
            // BrowserFragment.inflateView by posting it to the end of the main thread.
            ThreadUtils.postToMainThread {
                context.startService(intent)
            }
        }

        internal fun stop(context: Context) {
            val intent = Intent(context, SessionNotificationService::class.java)

            // We want to make sure we always call stop after start. So we're
            // putting these actions on the same sequential run queue.
            ThreadUtils.postToMainThread {
                context.stopService(intent)
            }
        }
    }
}
