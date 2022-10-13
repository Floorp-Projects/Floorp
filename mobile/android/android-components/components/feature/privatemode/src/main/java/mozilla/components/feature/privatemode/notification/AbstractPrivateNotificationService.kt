/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.privatemode.notification

import android.app.Notification
import android.app.PendingIntent
import android.app.PendingIntent.FLAG_ONE_SHOT
import android.app.Service
import android.content.Intent
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import android.os.IBinder
import androidx.annotation.CallSuper
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationCompat.VISIBILITY_SECRET
import androidx.core.app.NotificationManagerCompat
import androidx.core.app.NotificationManagerCompat.IMPORTANCE_LOW
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.privatemode.R
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.ids.SharedIdsHelper
import mozilla.components.support.ktx.android.notification.ChannelData
import mozilla.components.support.ktx.android.notification.ensureNotificationChannelExists
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import mozilla.components.support.utils.PendingIntentUtils
import java.util.Locale

/**
 * Manages notifications for private tabs.
 *
 * Private tab notifications solve two problems:
 * 1. They allow users to interact with the browser from outside of the app
 *    (example: by closing all private tabs).
 * 2. The notification will keep the process alive, allowing the browser to
 *    keep private tabs in memory.
 *
 * As long as a private tab is open this service will keep its notification alive.
 */
@Suppress("TooManyFunctions")
abstract class AbstractPrivateNotificationService : Service() {

    private var privateTabsScope: CoroutineScope? = null
    private var localeScope: CoroutineScope? = null

    abstract val store: BrowserStore

    /**
     * Customizes the private browsing notification.
     */
    abstract fun NotificationCompat.Builder.buildNotification()

    /**
     * Customize the notification response when the [Locale] has been changed.
     */
    abstract fun notifyLocaleChanged()

    /**
     * Erases all private tabs in reaction to the user tapping the notification.
     */
    @CallSuper
    protected open fun erasePrivateTabs() {
        store.dispatch(TabListAction.RemoveAllPrivateTabsAction)
    }

    /**
     * Retrieves the notification id based on the tag.
     */
    protected fun getNotificationId(): Int {
        return SharedIdsHelper.getIdForTag(this, NOTIFICATION_TAG)
    }

    /**
     * Retrieves the channel id based on the channel data.
     */
    protected fun getChannelId(): String {
        return ensureNotificationChannelExists(
            this,
            NOTIFICATION_CHANNEL,
            onSetupChannel = {
                if (SDK_INT >= Build.VERSION_CODES.O) {
                    enableLights(false)
                    enableVibration(false)
                    setShowBadge(false)
                }
            },
        )
    }

    /**
     * Re-build and notify an existing notification.
     */
    protected fun refreshNotification() {
        val notificationId = getNotificationId()
        val channelId = getChannelId()

        val notification = createNotification(channelId)
        NotificationManagerCompat.from(applicationContext).notify(notificationId, notification)
    }

    /**
     * Create the private browsing notification and
     * add a listener to stop the service once all private tabs are closed.
     *
     * The service should be started only if private tabs are open.
     */
    final override fun onCreate() {
        val notificationId = getNotificationId()
        val channelId = getChannelId()
        val notification = createNotification(channelId)

        startForeground(notificationId, notification)

        privateTabsScope = store.flowScoped { flow ->
            flow.map { state -> state.privateTabs.isEmpty() }
                .ifChanged()
                .collect { noPrivateTabs ->
                    if (noPrivateTabs) stopService()
                }
        }

        localeScope = store.flowScoped { flow ->
            flow.mapNotNull { state -> state.locale }
                .ifChanged()
                .collect {
                    notifyLocaleChanged()
                }
        }
    }

    /**
     * Builds a notification based on the specified channel id.
     *
     * @param channelId The channel id for the [Notification]
     */
    fun createNotification(channelId: String): Notification {
        return NotificationCompat.Builder(this, channelId)
            .setOngoing(true)
            .setVisibility(VISIBILITY_SECRET)
            .setShowWhen(false)
            .setLocalOnly(true)
            .setContentIntent(
                Intent(ACTION_ERASE).let {
                    it.setClass(this, this::class.java)
                    PendingIntent.getService(this, 0, it, PendingIntentUtils.defaultFlags or FLAG_ONE_SHOT)
                },
            )
            .apply { buildNotification() }
            .build()
    }

    final override fun onStartCommand(intent: Intent, flags: Int, startId: Int): Int {
        if (intent.action == ACTION_ERASE) {
            erasePrivateTabs()
        }

        return START_NOT_STICKY
    }

    final override fun onDestroy() {
        privateTabsScope?.cancel()
        localeScope?.cancel()
    }

    final override fun onBind(intent: Intent?): IBinder? = null

    final override fun onTaskRemoved(rootIntent: Intent) {
        if (rootIntent.action in ignoreTaskActions || rootIntent.component?.className in ignoreTaskComponentClasses) {
            // The app may have multiple tasks (e.g. for PWAs). If tasks get removed that are not
            // the main browser task then we do not want to remove all private tabs here.
            // I am not sure whether we can reliably identify the main task since it can be launched
            // from multiple entry points (e.g. the launcher, VIEW intents, ..).
            // So instead we ignore tasks with root intents that we absolutely do not want to handle
            // here (e.g. PWAs) and then extend the list if needed.
            return
        }

        store.dispatch(TabListAction.RemoveAllPrivateTabsAction)
        stopService()
    }

    private fun stopService() {
        stopForeground(true)
        stopSelf()
    }

    companion object {
        private const val NOTIFICATION_TAG =
            "mozilla.components.feature.privatemode.notification.AbstractPrivateNotificationService"
        const val ACTION_ERASE = "mozilla.components.feature.privatemode.action.ERASE"

        val NOTIFICATION_CHANNEL = ChannelData(
            id = "browsing-session",
            name = R.string.mozac_feature_privatemode_notification_channel_name,
            importance = IMPORTANCE_LOW,
        )

        // List of Intent actions that will get ignored when they are in the root intent that gets
        // passed to onTaskRemoved().
        private val ignoreTaskActions = listOf(
            "mozilla.components.feature.pwa.VIEW_PWA",
        )

        // List of Intent components classes that will get ignored when they are in the root intent
        // that gets passed to onTaskRemoved().
        private val ignoreTaskComponentClasses = listOf(
            "org.mozilla.fenix.customtabs.ExternalAppBrowserActivity",
        )
    }
}
