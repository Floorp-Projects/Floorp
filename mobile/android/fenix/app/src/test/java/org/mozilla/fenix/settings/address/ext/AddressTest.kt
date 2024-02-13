/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.address.ext

import mozilla.components.concept.storage.Address
import org.junit.Assert.assertEquals
import org.junit.Test

class AddressTest {

    @Test
    fun `WHEN all properties are present THEN all properties present in description`() {
        val addr = generateAddress()

        val description = addr.getAddressLabel()

        val expected = "${addr.streetAddress}, ${addr.addressLevel3}, ${addr.addressLevel2}, " +
            "${addr.organization}, ${addr.addressLevel1}, ${addr.country}, " +
            "${addr.postalCode}, ${addr.tel}, ${addr.email}"

        assertEquals(expected, description)
    }

    @Test
    fun `WHEN any properties are missing THEN description includes only present`() {
        val addr = generateAddress(
            addressLevel3 = "",
            organization = "",
            email = "",
        )

        val description = addr.getAddressLabel()

        val expected = "${addr.streetAddress}, ${addr.addressLevel2}, ${addr.addressLevel1}, " +
            "${addr.country}, ${addr.postalCode}, ${addr.tel}"
        assertEquals(expected, description)
    }

    @Test
    fun `WHEN everything is missing THEN description is empty`() {
        val addr = generateAddress(
            name = "",
            organization = "",
            streetAddress = "",
            addressLevel3 = "",
            addressLevel2 = "",
            addressLevel1 = "",
            postalCode = "",
            country = "",
            tel = "",
            email = "",
        )

        val description = addr.getAddressLabel()

        assertEquals("", description)
    }

    @Test
    fun `GIVEN multiline street address THEN joined as single line`() {
        val streetAddress = """
            line1
            line2
            line3
        """.trimIndent()

        val result = streetAddress.toOneLineAddress()

        assertEquals("line1 line2 line3", result)
    }

    private fun generateAddress(
        name: String = "Firefox The Browser",
        organization: String = "Mozilla",
        streetAddress: String = "street",
        addressLevel3: String = "3",
        addressLevel2: String = "2",
        addressLevel1: String = "1",
        postalCode: String = "code",
        country: String = "country",
        tel: String = "tel",
        email: String = "email",
    ) = Address(
        guid = "",
        name = name,
        organization = organization,
        streetAddress = streetAddress,
        addressLevel3 = addressLevel3,
        addressLevel2 = addressLevel2,
        addressLevel1 = addressLevel1,
        postalCode = postalCode,
        country = country,
        tel = tel,
        email = email,
        timeCreated = 1,
        timeLastUsed = 1,
        timeLastModified = 1,
        timesUsed = 1,
    )
}
