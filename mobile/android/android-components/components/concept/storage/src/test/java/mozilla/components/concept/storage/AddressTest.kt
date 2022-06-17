/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.storage

import org.junit.Assert.assertEquals
import org.junit.Test

class AddressTest {

    @Test
    fun `WHEN all name fields are populated THEN full name includes all names`() {
        val address = generateAddress()
        val fullName = address.fullName

        assertEquals(
            "${address.givenName} ${address.additionalName} ${address.familyName}",
            fullName
        )
    }

    @Test
    fun `WHEN additional name is missing THEN full name is given and family name combined`() {
        val address = generateAddress(additionalName = "")
        val fullName = address.fullName

        assertEquals("${address.givenName} ${address.familyName}", fullName)
    }

    @Test
    fun `WHEN only additional and family name are available THEN full name is additional and family name combined`() {
        val address = generateAddress(givenName = "")
        val fullName = address.fullName

        assertEquals("${address.additionalName} ${address.familyName}", fullName)
    }

    @Test
    fun `WHEN only family name is available THEN full name is family name`() {
        val address = generateAddress(givenName = "", additionalName = "")
        val fullName = address.fullName

        assertEquals(address.familyName, fullName)
    }

    private fun generateAddress(
        guid: String = "",
        givenName: String = "Firefox",
        additionalName: String = "The",
        familyName: String = "Browser",
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
        givenName = givenName,
        additionalName = additionalName,
        familyName = familyName,
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
