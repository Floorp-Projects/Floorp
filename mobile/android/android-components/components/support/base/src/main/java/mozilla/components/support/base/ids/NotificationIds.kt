/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.ids

import android.app.Notification
import android.app.NotificationManager
import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.core.app.NotificationManagerCompat

// If the tag is not used again in one week then clear the id
private const val NOTIFICATION_ID_LIFETIME: Long = 1000 * 60 * 60 * 24 * 7

// We start at 10000 instead of 0 to avoid conflicts with app code using random low numbers.
private const val ID_OFFSET = 10000

private const val PREFERENCE_FILE = "notification_ids"

/**
 * Helper for component and app code to use unique notification ids without conflicts.
 */
object NotificationIds {
    private val ids = SharedIds(PREFERENCE_FILE, NOTIFICATION_ID_LIFETIME, ID_OFFSET)

    /**
     * Get a unique notification ID for the provided unique tag.
     */
    fun getIdForTag(context: Context, tag: String): Int = ids.getIdForTag(context, tag)

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var now: () -> Long
        get() = ids.now
        set(value) { ids.now = value }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun clear(context: Context) = ids.clear(context)
}

/**
 * Post a notification to be shown in the status bar.
 *
 * Uses a unique [String] tag instead of an [Int] id like [NotificationManager.notify].
 */
fun NotificationManager.notify(context: Context, tag: String, notification: Notification) {
    notify(NotificationIds.getIdForTag(context, tag), notification)
}

/**
 * Post a notification to be shown in the status bar.
 *
 * Uses a unique [String] tag instead of an [Int] id like [NotificationManagerCompat.notify].
 */
fun NotificationManagerCompat.notify(context: Context, tag: String, notification: Notification) {
    notify(NotificationIds.getIdForTag(context, tag), notification)
}

/**
 * Cancel a previously shown notification.
 *
 * Uses a unique [String] tag instead of an [Int] id like [NotificationManager.cancel].
 */
fun NotificationManager.cancel(context: Context, tag: String) {
    cancel(NotificationIds.getIdForTag(context, tag))
}

/**
 * Cancel a previously shown notification.
 *
 * Uses a unique [String] tag instead of an [Int] id like [NotificationManagerCompat.cancel].
 */
fun NotificationManagerCompat.cancel(context: Context, tag: String) {
    cancel(NotificationIds.getIdForTag(context, tag))
}
