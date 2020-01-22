/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.ui

import java.text.NumberFormat
import java.util.Locale

/**
 * Try to find the default language on the map otherwise defaults to "en-US".
 */
fun Map<String, String>.translate(): String {
    val lang = Locale.getDefault().language
    return get(lang) ?: getValue("en-US")
}

/**
 * Get the formatted number amount for the current default locale.
 */
internal fun getFormattedAmount(amount: Int): String {
    return NumberFormat.getNumberInstance(Locale.getDefault()).format(amount)
}
