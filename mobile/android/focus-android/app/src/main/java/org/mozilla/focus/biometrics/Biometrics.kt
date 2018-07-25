/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.biometrics

import android.content.Context
import android.support.v4.hardware.fingerprint.FingerprintManagerCompat

object Biometrics {
    fun hasFingerprintHardware(context: Context): Boolean {
        val fingerprintManager = FingerprintManagerCompat.from(context)
        return fingerprintManager.isHardwareDetected && fingerprintManager.hasEnrolledFingerprints()
    }
}
