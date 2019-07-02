/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.locale

import java.util.Locale

fun String.toLocale(): Locale {
    val index: Int = if (contains('-')) {
        indexOf('-')
    } else {
        indexOf('_')
    }
    return if (index != -1) {
        val langCode = substring(0, index)
        val countryCode = substring(index + 1)
        Locale(langCode, countryCode)
    } else {
        Locale(this)
    }
}
