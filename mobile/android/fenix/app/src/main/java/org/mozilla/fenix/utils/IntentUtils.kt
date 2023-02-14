/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import android.app.PendingIntent
import android.os.Build

object IntentUtils {

    /**
     * Since Android 12 we need to set PendingIntent mutability explicitly, but Android 6 can be the minimum version
     * This additional requirement improves your app's security.
     * FLAG_IMMUTABLE -> Flag indicating that the created PendingIntent should be immutable.
     */
    val defaultIntentPendingFlags
        get() = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            PendingIntent.FLAG_IMMUTABLE
        } else {
            0 // No flags. Default behavior.
        }
}
