/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.search.provider.localization

import java.util.Locale

/**
 * LocalizationProvider implementation that only provides the language and country from the system's
 * default languageTag.
 */
class LocaleSearchLocalizationProvider : SearchLocalizationProvider() {
    override val language: String = Locale.getDefault().language

    override val country: String = Locale.getDefault().country

    override val region: String? = null
}
