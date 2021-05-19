/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.prompt

import android.os.Parcelable
import kotlinx.android.parcel.Parcelize

/**
 * Value type that represents a credit card.
 *
 * @property guid The unique identifier for this credit card.
 * @property name The credit card billing name.
 * @property number The credit card number.
 * @property expiryMonth The credit card expiry month.
 * @property expiryYear The credit card expiry year.
 * @property cardType The credit card network ID.
 */
@Parcelize
data class CreditCard(
    val guid: String?,
    val name: String,
    val number: String,
    val expiryMonth: String,
    val expiryYear: String,
    val cardType: String
) : Parcelable {
    val obfuscatedCardNumber: String
        get() = ellipsesStart +
            ellipsis + ellipsis + ellipsis + ellipsis +
            number.substring(number.length - digitsToShow) +
            ellipsesEnd

    companion object {
        // Left-To-Right Embedding (LTE) mark
        const val ellipsesStart = "\u202A"

        // One dot ellipsis
        const val ellipsis = "\u2022\u2060\u2006\u2060"

        // Pop Directional Formatting (PDF) mark
        const val ellipsesEnd = "\u202C"

        // Number of digits to be displayed after ellipses on an obfuscated credit card number.
        const val digitsToShow = 4
    }
}
