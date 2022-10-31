/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils.ext

/**
 * Strips characters other than digits from a string.
 * Used to strip a credit card number user input of spaces and separators.
 */
fun String.toCreditCardNumber(): String {
    return this.filter { it.isDigit() }
}
