package org.mozilla.focus.biometrics

import android.content.Context
import android.support.v4.hardware.fingerprint.FingerprintManagerCompat

object Biometrics {
    fun hasFingerprintHardware(context: Context): Boolean {
        val fingerprintManager = FingerprintManagerCompat.from(context)
        return fingerprintManager.isHardwareDetected && fingerprintManager.hasEnrolledFingerprints()
    }
}
