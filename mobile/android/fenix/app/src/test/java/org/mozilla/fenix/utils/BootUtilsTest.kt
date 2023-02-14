package org.mozilla.fenix.utils

import android.os.Build
import io.mockk.every
import io.mockk.mockk
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.BootUtils.Companion.getBootIdentifier
import org.robolectric.annotation.Config

private const val NO_BOOT_IDENTIFIER = "no boot identifier available"

@RunWith(FenixRobolectricTestRunner::class)
class BootUtilsTest {

    private lateinit var bootUtils: BootUtils

    @Before
    fun setUp() {
        bootUtils = mockk(relaxed = true)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M])
    fun `WHEN no boot id file & Android version is less than N(24) THEN getBootIdentifier returns NO_BOOT_IDENTIFIER`() {
        every { bootUtils.bootIdFileExists }.returns(false)

        assertEquals(NO_BOOT_IDENTIFIER, getBootIdentifier(testContext, bootUtils))
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M])
    fun `WHEN boot id file returns null & Android version is less than N(24) THEN getBootIdentifier returns NO_BOOT_IDENTIFIER`() {
        every { bootUtils.bootIdFileExists }.returns(true)
        every { bootUtils.deviceBootId }.returns(null)

        assertEquals(NO_BOOT_IDENTIFIER, getBootIdentifier(testContext, bootUtils))
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M])
    fun `WHEN boot id file has text & Android version is less than N(24) THEN getBootIdentifier returns the boot id`() {
        every { bootUtils.bootIdFileExists }.returns(true)
        val bootId = "test"
        every { bootUtils.deviceBootId }.returns(bootId)

        assertEquals(bootId, getBootIdentifier(testContext, bootUtils))
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M])
    fun `WHEN boot id file has text with whitespace & Android version is less than N(24) THEN getBootIdentifier returns the trimmed boot id`() {
        every { bootUtils.bootIdFileExists }.returns(true)
        val bootId = "  test  "
        every { bootUtils.deviceBootId }.returns(bootId)

        assertEquals(bootId, getBootIdentifier(testContext, bootUtils))
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.N])
    fun `WHEN Android version is N(24) THEN getBootIdentifier returns the boot count`() {
        val bootCount = "9"
        every { bootUtils.getDeviceBootCount(any()) }.returns(bootCount)
        assertEquals(bootCount, getBootIdentifier(testContext, bootUtils))
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.O])
    fun `WHEN Android version is more than N(24) THEN getBootIdentifier returns the boot count`() {
        val bootCount = "9"
        every { bootUtils.getDeviceBootCount(any()) }.returns(bootCount)
        assertEquals(bootCount, getBootIdentifier(testContext, bootUtils))
    }
}
