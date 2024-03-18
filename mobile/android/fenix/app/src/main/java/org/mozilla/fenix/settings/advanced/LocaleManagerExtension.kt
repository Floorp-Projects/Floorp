/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.advanced

import android.content.Context
import mozilla.components.support.locale.LocaleManager
import mozilla.components.support.locale.toLocale
import org.mozilla.fenix.BuildConfig
import java.util.Locale

/**
 * Returns a list of currently supported locales, with the system default set as the first one
 */
fun LocaleManager.getSupportedLocales(): List<Locale> {
    val resultLocaleList: MutableList<Locale> = ArrayList()
    resultLocaleList.add(0, getSystemDefault())

    resultLocaleList.addAll(
        BuildConfig.SUPPORTED_LOCALE_ARRAY
            .toList()
            .map {
                it.toLocale()
            }.sortedWith(
                compareBy(
                    { it.displayLanguage },
                    { it.displayCountry },
                ),
            ),
    )
    return resultLocaleList
}

/**
 * Returns the locale that corresponds to the language stored locally by us. If no suitable one is found,
 * return default.
 */
fun LocaleManager.getSelectedLocale(
    context: Context,
    localeList: List<Locale> = getSupportedLocales(),
): Locale {
    val selectedLocale = getCurrentLocale(context)?.toLanguageTag()
    val defaultLocale = getSystemDefault()

    return if (selectedLocale == null) {
        defaultLocale
    } else {
        val supportedMatch = localeList
            .firstOrNull { it.toLanguageTag() == selectedLocale }
        supportedMatch ?: defaultLocale
    }
}

fun LocaleManager.isDefaultLocaleSelected(context: Context): Boolean {
    return getCurrentLocale(context) == null
}
