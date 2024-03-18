/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.translate

/**
 * The list of supported languages that may be translated to and translated from. Usually
 * a given language will be bi-directional (translate both to and from),
 * but this is not guaranteed, which is why the support response is two lists.
 *
 * @property fromLanguages The languages that the machine learning model may translate from.
 * @property toLanguages The languages that the machine learning model may translate to.
 */
data class TranslationSupport(
    val fromLanguages: List<Language>? = null,
    val toLanguages: List<Language>? = null,
)

/**
 * Convenience method to convert [this.fromLanguages] and [this.toLanguages] to a single language
 * map for BCP 47 code to [Language] lookup.
 *
 * @return A combined map of the language options with the BCP 47 language as the key and the
 * [Language] object as the value or null.
 */
fun TranslationSupport.toLanguageMap(): Map<String, Language>? {
    val fromLanguagesMap = fromLanguages?.associate { it.code to it }
    val toLanguagesMap = toLanguages?.associate { it.code to it }

    return if (toLanguagesMap != null && fromLanguagesMap != null) {
        toLanguagesMap + fromLanguagesMap
    } else {
        toLanguagesMap
            ?: fromLanguagesMap
    }
}

/**
 * Convenience method to find a [Language] given a BCP 47 language code.
 *
 * @param languageCode The BCP 47 language code.
 *
 * @return The [Language] associated with the language code or null.
 */
fun TranslationSupport.findLanguage(languageCode: String): Language? {
    return toLanguageMap()?.get(languageCode)
}

/**
 * Convenience method to convert a language setting map using a BCP 47 code as a key to a map using
 * [Language] as a key.
 *
 * @param languageSettings The map of language settings, where the key, [String], is a BCP 47 code.
 */
fun TranslationSupport.mapLanguageSettings(
    languageSettings: Map<String, LanguageSetting>?,
): Map<Language?, LanguageSetting>? {
    return languageSettings?.mapKeys { findLanguage(it.key) }?.filterKeys { it != null }
}
