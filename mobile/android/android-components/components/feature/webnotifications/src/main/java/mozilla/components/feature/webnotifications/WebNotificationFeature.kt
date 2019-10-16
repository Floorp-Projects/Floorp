/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.webnotifications

import android.app.Activity
import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import android.os.Build
import androidx.annotation.DrawableRes
import androidx.core.app.NotificationCompat
import androidx.core.content.getSystemService
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.webnotifications.WebNotification
import mozilla.components.concept.engine.webnotifications.WebNotificationDelegate
import mozilla.components.support.base.log.logger.Logger
import java.lang.UnsupportedOperationException

private const val NOTIFICATION_CHANNEL_ID = "mozac.feature.webnotifications.generic.channel"

/**
 * Feature implementation for configuring and displaying web notifications to the user.
 *
 * Initialize this feature globally once on app start
 * ```Kotlin
 * WebNotificationFeature(
 *     applicationContext, engine, icons, R.mipmap.ic_launcher, BrowserActivity::class.java
 * )
 * ```
 *
 * @param context The application Context.
 * @param engine The browser engine.
 * @param browserIcons The entry point for loading the large icon for the notification.
 * @param smallIcon The small icon for the notification.
 * @param activityClass The Activity that the notification will launch if user taps on it
 */
class WebNotificationFeature(
    private val context: Context,
    private val engine: Engine,
    private val browserIcons: BrowserIcons,
    @DrawableRes private val smallIcon: Int,
    private val activityClass: Class<out Activity>?
) : WebNotificationDelegate {
    private val logger = Logger("WebNotificationFeature")
    private var pendingRequestId = 0
    private var notificationId = 0
    private val notificationIdMap = HashMap<String, Int>()
    private val notificationManager = context.getSystemService<NotificationManager>()
    private val nativeNotificationBridge = NativeNotificationBridge(browserIcons, smallIcon)

    init {
        try {
            engine.registerWebNotificationDelegate(this)
        } catch (e: UnsupportedOperationException) {
            logger.error("failed to register for web notification delegate", e)
        }
    }

    override fun onShowNotification(webNotification: WebNotification) {
        ensureNotificationGroupAndChannelExists()

        notificationIdMap[webNotification.tag]?.let {
            notificationManager?.cancel(it)
        }

        pendingRequestId++
        notificationId++
        notificationIdMap[webNotification.tag] = notificationId

        GlobalScope.launch(Dispatchers.IO) {
            val notification = nativeNotificationBridge.convertToAndroidNotification(
                webNotification, context, NOTIFICATION_CHANNEL_ID, activityClass, pendingRequestId)
            notificationManager?.notify(notificationId, notification)
        }
    }

    override fun onCloseNotification(webNotification: WebNotification) {
        notificationIdMap[webNotification.tag]?.let {
            notificationManager?.cancel(it)
        }
    }

    private fun ensureNotificationGroupAndChannelExists() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                NOTIFICATION_CHANNEL_ID,
                context.getString(R.string.mozac_feature_notification_channel_name),
                NotificationManager.IMPORTANCE_LOW
            )
            channel.setShowBadge(true)
            channel.lockscreenVisibility = NotificationCompat.VISIBILITY_PRIVATE

            notificationManager?.createNotificationChannel(channel)
        }
    }
}
