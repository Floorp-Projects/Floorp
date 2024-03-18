/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.storage

import mozilla.components.concept.storage.Address.Companion.toOneLineAddress
import org.junit.Assert.assertEquals
import org.junit.Test

class AddressTest {

    @Test
    fun `WHEN all address properties are present THEN full address present in label`() {
        val address = generateAddress()
        val expected =
            "${address.streetAddress}, ${address.addressLevel3}, ${address.addressLevel2}, " +
                "${address.organization}, ${address.addressLevel1}, ${address.country}, " +
                "${address.postalCode}, ${address.tel}, ${address.email}"

        assertEquals(expected, address.addressLabel)
    }

    @Test
    fun `WHEN any address properties are missing THEN label only includes only properties that are available`() {
        val address = generateAddress(
            addressLevel3 = "",
            organization = "",
            email = "",
        )
        val expected =
            "${address.streetAddress}, ${address.addressLevel2}, ${address.addressLevel1}, " +
                "${address.country}, ${address.postalCode}, ${address.tel}"

        assertEquals(expected, address.addressLabel)
    }

    @Test
    fun `WHEN no address properties are present THEN label is the empty string`() {
        val address = generateAddress(
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

        assertEquals("", address.addressLabel)
    }

    @Test
    fun `GIVEN multiline street address WHEN one line address is called THEN an one line address is returned`() {
        val streetAddress = """
            line1
            line2
            line3
        """.trimIndent()

        assertEquals("line1 line2 line3", streetAddress.toOneLineAddress())
    }

    private fun generateAddress(
        guid: String = "",
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
        guid = guid,
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
    )
}
