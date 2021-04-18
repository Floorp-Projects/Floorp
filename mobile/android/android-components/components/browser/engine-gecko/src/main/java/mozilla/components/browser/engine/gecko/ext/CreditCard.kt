/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.ext

import mozilla.components.concept.engine.prompt.CreditCard
import org.mozilla.geckoview.Autocomplete

// Placeholder for the card type. This will be replaced when we can identify the card type.
// This is dependent on https://github.com/mozilla-mobile/android-components/issues/9813.
private const val CARD_TYPE_PLACEHOLDER = ""

/**
 * Converts a GeckoView [Autocomplete.CreditCard] to an Android Components [CreditCard].
 */
fun Autocomplete.CreditCard.toCreditCard() = CreditCard(
    guid = guid,
    name = name,
    number = number,
    expiryMonth = expirationMonth,
    expiryYear = expirationYear,
    cardType = CARD_TYPE_PLACEHOLDER
)

/**
 * Converts an Android Components [CreditCard] to a GeckoView [Autocomplete.CreditCard].
 */
fun CreditCard.toAutocompleteCreditCard() = Autocomplete.CreditCard.Builder()
    .guid(guid)
    .name(name)
    .number(number)
    .expirationMonth(expiryMonth)
    .expirationYear(expiryYear)
    .build()
