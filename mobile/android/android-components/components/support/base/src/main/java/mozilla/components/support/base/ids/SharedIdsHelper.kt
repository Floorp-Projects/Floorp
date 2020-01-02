/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.ids

import android.content.Context
import androidx.annotation.VisibleForTesting

// If the tag is not used again in one week then clear the id
private const val ID_LIFETIME: Long = 1000 * 60 * 60 * 24 * 7

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
