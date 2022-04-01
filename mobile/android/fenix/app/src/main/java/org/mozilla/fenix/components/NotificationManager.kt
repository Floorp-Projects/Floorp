/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.annotation.TargetApi
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import androidx.core.content.getSystemService
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.TabData
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.R
import org.mozilla.fenix.utils.IntentUtils

/**
 * Manages notification channels and allows displaying different types of notifications.
 */
class NotificationManager(private val context: Context) {
    companion object {
        const val RECEIVE_TABS_TAG = "ReceivedTabs"
        const val RECEIVE_TABS_CHANNEL_ID = "ReceivedTabsChannel"
    }

    init {
        // Create the notification channels we are going to use, but only on API 26+ because the NotificationChannel
        // class is new and not in the support library.
        if (SDK_INT >= Build.VERSION_CODES.O) {
            createNotificationChannel(
                RECEIVE_TABS_CHANNEL_ID,
                // Pick 'high' because this is a user-triggered action that is expected to be part of a continuity flow.
                // That is, user is expected to be waiting for this notification on their device; make it obvious.
                NotificationManager.IMPORTANCE_HIGH,
                // Name and description are shown in the 'app notifications' settings for the app.
                context.getString(R.string.fxa_received_tab_channel_name),
                context.getString(R.string.fxa_received_tab_channel_description)
            )
        }
    }

    private val logger = Logger("NotificationManager")

    fun showReceivedTabs(context: Context, device: Device?, tabs: List<TabData>) {
        // In the future, experiment with displaying multiple tabs from the same device as as Notification Groups.
        // For now, a single notification per tab received will suffice.
        logger.debug("Showing ${tabs.size} tab(s) received from deviceID=${device?.id}")

        // We should not be displaying tabs with certain invalid schemes
        val filteredTabs = tabs.filter { isValidTabSchema(it) }
        logger.debug("${filteredTabs.size} tab(s) after filtering for unsupported schemes")
        filteredTabs.forEach { tab ->
            val showReceivedTabsIntentFlags = IntentUtils.defaultIntentPendingFlags or PendingIntent.FLAG_ONE_SHOT
            val intent = Intent(Intent.ACTION_VIEW, Uri.parse(tab.url))
            intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK
            intent.putExtra(RECEIVE_TABS_TAG, true)
            val pendingIntent: PendingIntent =
                PendingIntent.getActivity(context, 0, intent, showReceivedTabsIntentFlags)

            val builder = NotificationCompat.Builder(context, RECEIVE_TABS_CHANNEL_ID)
                .setSmallIcon(R.drawable.ic_status_logo)
                .setSendTabTitle(context, device, tab)
                .setWhen(System.currentTimeMillis())
                .setContentText(tab.url)
                .setContentIntent(pendingIntent)
                .setAutoCancel(true)
                // Explicitly set a priority for <API25 devices.
                // On newer devices this is inherited from the channel.
                .setPriority(NotificationCompat.PRIORITY_HIGH)
                .setDefaults(Notification.DEFAULT_VIBRATE or Notification.DEFAULT_SOUND)

            if (SDK_INT >= Build.VERSION_CODES.M) {
                builder.setCategory(Notification.CATEGORY_REMINDER)
            }

            val notification = builder.build()

            // Pick a random ID for this notification so that different tabs do not clash.
            @SuppressWarnings("MagicNumber")
            val notificationId = (Math.random() * 100).toInt()

            with(NotificationManagerCompat.from(context)) {
                notify(RECEIVE_TABS_TAG, notificationId, notification)
            }
        }
    }

    @TargetApi(Build.VERSION_CODES.O)
    private fun createNotificationChannel(
        channelId: String,
        importance: Int,
        channelName: String,
        channelDescription: String
    ) {
        val channel = NotificationChannel(channelId, channelName, importance).apply {
            description = channelDescription
        }
        // Register the channel with the system. Once this is done, we can't change importance or other notification
        // channel behaviour. We will be able to change 'name' and 'description' if we so choose.
        val notificationManager: NotificationManager = context.getSystemService()!!
        notificationManager.createNotificationChannel(channel)
    }

    private fun NotificationCompat.Builder.setSendTabTitle(
        context: Context,
        device: Device?,
        tab: TabData
    ): NotificationCompat.Builder {
        device?.let {
            setContentTitle(
                context.getString(
                    R.string.fxa_tab_received_from_notification_name,
                    it.displayName
                )
            )
            return this
        }

        if (tab.title.isEmpty()) {
            setContentTitle(context.getString(R.string.fxa_tab_received_notification_name))
        } else {
            setContentTitle(tab.title)
        }
        return this
    }
}

internal fun isValidTabSchema(tab: TabData): Boolean {
    // We don't sync certain schemas, about|resource|chrome|file|blob|moz-extension
    // See https://searchfox.org/mozilla-central/rev/7d379061bd56251df911728686c378c5820513d8/modules/libpref/init/all.js#4356
    val filteredSchemas = arrayOf("about:", "resource:", "chrome:", "file:", "blob:", "moz-extension:")
    return filteredSchemas.none({ tab.url.startsWith(it) })
}
