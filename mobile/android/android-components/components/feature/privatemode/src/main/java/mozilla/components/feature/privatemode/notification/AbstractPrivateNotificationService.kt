/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.privatemode.notification

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
import androidx.core.app.NotificationManagerCompat.IMPORTANCE_LOW
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.privatemode.R
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.ids.SharedIdsHelper
import mozilla.components.support.ktx.android.notification.ChannelData
import mozilla.components.support.ktx.android.notification.ensureNotificationChannelExists
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged

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
abstract class AbstractPrivateNotificationService : Service() {

    private var scope: CoroutineScope? = null

    abstract val store: BrowserStore

    /**
     * Customizes the private browsing notification.
     */
    abstract fun NotificationCompat.Builder.buildNotification()

    /**
     * Erases all private tabs in reaction to the user tapping the notification.
     */
    @CallSuper
    protected open fun erasePrivateTabs() {
        store.dispatch(TabListAction.RemoveAllPrivateTabsAction)
    }

    /**
     * Create the private browsing notification and
     * add a listener to stop the service once all private tabs are closed.
     *
     * The service should be started only if private tabs are open.
     */
    @ExperimentalCoroutinesApi
    final override fun onCreate() {
        val id = SharedIdsHelper.getIdForTag(this, NOTIFICATION_TAG)
        val channelId = ensureNotificationChannelExists(this, NOTIFICATION_CHANNEL, onSetupChannel = {
            if (SDK_INT >= Build.VERSION_CODES.O) {
                enableLights(false)
                enableVibration(false)
                setShowBadge(false)
            }
        })

        val notification = NotificationCompat.Builder(this, channelId)
            .setOngoing(true)
            .setVisibility(VISIBILITY_SECRET)
            .setShowWhen(false)
            .setLocalOnly(true)
            .setContentIntent(Intent(ACTION_ERASE).let {
                it.setClass(this, this::class.java)
                PendingIntent.getService(this, 0, it, FLAG_ONE_SHOT)
            })
            .apply { buildNotification() }
            .build()

        startForeground(id, notification)

        scope = store.flowScoped { flow ->
            flow.map { state -> state.privateTabs.isEmpty() }
                .ifChanged()
                .collect { noPrivateTabs ->
                    if (noPrivateTabs) stopService()
                }
        }
    }

    final override fun onStartCommand(intent: Intent, flags: Int, startId: Int): Int {
        if (intent.action == ACTION_ERASE) {
            erasePrivateTabs()
        }

        return START_NOT_STICKY
    }

    final override fun onDestroy() {
        scope?.cancel()
    }

    final override fun onBind(intent: Intent?): IBinder? = null

    final override fun onTaskRemoved(rootIntent: Intent) {
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
            importance = IMPORTANCE_LOW
        )
    }
}
