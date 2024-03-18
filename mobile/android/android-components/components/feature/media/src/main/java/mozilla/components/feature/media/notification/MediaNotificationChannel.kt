/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.notification

import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import android.os.Build
import androidx.core.app.NotificationCompat
import mozilla.components.feature.media.R

private const val NOTIFICATION_CHANNEL_ID = "mozac.feature.media.generic"
private const val LEGACY_NOTIFICATION_CHANNEL_ID = "Media"

internal object MediaNotificationChannel {
    /**
     * Make sure a notification channel for media notification exists.
     *
     * Returns the channel id to be used for media notifications.
     */
    fun ensureChannelExists(context: Context): String {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val notificationManager: NotificationManager = context.getSystemService(
                Context.NOTIFICATION_SERVICE,
            ) as NotificationManager

            val channel = NotificationChannel(
                NOTIFICATION_CHANNEL_ID,
                context.getString(R.string.mozac_feature_media_notification_channel),
                NotificationManager.IMPORTANCE_LOW,
            )
            channel.setShowBadge(false)
            channel.lockscreenVisibility = NotificationCompat.VISIBILITY_PUBLIC

            notificationManager.createNotificationChannel(channel)

            // We can't just change a channel. So we had to re-create the channel with a new name.
            notificationManager.deleteNotificationChannel(LEGACY_NOTIFICATION_CHANNEL_ID)
        }

        return NOTIFICATION_CHANNEL_ID
    }
}
