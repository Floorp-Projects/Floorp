/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import android.os.Build
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.any
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.utils.BootUtils.Companion.getBootIdentifier
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.`when`
import org.mockito.MockitoAnnotations
import org.robolectric.annotation.Config

private const val NO_BOOT_IDENTIFIER = "no boot identifier available"

@RunWith(AndroidJUnit4::class)
class BootUtilsTest {

    @Mock private lateinit var bootUtils: BootUtils

    @Before
    fun setUp() {
        MockitoAnnotations.openMocks(this)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M])
    fun `WHEN no boot id file & Android version is less than N(24) THEN getBootIdentifier returns NO_BOOT_IDENTIFIER`() {
        `when`(bootUtils.bootIdFileExists).thenReturn(false)

        assertEquals(NO_BOOT_IDENTIFIER, getBootIdentifier(testContext, bootUtils))
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M])
    fun `WHEN boot id file returns null & Android version is less than N(24) THEN getBootIdentifier returns NO_BOOT_IDENTIFIER`() {
        `when`(bootUtils.bootIdFileExists).thenReturn(true)
        `when`(bootUtils.deviceBootId).thenReturn(null)

        assertEquals(NO_BOOT_IDENTIFIER, getBootIdentifier(testContext, bootUtils))
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M])
    fun `WHEN boot id file has text & Android version is less than N(24) THEN getBootIdentifier returns the boot id`() {
        `when`(bootUtils.bootIdFileExists).thenReturn(true)
        val bootId = "test"
        `when`(bootUtils.deviceBootId).thenReturn(bootId)

        assertEquals(bootId, getBootIdentifier(testContext, bootUtils))
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M])
    fun `WHEN boot id file has text with whitespace & Android version is less than N(24) THEN getBootIdentifier returns the trimmed boot id`() {
        `when`(bootUtils.bootIdFileExists).thenReturn(true)
        val bootId = "  test  "
        `when`(bootUtils.deviceBootId).thenReturn(bootId)

        assertEquals(bootId, getBootIdentifier(testContext, bootUtils))
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.N])
    fun `WHEN Android version is N(24) THEN getBootIdentifier returns the boot count`() {
        val bootCount = "9"
        `when`(bootUtils.getDeviceBootCount(any())).thenReturn(bootCount)
        assertEquals(bootCount, getBootIdentifier(testContext, bootUtils))
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.O])
    fun `WHEN Android version is more than N(24) THEN getBootIdentifier returns the boot count`() {
        val bootCount = "9"
        `when`(bootUtils.getDeviceBootCount(any())).thenReturn(bootCount)
        assertEquals(bootCount, getBootIdentifier(testContext, bootUtils))
    }
}
