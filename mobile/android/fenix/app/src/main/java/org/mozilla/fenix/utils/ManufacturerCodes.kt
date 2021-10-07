/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import android.os.Build

/**
 * A listing of codes returned by [android.os.Build.MANUFACTURER] for different manufacturers.
 * While we try to get the casing accurate, it may be good to use .equals(str, ignoreCase = true)
 * to do the comparison.
 */
object ManufacturerCodes {
    const val HUAWEI: String = "HUAWEI"
    private const val LG = "LGE"
    const val ONE_PLUS: String = "OnePlus"
    private const val SAMSUNG = "samsung"

    val isLG get() = Build.MANUFACTURER.equals(LG, ignoreCase = true)
    val isSamsung get() = Build.MANUFACTURER.equals(SAMSUNG, ignoreCase = true)
}
