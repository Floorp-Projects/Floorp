/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.feature

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.os.Build
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

/**
 * Displays site controls notification for fullscreen web apps.
 * @param sessionId ID of the web app session to observe.
 * @param manifest Web App Manifest reference used to populate the notification.
 * @param controlsBuilder Customizes the created notification.
 */
class WebAppSiteControlsFeature(
    private val applicationContext: Context,
    private val sessionManager: SessionManager,
    private val sessionId: String,
    private val manifest: WebAppManifest? = null,
    private val controlsBuilder: SiteControlsBuilder = SiteControlsBuilder.Default()
) : BroadcastReceiver(), LifecycleObserver {

    constructor(
        applicationContext: Context,
        sessionManager: SessionManager,
        reloadUrlUseCase: SessionUseCases.ReloadUrlUseCase,
        sessionId: String,
        manifest: WebAppManifest? = null,
        controlsBuilder: SiteControlsBuilder = SiteControlsBuilder.CopyAndRefresh(reloadUrlUseCase)
    ) : this(
        applicationContext,
        sessionManager,
        sessionId,
        manifest,
        controlsBuilder
    )

    @OnLifecycleEvent(Lifecycle.Event.ON_RESUME)
    fun onResume() {
        val filter = controlsBuilder.getFilter()
        applicationContext.registerReceiver(this, filter)

        NotificationManagerCompat.from(applicationContext)
            .notify(NOTIFICATION_TAG, NOTIFICATION_ID, buildNotification())
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_PAUSE)
    fun onPause() {
        applicationContext.unregisterReceiver(this)

        NotificationManagerCompat.from(applicationContext)
            .cancel(NOTIFICATION_TAG, NOTIFICATION_ID)
    }

    /**
     * Responds to [PendingIntent]s fired by the site controls notification.
     */
    override fun onReceive(context: Context, intent: Intent) {
        sessionManager.findSessionById(sessionId)?.also { session ->
            controlsBuilder.onReceiveBroadcast(context, session, intent)
        }
    }

    /**
     * Build the notification with site controls to be displayed while the web app is active.
     */
    private fun buildNotification(): Notification {
        val channelId = ensureChannelExists()

        return NotificationCompat.Builder(applicationContext, channelId)
            .setSmallIcon(R.drawable.ic_pwa)
            .setContentTitle(manifest?.name ?: manifest?.shortName)
            .setBadgeIconType(BADGE_ICON_NONE)
            .setColor(manifest?.themeColor ?: NotificationCompat.COLOR_DEFAULT)
            .setPriority(NotificationCompat.PRIORITY_MIN)
            .setShowWhen(false)
            .setOngoing(true)
            .also { controlsBuilder.buildNotification(applicationContext, it, channelId) }
            .build()
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

    companion object {
        private const val NOTIFICATION_CHANNEL_ID = "Site Controls"
        private const val NOTIFICATION_TAG = "SiteControls"
        private const val NOTIFICATION_ID = 1
    }
}
