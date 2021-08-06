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
private const val ID_LIFETIME: Long = 1000L * 60L * 60L * 24L * 7L

// We start at 10000 instead of 0 to avoid conflicts with app code using random low numbers.
private const val ID_OFFSET = 10000

private const val ID_PREFERENCE_FILE = "mozac_support_base_shared_ids_helper"

/**
 * Helper for component and app code to use unique notification ids without conflicts.
 */
object SharedIdsHelper {
    private val ids = SharedIds(ID_PREFERENCE_FILE, ID_LIFETIME, ID_OFFSET)

    /**
     * Get a unique notification ID for the provided unique tag.
     */
    fun getIdForTag(context: Context, tag: String): Int = ids.getIdForTag(context, tag)

    /**
     * Get the next available unique notification ID for the provided unique tag.
     */
    fun getNextIdForTag(context: Context, tag: String): Int = ids.getNextIdForTag(context, tag)

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
 * This will replace the previous notification with the same tag. See also: [SharedIdsHelper] for more details.
 */
fun NotificationManager.notify(context: Context, tag: String, notification: Notification) {
    notify(tag, SharedIdsHelper.getIdForTag(context, tag), notification)
}

/**
 * Post a notification to be shown in the status bar.
 *
 * Uses a unique [String] tag instead of an [Int] id like [NotificationManagerCompat.notify].
 * This will replace the previous notification with the same tag. See also: [SharedIdsHelper] for more details.
 */
fun NotificationManagerCompat.notify(context: Context, tag: String, notification: Notification) {
    notify(tag, SharedIdsHelper.getIdForTag(context, tag), notification)
}

/**
 * Cancel a previously shown notification.
 *
 * Uses a unique [String] tag instead of an [Int] id like [NotificationManager.cancel].
 */
fun NotificationManager.cancel(context: Context, tag: String) {
    cancel(tag, SharedIdsHelper.getIdForTag(context, tag))
}

/**
 * Cancel a previously shown notification.
 *
 * Uses a unique [String] tag instead of an [Int] id like [NotificationManagerCompat.cancel].
 */
fun NotificationManagerCompat.cancel(context: Context, tag: String) {
    cancel(tag, SharedIdsHelper.getIdForTag(context, tag))
}
