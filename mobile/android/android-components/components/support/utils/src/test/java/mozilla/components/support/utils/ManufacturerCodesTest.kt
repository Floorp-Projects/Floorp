/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import mozilla.components.support.utils.ManufacturerCodes.manufacturer
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ManufacturerCodesTest {

    @Test
    fun testIsHuawei() {
        manufacturer = "Huawei" // expected value for Huawei devices
        assertTrue(ManufacturerCodes.isHuawei)

        assertFalse(ManufacturerCodes.isSamsung)

        assertFalse(ManufacturerCodes.isXiaomi)

        assertFalse(ManufacturerCodes.isOnePlus)
    }

    @Test
    fun testIsSamsung() {
        manufacturer = "Samsung" // expected value for Samsung devices

        assertFalse(ManufacturerCodes.isHuawei)

        assertTrue(ManufacturerCodes.isSamsung)

        assertFalse(ManufacturerCodes.isXiaomi)

        assertFalse(ManufacturerCodes.isOnePlus)
    }

    @Test
    fun testIsXiaomi() {
        manufacturer = "Xiaomi" // expected value for Xiaomi devices

        assertFalse(ManufacturerCodes.isHuawei)

        assertFalse(ManufacturerCodes.isSamsung)

        assertFalse(ManufacturerCodes.isOnePlus)

        assertTrue(ManufacturerCodes.isXiaomi)
    }

    @Test
    fun testIsOnePlus() {
        manufacturer = "OnePlus" // expected value for OnePlus devices

        assertFalse(ManufacturerCodes.isHuawei)

        assertFalse(ManufacturerCodes.isSamsung)

        assertFalse(ManufacturerCodes.isXiaomi)

        assertTrue(ManufacturerCodes.isOnePlus)
    }
}
