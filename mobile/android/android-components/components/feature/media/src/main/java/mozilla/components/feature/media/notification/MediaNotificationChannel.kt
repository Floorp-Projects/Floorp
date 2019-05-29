/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.notification

import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import android.os.Build
import mozilla.components.feature.media.R

private const val NOTIFICATION_CHANNEL_ID = "Media"

internal object MediaNotificationChannel {
    /**
     * Make sure a notification channel for media notification exists.
     *
     * Returns the channel id to be used for media notifications.
     */
    fun ensureChannelExists(context: Context): String {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val notificationManager: NotificationManager = context.getSystemService(
                Context.NOTIFICATION_SERVICE
            ) as NotificationManager

            val channel = NotificationChannel(
                NOTIFICATION_CHANNEL_ID,
                context.getString(R.string.mozac_feature_media_notification_channel),
                NotificationManager.IMPORTANCE_DEFAULT
            )

            notificationManager.createNotificationChannel(channel)
        }

        return NOTIFICATION_CHANNEL_ID
    }
}
