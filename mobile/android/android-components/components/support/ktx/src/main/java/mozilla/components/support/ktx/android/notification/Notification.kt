/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("MatchingDeclarationName")

package mozilla.components.support.ktx.android.notification

import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import android.os.Build
import androidx.annotation.StringRes
import androidx.core.content.getSystemService

/**
 * Make sure a notification channel exists.
 * @param context A [Context], used for creating the notification channel.
 * @param onSetupChannel A lambda in the context of the NotificationChannel that gives you the
 * opportunity to apply any setup on the channel before gets created.
 * @param onCreateChannel A lambda in the context of the NotificationManager that gives you the
 * opportunity to perform any operation on the [NotificationManager].
 * @return Returns the channel id to be used.
 */
fun ensureNotificationChannelExists(
    context: Context,
    channelDate: ChannelData,
    onSetupChannel: NotificationChannel.() -> Unit = {},
    onCreateChannel: NotificationManager.() -> Unit = {},
): String {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
        val notificationManager = requireNotNull(context.getSystemService<NotificationManager>())
        val channel = NotificationChannel(
            channelDate.id,
            context.getString(channelDate.name),
            channelDate.importance,
        )
        onSetupChannel(channel)
        notificationManager.createNotificationChannel(channel)
        onCreateChannel(notificationManager)
    }

    return channelDate.id
}

/**
 * Wraps the data of a NotificationChannel as this class is available after API 26.
 */
class ChannelData(val id: String, @StringRes val name: Int, val importance: Int)
