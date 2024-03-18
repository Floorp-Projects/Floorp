/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.locale

import android.content.Context
import android.content.res.Configuration
import android.content.res.Resources
import mozilla.components.support.ktx.android.content.res.locale
import org.mozilla.focus.locale.LocaleManager.Companion.instance
import java.util.Locale

/**
 * This is a helper class to do typical locale switching operations without
 * hitting StrictMode errors or adding boilerplate to common activity
 * subclasses.
 *
 * Inherit from `LocaleAwareFragmentActivity` or `LocaleAwareActivity`.
 */
object Locales {
    /**
     * Sometimes we want just the language for a locale, not the entire language
     * tag. But Java's .getLanguage method is wrong.
     *
     * This method is equivalent to the first part of
     * [Locales.getLanguageTag].
     *
     * @return a language string, such as "he" for the Hebrew locales.
     */
    fun getLanguage(locale: Locale): String {
        // Modernize certain language codes.
        return when (val language = locale.language) {
            "iw" -> {
                "he"
            }

            "in" -> {
                "id"
            }

            "ji" -> {
                "yi"
            }

            else -> language
        }
    }

    /**
     * Gecko uses locale codes like "es-ES", whereas a Java [Locale]
     * stringifies as "es_ES".
     *
     * This method approximates the Java 7 method
     * `Locale#toLanguageTag()`.
     *
     * @return a locale string suitable for passing to Gecko.
     */
    @JvmStatic
    fun getLanguageTag(locale: Locale): String {
        val language = getLanguage(locale)
        val country = locale.country

        return if (country.isEmpty()) {
            language
        } else {
            "$language-$country"
        }
    }

    /**
     * Parses a locale code [String] and returns the corresponding [Locale].
     */
    fun parseLocaleCode(localeCode: String): Locale {
        var index: Int

        if (localeCode.indexOf('-').also { index = it } != -1 ||
            localeCode.indexOf('_').also { index = it } != -1
        ) {
            val langCode = localeCode.substring(0, index)
            val countryCode = localeCode.substring(index + 1)
            return Locale(langCode, countryCode)
        }

        return Locale(localeCode)
    }

    /**
     * Get a Resources instance with the currently selected locale applied.
     */
    fun getLocalizedResources(context: Context): Resources {
        val currentResources = context.resources
        val currentLocale: Locale? = instance.get()?.getCurrentLocale(context)
        val viewLocale = currentResources.locale

        if (currentLocale == null) {
            return currentResources
        }

        if (currentLocale.toLanguageTag() == viewLocale.toLanguageTag()) {
            return currentResources
        }

        val configuration = Configuration(currentResources.configuration)
        configuration.setLocale(currentLocale)
        return context.createConfigurationContext(configuration).resources
    }
}
