/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

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
