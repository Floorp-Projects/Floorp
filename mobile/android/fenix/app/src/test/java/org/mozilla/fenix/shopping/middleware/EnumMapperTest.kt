/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class EnumMapperTest {

    private enum class DeviceType {
        PHONE,
        TABLET,
        WEARABLE,
        OTHER_TYPE,
    }

    @Test
    fun `GIVEN an enum WHEN a string is an enum object THEN it is mapped to the enum`() {
        val phone = "phone"
        val tablet = "TABLET"
        val wearable = "WEARABLE"
        val other = "other_type"
        val otherWithSpace = "other type"

        assertEquals(DeviceType.PHONE, phone.asEnumOrDefault<DeviceType>())
        assertEquals(DeviceType.TABLET, tablet.asEnumOrDefault<DeviceType>())
        assertEquals(DeviceType.WEARABLE, wearable.asEnumOrDefault<DeviceType>())
        assertEquals(DeviceType.OTHER_TYPE, other.asEnumOrDefault<DeviceType>())
        assertEquals(DeviceType.OTHER_TYPE, otherWithSpace.asEnumOrDefault<DeviceType>())
    }

    @Test
    fun `GIVEN an enum WHEN a string is not an enum object and not default is passed THEN null is returned`() {
        val input = "car"

        assertNull(input.asEnumOrDefault<DeviceType>())
    }

    @Test
    fun `GIVEN an enum WHEN a string is not an enum object and default is passed THEN default is returned`() {
        val input = "car"

        assertEquals(DeviceType.OTHER_TYPE, input.asEnumOrDefault(DeviceType.OTHER_TYPE))
    }
}
