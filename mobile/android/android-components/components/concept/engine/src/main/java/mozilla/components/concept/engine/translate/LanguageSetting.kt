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

    /**
     * Helper function to transform a given [LanguageSetting] setting into its boolean counterpart.
     *
     * @param categoryToSetFor The [LanguageSetting] type that we would like to determine the
     * boolean value for. For example, if trying to calculate a boolean 'isAlways',
     * [categoryToSetFor] would be [LanguageSetting.ALWAYS].
     *
     * @return A boolean that corresponds to the language setting. Will return null if not enough
     * information is present to make a determination.
     */
    fun toBoolean(
        categoryToSetFor: LanguageSetting,
    ): Boolean? {
        when (this) {
            ALWAYS -> {
                return when (categoryToSetFor) {
                    ALWAYS -> true
                    // Cannot determine offer without more information
                    OFFER -> null
                    NEVER -> false
                }
            }

            OFFER -> {
                return when (categoryToSetFor) {
                    ALWAYS -> false
                    OFFER -> true
                    NEVER -> false
                }
            }

            NEVER -> {
                return when (categoryToSetFor) {
                    ALWAYS -> false
                    // Cannot determine offer without more information
                    OFFER -> null
                    NEVER -> true
                }
            }
        }
    }

    /**
     * Helper function to transform a given [LanguageSetting] that represents a category and the given boolean to its
     * correct [LanguageSetting]. The calling object should be the object to set for.
     *
     * For example, if trying to calculate a value for an `isAlways` boolean, then `this` should be [ALWAYS].
     *
     * @param value The given [Boolean] to convert to a [LanguageSetting].
     * @return A language setting that corresponds to the boolean. Will return null if not enough information is present
     * to make a determination.
     */
    fun toLanguageSetting(
        value: Boolean,
    ): LanguageSetting? {
        when (this) {
            ALWAYS -> {
                return when (value) {
                    true -> ALWAYS
                    false -> NEVER
                }
            }

            OFFER -> {
                return when (value) {
                    true -> OFFER
                    // Cannot determine if it should be ALWAYS or NEVER without more information
                    false -> null
                }
            }

            NEVER -> {
                return when (value) {
                    true -> NEVER
                    false -> ALWAYS
                }
            }
        }
    }
}
