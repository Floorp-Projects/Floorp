/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.utils

import java.util.Locale

/**
 * Gets a gecko-compatible locale string (e.g. "es-ES" instead of Java [Locale]
 * "es_ES") for the default locale.
 *
 * This method approximates the API21 method [Locale.toLanguageTag].
 *
 * @return a locale string that supports custom injected locale/languages.
 */
internal fun getLocaleTag(): String {
    // Thanks to toLanguageTag() being introduced in API21, we could have
    // simple returned `locale.toLanguageTag();` from this function. However
    // what kind of languages the Android build supports is up to the manufacturer
    // and our apps usually support translations for more rare languages, through
    // our custom locale injector. For this reason, we can't use `toLanguageTag`
    // and must try to replicate its logic ourselves.
    val locale = Locale.getDefault()
    val language = getLanguageFromLocale(locale)
    val country = locale.country // Can be an empty string.
    return if (country.isEmpty()) language else "$language-$country"
}

/**
 * Sometimes we want just the language for a locale, not the entire language
 * tag. But Java's .getLanguage method is wrong. A reference to the deprecated
 * ISO language codes and their mapping can be found in [Locale.toLanguageTag] docs.
 *
 * @param locale a [Locale] object to be stringified.
 * @return a language string, such as "he" for the Hebrew locales.
 */
internal fun getLanguageFromLocale(locale: Locale): String {
    // Can, but should never be, an empty string.
    val language = locale.language

    // Modernize certain language codes.
    return when (language) {
        "iw" -> "he"
        "in" -> "id"
        "ji" -> "yi"
        else -> language
    }
}
