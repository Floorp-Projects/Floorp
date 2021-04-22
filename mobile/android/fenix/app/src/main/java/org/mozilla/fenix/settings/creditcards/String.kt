/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.creditcards

import androidx.annotation.VisibleForTesting

private const val MAX_CREDIT_CARD_NUMBER_LENGTH = 19
private const val MIN_CREDIT_CARD_NUMBER_LENGTH = 12

/**
 * Strips characters other than digits from a string.
 * Used to strip a credit card number user input of spaces and separators.
 */
fun String.toCreditCardNumber(): String {
    return this.filter { it.isDigit() }
}

/**
 * Uses string size and Luhn Algorithm validation to validate a credit card number.
 */
fun String.validateCreditCardNumber(): Boolean {
    val creditCardNumber = this.toCreditCardNumber()

    if (creditCardNumber != this) return false

    // credit card numbers have at least 12 digits and at most 19 digits
    if (creditCardNumber.length < MIN_CREDIT_CARD_NUMBER_LENGTH ||
        creditCardNumber.length > MAX_CREDIT_CARD_NUMBER_LENGTH
    ) return false

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
