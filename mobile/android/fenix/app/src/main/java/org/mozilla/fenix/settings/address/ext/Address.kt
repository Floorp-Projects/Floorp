/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.address.ext

import androidx.annotation.VisibleForTesting
import mozilla.components.concept.storage.Address

/**
 * Generate a description item text for an [Address]. The element ordering is based on the
 * priorities defined by the desktop code found here:
 * https://searchfox.org/mozilla-central/rev/d989c65584ded72c2de85cb40bede7ac2f176387/toolkit/components/formautofill/FormAutofillUtils.jsm#323
 */
fun Address.getAddressLabel(): String = listOf(
    streetAddress.toOneLineAddress(),
    addressLevel3,
    addressLevel2,
    organization,
    addressLevel1,
    country,
    postalCode,
    tel,
    email,
).filter { it.isNotEmpty() }.joinToString(", ")

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal fun String.toOneLineAddress(): String =
    this.split("\n").joinToString(separator = " ") { it.trim() }
