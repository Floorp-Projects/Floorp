/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.biometric.ext

import androidx.biometric.BiometricManager
import androidx.biometric.BiometricManager.BIOMETRIC_ERROR_HW_UNAVAILABLE
import androidx.biometric.BiometricManager.BIOMETRIC_ERROR_NO_HARDWARE
import androidx.biometric.BiometricManager.BIOMETRIC_SUCCESS
import io.mockk.every
import io.mockk.mockk
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

class BiometricManagerKtTest {

    lateinit var manager: BiometricManager

    @Before
    fun setup() {
        manager = mockk()
    }

    @Test
    fun `isHardwareAvailable checks status`() {
        every { manager.canAuthenticate(any()) }.answers { BIOMETRIC_ERROR_NO_HARDWARE }

        assertFalse(manager.isHardwareAvailable())

        every { manager.canAuthenticate(any()) }.answers { BIOMETRIC_ERROR_HW_UNAVAILABLE }

        assertFalse(manager.isHardwareAvailable())

        every { manager.canAuthenticate(any()) }.answers { BIOMETRIC_SUCCESS }

        assertTrue(manager.isHardwareAvailable())
    }

    @Test
    fun `isEnrolled checks status`() {
        every { manager.canAuthenticate(any()) }.answers { BIOMETRIC_ERROR_NO_HARDWARE }

        assertFalse(manager.isEnrolled())

        every { manager.canAuthenticate(any()) }.answers { BIOMETRIC_SUCCESS }

        assertTrue(manager.isEnrolled())
    }
}
