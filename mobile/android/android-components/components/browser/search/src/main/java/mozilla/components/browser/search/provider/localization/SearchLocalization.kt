/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.search.provider.localization

/**
 * Data class representing the result of calling [SearchLocalizationProvider.determineRegion].
 *
 * @property language ISO 639 alpha-2 or alpha-3 language code, or registered language subtags up to
 * 8 alpha letters (for future enhancements). Example: "en" (English), "ja" (Japanese), "kok" (Konkani)
 * @property country ISO 3166 alpha-2 country code or UN M.49 numeric-3 area code. Example: "US"
 * (United States), "FR" (France), "029" (Caribbean)
 * @property region
 */
data class SearchLocalization(
    val language: String,
    val country: String,
    val region: String? = null
) {
    /**
     * Returns a language tag of the form <language>-<country>.
     *
     * Example: "en-US" (English, United States)
     */
    val languageTag: String
        get() = "$language-$country"
}
