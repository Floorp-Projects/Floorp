/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.creditcards

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class StringTest {

    @Test
    fun `toCreditCardNumber returns a string with only digits `() {
        assertEquals("123456789", "1 234 5678 9".toCreditCardNumber())
        assertEquals("123456789", "1.23.4+5678/9".toCreditCardNumber())
        assertEquals("123456789", ",12r34t5678&9".toCreditCardNumber())
        assertEquals("123456789", " 1 234 5678 9 ".toCreditCardNumber())
        assertEquals("123456789", " abc 1 234  abc 5678 9".toCreditCardNumber())
        assertEquals("123456789", "1-234-5678-9".toCreditCardNumber())
    }

    @Test
    fun `last4Digits returns a string with only last 4 digits `() {
        assertEquals("8431", "371449635398431".last4Digits())
        assertEquals("2345", "12345".last4Digits())
        assertEquals("1234", "1234".last4Digits())
        assertEquals("123", "123".last4Digits())
        assertEquals("1", "1".last4Digits())
        assertEquals("", "".last4Digits())
    }

    @Test
    fun `validateCreditCardNumber returns true for valid credit card numbers `() {
        val americanExpressCard = "371449635398431"
        val dinnersClubCard = "30569309025904"
        val discoverCard = "6011111111111117"
        val jcbCard = "3530111333300000"
        val masterCardCard = "5555555555554444"
        val visaCard = "4111111111111111"

        assertTrue(americanExpressCard.validateCreditCardNumber())
        assertTrue(dinnersClubCard.validateCreditCardNumber())
        assertTrue(discoverCard.validateCreditCardNumber())
        assertTrue(jcbCard.validateCreditCardNumber())
        assertTrue(masterCardCard.validateCreditCardNumber())
        assertTrue(visaCard.validateCreditCardNumber())
    }

    @Test
    fun `validateCreditCardNumber returns false got invalid credit card numbers `() {
        val shortCardNumber = "12345678901"
        val longCardNumber = "12345678901234567890"

        val americanExpressCardInvalid = "371449635398432"
        val dinnersClubCardInvalid = "30569309025905"
        val discoverCardInvalid = "6011111111111118"
        val jcbCardInvalid = "3530111333300001"
        val masterCardCardInvalid = "5555555555554445"
        val visaCardInvalid = "4111111111111112"
        val voyagerCardInvalid = "869941728035896"

        assertFalse(shortCardNumber.validateCreditCardNumber())
        assertFalse(longCardNumber.validateCreditCardNumber())

        assertFalse(americanExpressCardInvalid.validateCreditCardNumber())
        assertFalse(dinnersClubCardInvalid.validateCreditCardNumber())
        assertFalse(discoverCardInvalid.validateCreditCardNumber())
        assertFalse(jcbCardInvalid.validateCreditCardNumber())
        assertFalse(masterCardCardInvalid.validateCreditCardNumber())
        assertFalse(visaCardInvalid.validateCreditCardNumber())
        assertFalse(voyagerCardInvalid.validateCreditCardNumber())
    }

    @Test
    fun `luhnAlgorithmValidation returns false for invalid identification numbers `() {
        // "4242424242424242" is a valid identification number
        assertFalse(luhnAlgorithmValidation("4242424242424240"))
        assertFalse(luhnAlgorithmValidation("4242424242424241"))
        assertFalse(luhnAlgorithmValidation("4242424242424243"))
        assertFalse(luhnAlgorithmValidation("4242424242424244"))
        assertFalse(luhnAlgorithmValidation("4242424242424245"))
        assertFalse(luhnAlgorithmValidation("4242424242424246"))
        assertFalse(luhnAlgorithmValidation("4242424242424247"))
        assertFalse(luhnAlgorithmValidation("4242424242424248"))
        assertFalse(luhnAlgorithmValidation("4242424242424249"))
        assertFalse(luhnAlgorithmValidation("1"))
        assertFalse(luhnAlgorithmValidation("12"))
        assertFalse(luhnAlgorithmValidation("123"))
    }

    @Test
    fun `luhnAlgorithmValidation returns true for valid identification numbers `() {
        assertTrue(luhnAlgorithmValidation("0"))
        assertTrue(luhnAlgorithmValidation("00"))
        assertTrue(luhnAlgorithmValidation("18"))
        assertTrue(luhnAlgorithmValidation("0000000000000000"))
        assertTrue(luhnAlgorithmValidation("4242424242424242"))
        assertTrue(luhnAlgorithmValidation("42424242424242426"))
        assertTrue(luhnAlgorithmValidation("424242424242424267"))
        assertTrue(luhnAlgorithmValidation("4242424242424242675"))
        assertTrue(luhnAlgorithmValidation("000000018"))
        assertTrue(luhnAlgorithmValidation("99999999999999999999"))
        assertTrue(luhnAlgorithmValidation("1234567812345670"))
    }
}
