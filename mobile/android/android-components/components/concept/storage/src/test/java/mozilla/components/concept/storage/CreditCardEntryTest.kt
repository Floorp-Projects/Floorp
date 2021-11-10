/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.storage

import mozilla.components.concept.storage.CreditCard.Companion.ellipsesEnd
import mozilla.components.concept.storage.CreditCard.Companion.ellipsesStart
import mozilla.components.concept.storage.CreditCard.Companion.ellipsis
import mozilla.components.support.ktx.kotlin.last4Digits
import org.junit.Assert.assertEquals
import org.junit.Test

class CreditCardEntryTest {

    private val creditCard = CreditCardEntry(
        guid = "1",
        name = "Banana Apple",
        number = "4111111111111110",
        expiryMonth = "5",
        expiryYear = "2030",
        cardType = "amex"
    )

    @Test
    fun `WHEN obfuscatedCardNumber getter is called THEN the expected obfuscated card number is returned`() {
        assertEquals(
            ellipsesStart +
                ellipsis + ellipsis + ellipsis + ellipsis +
                creditCard.number.last4Digits() +
                ellipsesEnd,
            creditCard.obfuscatedCardNumber
        )
    }

    @Test
    fun `WHEN expiryDdate getter is called THEN the expected expiry date string is returned`() {
        assertEquals("0${creditCard.expiryMonth}/${creditCard.expiryYear}", creditCard.expiryDate)
    }
}
