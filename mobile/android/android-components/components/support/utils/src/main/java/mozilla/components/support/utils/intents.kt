/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:JvmName("IntentUtils")

package mozilla.components.support.utils

import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.os.Build

/**
 * Create a [PendingIntent] instance to run a certain service described with the [Intent].
 *
 * This method will allow you to launch a service that will be able to overpass
 * [background service limitations](https://developer.android.com/about/versions/oreo/background#services)
 * introduced in Android Oreo.
 *
 * @param context an [Intent] to start a service.
 */
@JvmName("createForegroundServicePendingIntent")
fun Intent.asForegroundServicePendingIntent(context: Context): PendingIntent =
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
        PendingIntent.getForegroundService(context, 0, this, 0)
    } else {
        PendingIntent.getService(context, 0, this, 0)
    }
