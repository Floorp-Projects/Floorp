/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.annotation.TargetApi
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import mozilla.components.concept.sync.DeviceEvent
import org.mozilla.fenix.R

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
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
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

    fun showReceivedTabs(event: DeviceEvent.TabReceived) {
        // In the future, experiment with displaying multiple tabs from the same device as as Notification Groups.
        // For now, a single notification per tab received will suffice.
        event.entries.forEach { tab ->
            val intent = Intent(Intent.ACTION_VIEW, Uri.parse(tab.url))
            val pendingIntent: PendingIntent = PendingIntent.getActivity(context, 0, intent, 0)

            val builder = NotificationCompat.Builder(context, RECEIVE_TABS_CHANNEL_ID)
                .setSmallIcon(R.drawable.ic_status_logo)
                .setContentTitle(tab.title)
                .setContentText(tab.url)
                .setContentIntent(pendingIntent)
                // Explicitly set a priority for <API25 devices.
                // On newer devices this is inherited from the channel.
                .setPriority(NotificationCompat.PRIORITY_HIGH)

            // Pick a random ID for this notification so that different tabs do not clash.
            @SuppressWarnings("MagicNumber")
            val notificationId = (Math.random() * 100).toInt()

            with(NotificationManagerCompat.from(context)) {
                notify(RECEIVE_TABS_TAG, notificationId, builder.build())
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
        val notificationManager: NotificationManager =
            context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        notificationManager.createNotificationChannel(channel)
    }
}
