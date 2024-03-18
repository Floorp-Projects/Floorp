/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import android.app.PendingIntent
import android.os.Build

/**
 * Helper methods for when working with [PendingIntent]s.
 */
object PendingIntentUtils {
    /**
     * Android 6 introduced FLAG_IMMUTABLE to prevents apps that receive a PendingIntent from
     * filling in unpopulated properties. Android 12 requires mutability explicitly.
     *
     * This property will return:
     * - PendingIntent.FLAG_IMMUTABLE - for Android API 23+
     * - 0 (framework default flags) - for Android APIs lower than 23.
     */
    val defaultFlags
        get() = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            PendingIntent.FLAG_IMMUTABLE
        } else {
            0 // No flags. Default behavior.
        }
}
