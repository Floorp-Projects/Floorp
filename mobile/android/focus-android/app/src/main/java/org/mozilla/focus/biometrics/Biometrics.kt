/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("DEPRECATION")

package org.mozilla.focus.biometrics

import android.app.KeyguardManager
import android.content.Context
import android.os.Build
import androidx.core.hardware.fingerprint.FingerprintManagerCompat
import org.mozilla.focus.utils.Settings

object Biometrics {
    fun hasFingerprintHardware(context: Context): Boolean {
        val fingerprintManager = FingerprintManagerCompat.from(context)

        // #3566: There are some devices with fingerprint readers that will retain enrolled fingerprints
        // after disabling the lockscreen. AndroidKeyStore requires having the lock screen enabled,
        // so we need to make sure the lock screen is enabled before we can enable the fingerprint sensor
        val keyguardManager = context.getSystemService(Context.KEYGUARD_SERVICE) as KeyguardManager

        return keyguardManager.isKeyguardSecure &&
            fingerprintManager.isHardwareDetected &&
            fingerprintManager.hasEnrolledFingerprints()
    }

    fun isBiometricsEnabled(context: Context): Boolean {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.M &&
            Settings.getInstance(context).shouldUseBiometrics() &&
            hasFingerprintHardware(context)
    }
}
