/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import android.os.Build
import androidx.annotation.VisibleForTesting

/**
 * Used to check if a device is from a specific manufacturer,
 * using the value returned by [android.os.Build.MANUFACTURER].
 */
object ManufacturerCodes {
    // Manufacturer codes taken from https://developers.google.com/zero-touch/resources/manufacturer-names
    private const val HUAWEI: String = "Huawei"
    private const val SAMSUNG = "Samsung"
    private const val XIAOMI = "Xiaomi"
    private const val ONE_PLUS = "OnePlus"
    private const val LG = "LGE"

    @VisibleForTesting
    internal var manufacturer = Build.MANUFACTURER // is a var for testing purposes

    val isHuawei get() = manufacturer.equals(HUAWEI, ignoreCase = true)
    val isSamsung get() = manufacturer.equals(SAMSUNG, ignoreCase = true)
    val isXiaomi get() = manufacturer.equals(XIAOMI, ignoreCase = true)
    val isOnePlus get() = manufacturer.equals(ONE_PLUS, ignoreCase = true)
    val isLG get() = manufacturer.equals(LG, ignoreCase = true)
}
