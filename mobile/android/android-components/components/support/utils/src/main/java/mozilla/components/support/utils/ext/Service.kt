/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils.ext

import android.app.Service
import android.app.Service.STOP_FOREGROUND_DETACH
import android.app.Service.STOP_FOREGROUND_REMOVE
import android.os.Build

/**
 * Remove this service from foreground state.
 * @param removeNotification whether the notification is to be removed or not.
 */
fun Service.stopForegroundCompat(removeNotification: Boolean) {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
        when (removeNotification) {
            true -> stopForeground(STOP_FOREGROUND_REMOVE)
            false -> stopForeground(STOP_FOREGROUND_DETACH)
        }
    } else {
        @Suppress("DEPRECATION")
        stopForeground(removeNotification)
    }
}
