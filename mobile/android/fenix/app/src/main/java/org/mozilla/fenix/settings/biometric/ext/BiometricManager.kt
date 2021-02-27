/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.biometric.ext

import androidx.biometric.BiometricManager
import androidx.biometric.BiometricManager.Authenticators.BIOMETRIC_WEAK
import androidx.biometric.BiometricManager.BIOMETRIC_ERROR_HW_UNAVAILABLE
import androidx.biometric.BiometricManager.BIOMETRIC_ERROR_NO_HARDWARE
import androidx.biometric.BiometricManager.BIOMETRIC_SUCCESS

/**
 * Checks if the hardware requirements are met for using the [BiometricManager].
 */
fun BiometricManager.isHardwareAvailable(): Boolean {
    val status = canAuthenticate(BIOMETRIC_WEAK)
    return status != BIOMETRIC_ERROR_NO_HARDWARE && status != BIOMETRIC_ERROR_HW_UNAVAILABLE
}

/**
 * Checks if the user can use the [BiometricManager] and is therefore enrolled.
 */
fun BiometricManager.isEnrolled(): Boolean {
    val status = canAuthenticate(BIOMETRIC_WEAK)
    return status == BIOMETRIC_SUCCESS
}
