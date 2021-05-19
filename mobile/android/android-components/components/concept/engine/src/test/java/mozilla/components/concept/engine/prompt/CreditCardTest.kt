/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.prompt

import org.junit.Assert.assertEquals
import org.junit.Test

class CreditCardTest {

    @Test
    fun `Create a CreditCard`() {
        val guid = "1"
        val name = "Banana Apple"
        val number = "4111111111111110"
        val last4Digits = "1110"
        val expiryMonth = "5"
        val expiryYear = "2030"
        val cardType = "amex"
        val creditCard = CreditCard(
            guid = guid,
            name = name,
            number = number,
            expiryMonth = expiryMonth,
            expiryYear = expiryYear,
            cardType = cardType
        )

        assertEquals(guid, creditCard.guid)
        assertEquals(name, creditCard.name)
        assertEquals(number, creditCard.number)
        assertEquals(expiryMonth, creditCard.expiryMonth)
        assertEquals(expiryYear, creditCard.expiryYear)
        assertEquals(cardType, creditCard.cardType)
        assertEquals(
            CreditCard.ellipsesStart +
                CreditCard.ellipsis + CreditCard.ellipsis + CreditCard.ellipsis + CreditCard.ellipsis +
                last4Digits +
                CreditCard.ellipsesEnd,
            creditCard.obfuscatedCardNumber
        )
    }
}
