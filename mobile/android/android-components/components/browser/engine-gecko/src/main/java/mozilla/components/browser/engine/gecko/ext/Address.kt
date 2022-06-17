/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.ext

import mozilla.components.concept.storage.Address
import org.mozilla.geckoview.Autocomplete

/**
 * Converts a GeckoView [Autocomplete.Address] to an Android Components [Address].
 */
fun Autocomplete.Address.toAddress() = Address(
    guid = guid ?: "",
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
    email = email
)

/**
 * Converts an Android Components [Address] to a GeckoView [Autocomplete.Address].
 */
fun Address.toAutocompleteAddress() = Autocomplete.Address.Builder()
    .guid(guid)
    .name(fullName)
    .givenName(givenName)
    .additionalName(additionalName)
    .familyName(familyName)
    .organization(organization)
    .streetAddress(streetAddress)
    .addressLevel3(addressLevel3)
    .addressLevel2(addressLevel2)
    .addressLevel1(addressLevel1)
    .postalCode(postalCode)
    .country(country)
    .tel(tel)
    .email(email)
    .build()
