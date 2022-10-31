/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.auth

import android.content.Context
import android.os.Build
import androidx.biometric.BiometricManager

/**
 * Utility class for BiometricPromptAuth
 */

fun Context.canUseBiometricFeature(): Boolean {
    return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
        val manager = BiometricManager.from(this)
        return BiometricUtils.canUseFeature(manager)
    } else {
        false
    }
}

internal object BiometricUtils {

    /**
     * Checks if the appropriate SDK version and hardware capabilities are met to use the feature.
     */
    internal fun canUseFeature(manager: BiometricManager): Boolean {
        return isHardwareAvailable(manager) && isEnrolled(manager)
    }

    /**
     * Checks if the hardware requirements are met for using the [BiometricManager].
     */
    internal fun isHardwareAvailable(biometricManager: BiometricManager): Boolean {
        val status =
            biometricManager.canAuthenticate(BiometricManager.Authenticators.BIOMETRIC_WEAK)
        return status != BiometricManager.BIOMETRIC_ERROR_NO_HARDWARE &&
            status != BiometricManager.BIOMETRIC_ERROR_HW_UNAVAILABLE
    }

    /**
     * Checks if the user can use the [BiometricManager] and is therefore enrolled.
     */
    internal fun isEnrolled(biometricManager: BiometricManager): Boolean {
        val status =
            biometricManager.canAuthenticate(BiometricManager.Authenticators.BIOMETRIC_WEAK)
        return status == BiometricManager.BIOMETRIC_SUCCESS
    }
}
