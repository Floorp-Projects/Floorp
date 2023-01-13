/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.os.Build
import androidx.core.app.NotificationChannelCompat
import androidx.core.app.NotificationManagerCompat

/**
 * Returns whether notifications are enabled, catches any exception that was thrown from
 * [NotificationManagerCompat.areNotificationsEnabled] and returns false.
 */
@Suppress("TooGenericExceptionCaught")
fun NotificationManagerCompat.areNotificationsEnabledSafe(): Boolean {
    return try {
        areNotificationsEnabled()
    } catch (e: Exception) {
        false
    }
}

/**
 * If the channel does not exist or is null, this returns false.
 * If the channel exists with importance more than [NotificationManagerCompat.IMPORTANCE_NONE] and
 * notifications are enabled for the app, this returns true.
 * On <= SDK 26, this checks if notifications are enabled for the app.
 *
 * @param channelId the id of the notification channel to check.
 * @return true if the channel is enabled, false otherwise.
 */
fun NotificationManagerCompat.isNotificationChannelEnabled(channelId: String): Boolean {
    return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
        val channel = getNotificationChannelSafe(channelId)
        if (channel == null) {
            false
        } else {
            areNotificationsEnabledSafe() && channel.importance != NotificationManagerCompat.IMPORTANCE_NONE
        }
    } else {
        areNotificationsEnabledSafe()
    }
}

/**
 * Returns the notification channel with the given [channelId], or null if the channel does not
 * exist, catches any exception that was thrown by
 * [NotificationManagerCompat.getNotificationChannelCompat] and returns null.
 *
 * @param channelId the id of the notification channel to check.
 */
@Suppress("TooGenericExceptionCaught")
private fun NotificationManagerCompat.getNotificationChannelSafe(channelId: String): NotificationChannelCompat? {
    return try {
        getNotificationChannelCompat(channelId)
    } catch (e: Exception) {
        null
    }
}
