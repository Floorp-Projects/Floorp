/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.auth

import android.os.Build
import androidx.biometric.BiometricManager
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
class BiometricUtilsTest {

    @Config(sdk = [Build.VERSION_CODES.LOLLIPOP])
    @Test
    fun `canUseFeature checks for SDK compatible`() {
        assertFalse(testContext.canUseBiometricFeature())
    }

    @Test
    fun `isHardwareAvailable is true based on AuthenticationStatus`() {
        val manager: BiometricManager = mock {
            whenever(canAuthenticate(BiometricManager.Authenticators.BIOMETRIC_WEAK))
                .thenReturn(BiometricManager.BIOMETRIC_SUCCESS)
                .thenReturn(BiometricManager.BIOMETRIC_ERROR_HW_UNAVAILABLE)
                .thenReturn(BiometricManager.BIOMETRIC_ERROR_NO_HARDWARE)
        }

        assertTrue(BiometricUtils.isHardwareAvailable(manager))
        assertFalse(BiometricUtils.isHardwareAvailable(manager))
        assertFalse(BiometricUtils.isHardwareAvailable(manager))
    }

    @Test
    fun `isEnrolled is true based on AuthenticationStatus`() {
        val manager: BiometricManager = mock {
            whenever(canAuthenticate(BiometricManager.Authenticators.BIOMETRIC_WEAK))
                .thenReturn(BiometricManager.BIOMETRIC_SUCCESS)
        }
        assertTrue(BiometricUtils.isEnrolled(manager))
    }
}
