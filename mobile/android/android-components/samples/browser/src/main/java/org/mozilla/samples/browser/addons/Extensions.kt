/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser.addons

import java.text.NumberFormat
import java.util.Locale

/**
 * Try to find the default language on the map otherwise defaults to "en-US".
 */
internal fun Map<String, String>.translate(): String {
    val lang = Locale.getDefault().isO3Language
    return get(lang) ?: getValue("en-US")
}

internal fun getFormattedAmount(amount: Int): String {
    return NumberFormat.getNumberInstance(Locale.getDefault()).format(amount)
}
