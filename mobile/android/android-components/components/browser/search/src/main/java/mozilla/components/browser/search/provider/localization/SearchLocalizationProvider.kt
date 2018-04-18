/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.search.provider.localization

/**
 * Class providing language, country and optionally region (actual location) of the user/device to
 * customize the search experience.
 */
abstract class SearchLocalizationProvider {
    /**
     * ISO 639 alpha-2 or alpha-3 language code, or registered language subtags up to 8 alpha letters
     * (for future enhancements).
     *
     * Example: "en" (English), "ja" (Japanese), "kok" (Konkani)
     */
    abstract val language: String

    /**
     * ISO 3166 alpha-2 country code or UN M.49 numeric-3 area code.
     *
     * Example: "US" (United States), "FR" (France), "029" (Caribbean)
     */
    abstract val country: String

    /**
     * Optional actual location (region only) of the user/device. ISO 3166 alpha-2 country code or
     * UN M.49 numeric-3 area code.
     *
     * This value is used to customize the experience for users that use a languageTag that doesn't match
     * their geographic region. For example a user in Russia with "en-US" languageTag might get offered
     * an English version of Yandex if the region is set to "RU".
     *
     * Can be null if no location information is available.
     */
    abstract val region: String?

    /**
     * Returns a language tag of the form <language>-<country>.
     *
     * Example: "en-US" (English, United States)
     */
    val languageTag: String
            get() = "$language-$country"
}
