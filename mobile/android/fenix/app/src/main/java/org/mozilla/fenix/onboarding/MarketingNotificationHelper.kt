/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import android.os.Build
import androidx.core.app.NotificationManagerCompat
import org.mozilla.fenix.GleanMetrics.Events.marketingNotificationAllowed
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.areNotificationsEnabledSafe

// Channel ID was not updated when it was renamed to marketing.  Thus, we'll have to continue
// to use this ID as the marketing channel ID
private const val MARKETING_CHANNEL_ID = "org.mozilla.fenix.default.browser.channel"

// For notification that uses the marketing notification channel, IDs should be unique.
const val DEFAULT_BROWSER_NOTIFICATION_ID = 1
const val RE_ENGAGEMENT_NOTIFICATION_ID = 2

/**
 * Make sure the marketing notification channel exists.
 *
 * Returns the channel id to be used for notifications.
 */
fun ensureMarketingChannelExists(context: Context): String {
    var channelEnabled = true
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
        val notificationManager: NotificationManager =
            context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager

        var channel =
            notificationManager.getNotificationChannel(MARKETING_CHANNEL_ID)

        if (channel == null) {
            channel = NotificationChannel(
                MARKETING_CHANNEL_ID,
                context.getString(R.string.notification_marketing_channel_name),
                NotificationManager.IMPORTANCE_DEFAULT,
            )

            notificationManager.createNotificationChannel(channel)
        }

        channelEnabled = channel.importance != NotificationManager.IMPORTANCE_NONE
    }

    val notificationsEnabled = NotificationManagerCompat.from(context).areNotificationsEnabledSafe()

    marketingNotificationAllowed.set(notificationsEnabled && channelEnabled)

    return MARKETING_CHANNEL_ID
}
