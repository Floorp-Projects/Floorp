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
import android.graphics.Bitmap
import android.graphics.drawable.Icon
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationCompat.BADGE_ICON_NONE
import androidx.core.app.NotificationManagerCompat
import androidx.core.content.getSystemService
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.launch
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.extension.toMonochromeIconRequest
import mozilla.components.browser.state.selector.findCustomTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.feature.pwa.R
import mozilla.components.feature.session.SessionUseCases

/**
 * Displays site controls notification for fullscreen web apps.
 * @param sessionId ID of the web app session to observe.
 * @param manifest Web App Manifest reference used to populate the notification.
 * @param controlsBuilder Customizes the created notification.
 */
@Suppress("LongParameterList")
class WebAppSiteControlsFeature(
    private val applicationContext: Context,
    private val store: BrowserStore,
    private val sessionId: String,
    private val manifest: WebAppManifest? = null,
    private val controlsBuilder: SiteControlsBuilder = SiteControlsBuilder.Default(),
    private val icons: BrowserIcons? = null,
) : BroadcastReceiver(), DefaultLifecycleObserver {

    constructor(
        applicationContext: Context,
        store: BrowserStore,
        reloadUrlUseCase: SessionUseCases.ReloadUrlUseCase,
        sessionId: String,
        manifest: WebAppManifest? = null,
        controlsBuilder: SiteControlsBuilder = SiteControlsBuilder.CopyAndRefresh(reloadUrlUseCase),
        icons: BrowserIcons? = null,
    ) : this(
        applicationContext,
        store,
        sessionId,
        manifest,
        controlsBuilder,
        icons,
    )

    private var notificationIcon: Deferred<mozilla.components.browser.icons.Icon>? = null

    /**
     * Starts loading the [notificationIcon] on create.
     */
    override fun onCreate(owner: LifecycleOwner) {
        if (SDK_INT >= Build.VERSION_CODES.M && manifest != null && icons != null) {
            val request = manifest.toMonochromeIconRequest()
            if (request.resources.isNotEmpty()) {
                notificationIcon = icons.loadIcon(request)
            }
        }
    }

    /**
     * Displays a notification from the given [SiteControlsBuilder.buildNotification] that will be
     * shown as long as the lifecycle is in the foreground. Registers this class as a broadcast
     * receiver to receive events from the notification and call [SiteControlsBuilder.onReceiveBroadcast].
     */
    override fun onResume(owner: LifecycleOwner) {
        val filter = controlsBuilder.getFilter()
        applicationContext.registerReceiver(this, filter)

        val manager = NotificationManagerCompat.from(applicationContext)
        val iconAsync = notificationIcon
        if (iconAsync != null) {
            owner.lifecycleScope.launch {
                val bitmap = iconAsync.await().bitmap
                manager.notify(NOTIFICATION_TAG, NOTIFICATION_ID, buildNotification(bitmap))
            }
        } else {
            manager.notify(NOTIFICATION_TAG, NOTIFICATION_ID, buildNotification(null))
        }
    }

    /**
     * Cancels the site controls notification and unregisters the broadcast receiver.
     */
    override fun onPause(owner: LifecycleOwner) {
        applicationContext.unregisterReceiver(this)

        NotificationManagerCompat.from(applicationContext)
            .cancel(NOTIFICATION_TAG, NOTIFICATION_ID)
    }

    /**
     * Cancels the [notificationIcon] loading job on destroy.
     */
    override fun onDestroy(owner: LifecycleOwner) {
        notificationIcon?.cancel()
    }

    /**
     * Responds to [PendingIntent]s fired by the site controls notification.
     */
    override fun onReceive(context: Context, intent: Intent) {
        store.state.findCustomTab(sessionId)?.also { tab ->
            controlsBuilder.onReceiveBroadcast(context, tab, intent)
        }
    }

    /**
     * Build the notification with site controls to be displayed while the web app is active.
     */
    private fun buildNotification(icon: Bitmap?): Notification {
        val builder = if (SDK_INT >= Build.VERSION_CODES.O) {
            val channelId = ensureChannelExists()
            Notification.Builder(applicationContext, channelId).apply {
                setBadgeIconType(BADGE_ICON_NONE)
            }
        } else {
            @Suppress("Deprecation")
            Notification.Builder(applicationContext).apply {
                setPriority(Notification.PRIORITY_MIN)
            }
        }
        if (icon != null && SDK_INT >= Build.VERSION_CODES.M) {
            builder.setSmallIcon(Icon.createWithBitmap(icon))
        } else {
            builder.setSmallIcon(R.drawable.ic_pwa)
        }
        builder.setContentTitle(manifest?.name ?: manifest?.shortName)
        builder.setColor(manifest?.themeColor ?: NotificationCompat.COLOR_DEFAULT)
        builder.setShowWhen(false)
        builder.setOngoing(true)
        controlsBuilder.buildNotification(applicationContext, builder)
        return builder.build()
    }

    /**
     * Make sure a notification channel for site controls notifications exists.
     *
     * Returns the channel id to be used for notifications.
     */
    private fun ensureChannelExists(): String {
        if (SDK_INT >= Build.VERSION_CODES.O) {
            val notificationManager: NotificationManager = applicationContext.getSystemService()!!

            val channel = NotificationChannel(
                NOTIFICATION_CHANNEL_ID,
                applicationContext.getString(R.string.mozac_feature_pwa_site_controls_notification_channel),
                NotificationManager.IMPORTANCE_MIN,
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
