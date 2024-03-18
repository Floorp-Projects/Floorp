/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.os

import android.Manifest.permission.VIBRATE
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import android.os.VibrationEffect
import android.os.VibrationEffect.DEFAULT_AMPLITUDE
import android.os.Vibrator
import androidx.annotation.RequiresPermission

/**
 * Vibrate constantly for the specified period of time.
 *
 * @param milliseconds The number of milliseconds to vibrate.
 */
@RequiresPermission(VIBRATE)
fun Vibrator.vibrateOneShot(milliseconds: Long) {
    if (SDK_INT >= Build.VERSION_CODES.O) {
        vibrate(VibrationEffect.createOneShot(milliseconds, DEFAULT_AMPLITUDE))
    } else {
        @Suppress("Deprecation")
        vibrate(milliseconds)
    }
}
