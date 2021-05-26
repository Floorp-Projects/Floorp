/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.creditcards

import androidx.annotation.VisibleForTesting
import mozilla.components.support.utils.creditCardIIN

// Number of last digits to be shown when credit card number is obfuscated.
private const val LAST_VISIBLE_DIGITS_COUNT = 4

/**
 * Strips characters other than digits from a string.
 * Used to strip a credit card number user input of spaces and separators.
 */
fun String.toCreditCardNumber(): String {
    return this.filter { it.isDigit() }
}

/**
 * Returns the last 4 digits from a formatted credit card number string.
 */
fun String.last4Digits(): String {
    return this.takeLast(LAST_VISIBLE_DIGITS_COUNT)
}

/**
 * Returns true if the provided string is a valid credit card by checking if it has a matching
 * credit card issuer network passes the Luhn Algorithm, and false otherwise.
 */
fun String.validateCreditCardNumber(): Boolean {
    val creditCardNumber = this.toCreditCardNumber()

    if (creditCardNumber != this || creditCardNumber.creditCardIIN() == null) {
        return false
    }

    return luhnAlgorithmValidation(creditCardNumber)
}

/**
 * Implementation of Luhn Algorithm validation (https://en.wikipedia.org/wiki/Luhn_algorithm)
 */
@Suppress("MagicNumber")
@VisibleForTesting
internal fun luhnAlgorithmValidation(creditCardNumber: String): Boolean {
    var checksum = 0
    val reversedCardNumber = creditCardNumber.reversed()

    for (index in reversedCardNumber.indices) {
        val digit = Character.getNumericValue(reversedCardNumber[index])
        checksum += if (index % 2 == 0) digit else (digit * 2).let { (it / 10) + (it % 10) }
    }

    return (checksum % 10) == 0
}
