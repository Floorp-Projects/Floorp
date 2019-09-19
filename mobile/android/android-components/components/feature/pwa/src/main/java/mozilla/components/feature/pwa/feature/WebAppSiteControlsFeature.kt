/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.feature

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import android.widget.Toast
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationCompat.BADGE_ICON_NONE
import androidx.core.app.NotificationManagerCompat
import androidx.core.content.getSystemService
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.OnLifecycleEvent
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.feature.pwa.R
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.base.ids.cancel
import mozilla.components.support.base.ids.notify

/**
 * Displays site controls notification for fullscreen web apps.
 */
class WebAppSiteControlsFeature(
    private val applicationContext: Context,
    private val sessionManager: SessionManager,
    private val reloadUrlUseCase: SessionUseCases.ReloadUrlUseCase,
    private val sessionId: String,
    private val manifest: WebAppManifest?
) : BroadcastReceiver(), LifecycleObserver {

    @OnLifecycleEvent(Lifecycle.Event.ON_RESUME)
    fun onResume() {
        val filter = IntentFilter().apply {
            addAction(ACTION_COPY)
            addAction(ACTION_REFRESH)
        }
        applicationContext.registerReceiver(this, filter)

        NotificationManagerCompat.from(applicationContext)
            .notify(applicationContext, NOTIFICATION_TAG, buildNotification())
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_PAUSE)
    fun onPause() {
        applicationContext.unregisterReceiver(this)

        NotificationManagerCompat.from(applicationContext)
            .cancel(applicationContext, NOTIFICATION_TAG)
    }

    /**
     * Responds to [PendingIntent]s fired by the site controls notification.
     */
    override fun onReceive(context: Context, intent: Intent) {
        sessionManager.findSessionById(sessionId)?.also { session ->
            when (intent.action) {
                ACTION_COPY -> {
                    applicationContext.getSystemService<ClipboardManager>()?.let { clipboardManager ->
                        clipboardManager.setPrimaryClip(ClipData.newPlainText(session.url, session.url))
                        Toast.makeText(
                            applicationContext,
                            applicationContext.getString(R.string.mozac_feature_pwa_copy_success),
                            Toast.LENGTH_SHORT
                        ).show()
                    }
                }
                ACTION_REFRESH -> reloadUrlUseCase(session)
            }
        }
    }

    /**
     * Build the notification with site controls to be displayed while the web app is active.
     */
    private fun buildNotification(): Notification {
        val channelId = ensureChannelExists()

        with(applicationContext) {
            val copyIntent = createPendingIntent(ACTION_COPY, 1)
            val refreshAction = NotificationCompat.Action(
                R.drawable.ic_refresh,
                getString(R.string.mozac_feature_pwa_site_controls_refresh),
                createPendingIntent(ACTION_REFRESH, 2)
            )

            return NotificationCompat.Builder(this, channelId)
                .setSmallIcon(R.drawable.ic_pwa)
                .setContentTitle(manifest?.name)
                .setContentText(getString(R.string.mozac_feature_pwa_site_controls_notification_text))
                .setBadgeIconType(BADGE_ICON_NONE)
                .setColor(manifest?.themeColor ?: NotificationCompat.COLOR_DEFAULT)
                .setPriority(NotificationCompat.PRIORITY_MIN)
                .setShowWhen(false)
                .setContentIntent(copyIntent)
                .addAction(refreshAction)
                .setOngoing(true)
                .build()
        }
    }

    /**
     * Make sure a notification channel for site controls notifications exists.
     *
     * Returns the channel id to be used for notifications.
     */
    private fun ensureChannelExists(): String {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val notificationManager: NotificationManager = applicationContext.getSystemService()!!

            val channel = NotificationChannel(
                NOTIFICATION_CHANNEL_ID,
                applicationContext.getString(R.string.mozac_feature_pwa_site_controls_notification_channel),
                NotificationManager.IMPORTANCE_MIN
            )

            notificationManager.createNotificationChannel(channel)
        }

        return NOTIFICATION_CHANNEL_ID
    }

    private fun createPendingIntent(action: String, requestCode: Int): PendingIntent {
        val intent = Intent(action)
        intent.setPackage(applicationContext.packageName)
        return PendingIntent.getBroadcast(applicationContext, requestCode, intent, 0)
    }

    companion object {
        private const val NOTIFICATION_CHANNEL_ID = "Site Controls"
        private const val NOTIFICATION_TAG = "SiteControls"
        private const val ACTION_COPY = "mozilla.components.feature.pwa.COPY"
        private const val ACTION_REFRESH = "mozilla.components.feature.pwa.REFRESH"
    }
}
