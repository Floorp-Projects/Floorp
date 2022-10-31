/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class CreditCardUtilsTest {

    @Test
    fun `GIVEN a list of recognized card numbers and their respective card type WHEN creditCardIIN is called for a given a card number THEN the correct cardType name is returned`() {
        /**
         * Test cases based on
         * https://searchfox.org/mozilla-central/source/toolkit/modules/tests/xpcshell/test_CreditCard.js
         */
        val recognizedCards = listOf(
            // Edge cases
            Pair("2221000000000000", "mastercard"),
            Pair("2720000000000000", "mastercard"),
            Pair("2200000000000000", "mir"),
            Pair("2204000000000000", "mir"),
            Pair("340000000000000", "amex"),
            Pair("370000000000000", "amex"),
            Pair("3000000000000000", "diners"),
            Pair("3050000000000000", "diners"),
            Pair("3095000000000000", "diners"),
            Pair("36000000000000", "diners"),
            Pair("3800000000000000", "diners"),
            Pair("3900000000000000", "diners"),
            Pair("3528000000000000", "jcb"),
            Pair("3589000000000000", "jcb"),
            Pair("4035000000000000", "cartebancaire"),
            Pair("4360000000000000", "cartebancaire"),
            Pair("4000000000000000", "visa"),
            Pair("4999999999999999", "visa"),
            Pair("5400000000000000", "mastercard"),
            Pair("5500000000000000", "mastercard"),
            Pair("5100000000000000", "mastercard"),
            Pair("5399999999999999", "mastercard"),
            Pair("6011000000000000", "discover"),
            Pair("6221260000000000", "discover"),
            Pair("6229250000000000", "discover"),
            Pair("6240000000000000", "discover"),
            Pair("6269990000000000", "discover"),
            Pair("6282000000000000", "discover"),
            Pair("6288990000000000", "discover"),
            Pair("6400000000000000", "discover"),
            Pair("6500000000000000", "discover"),
            Pair("6200000000000000", "unionpay"),
            Pair("8100000000000000", "unionpay"),
            // Valid according to Luhn number
            Pair("2204941877211882", "mir"),
            Pair("2720994326581252", "mastercard"),
            Pair("374542158116607", "amex"),
            Pair("36006666333344", "diners"),
            Pair("3541675340715696", "jcb"),
            Pair("3543769248058305", "jcb"),
            Pair("4035501428146300", "cartebancaire"),
            Pair("4111111111111111", "visa"),
            Pair("5346755600299631", "mastercard"),
            Pair("5495770093313616", "mastercard"),
            Pair("5574238524540144", "mastercard"),
            Pair("6011029459267962", "discover"),
            Pair("6278592974938779", "unionpay"),
            Pair("8171999927660000", "unionpay"),
            Pair("30569309025904", "diners"),
            Pair("38520000023237", "diners"),
            Pair("3   8 5 2 0 0 0 0 0 2 3 2 3 7", "diners"),
        )

        for ((cardNumber, cardType) in recognizedCards) {
            assertEquals(cardNumber.creditCardIIN()?.creditCardIssuerNetwork?.name, cardType)
        }

        val unrecognizedCards = listOf(
            "411111111111111",
            "41111111111111111",
            "",
            "9111111111111111",
        )

        for (cardNumber in unrecognizedCards) {
            assertNull(cardNumber.creditCardIIN())
        }
    }

    @Test
    fun `GIVEN a various card type strings WHEN creditCardIssuerNetwork is called THEN the correct CreditCardIssuerNetwork is returned`() {
        val amexCard = CreditCardIssuerNetwork(
            name = CreditCardNetworkType.AMEX.cardName,
            icon = R.drawable.ic_cc_logo_amex,
        )

        assertEquals(amexCard, CreditCardNetworkType.AMEX.cardName.creditCardIssuerNetwork())

        val genericCard = CreditCardIssuerNetwork(
            name = CreditCardNetworkType.GENERIC.cardName,
            icon = R.drawable.ic_icon_credit_card_generic,
        )

        assertEquals(genericCard, "".creditCardIssuerNetwork())
        assertEquals(genericCard, "blah".creditCardIssuerNetwork())
    }
}
