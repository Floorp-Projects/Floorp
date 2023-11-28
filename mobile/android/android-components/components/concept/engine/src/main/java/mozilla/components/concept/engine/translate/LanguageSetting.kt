/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.translate

/**
 * The preferences setting a given language may have on the translations engine.
 *
 * @param languageSetting The specified language setting.
 */
enum class LanguageSetting(private val languageSetting: String) {
    /**
     * The translations engine should always expect a given language to be translated and
     * automatically translate on page load.
     */
    ALWAYS("always"),

    /**
     * The translations engine should offer a given language to be translated. This is the default
     * setting. Note, this means the language will parallel the global offer setting
     */
    OFFER("offer"),

    /**
     * The translations engine should never offer to translate a given language.
     */
    NEVER("never"),
    ;

    companion object {
        /**
         * Convenience method to map a string name to the enumerated type.
         *
         * @param languageSetting The specified language setting.
         */
        fun fromValue(languageSetting: String): LanguageSetting = when (languageSetting) {
            "always" -> ALWAYS
            "offer" -> OFFER
            "never" -> NEVER
            else ->
                throw IllegalArgumentException("The language setting $languageSetting is not mapped.")
        }
    }
}
