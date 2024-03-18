/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.locale

import android.content.Context
import android.content.SharedPreferences
import androidx.preference.PreferenceManager
import org.mozilla.focus.R
import org.mozilla.focus.generated.LocalesList
import java.util.Locale
import java.util.concurrent.atomic.AtomicReference

/**
 * This class manages persistence, application, and otherwise handling of
 * user-specified locales.
 *
 * Of note:
 *
 * * It's a singleton, because its scope extends to that of the application,
 * and definitionally all changes to the locale of the app must go through
 * this.
 * * It's lazy.
 * * It relies on using the SharedPreferences file owned by the app for performance.
 */
class LocaleManager {
    // These are volatile because we don't impose restrictions over which thread calls our methods.
    @Volatile
    private var currentLocale: Locale? = null

    private fun getSharedPreferences(context: Context): SharedPreferences {
        if (PREF_LOCALE == null) {
            PREF_LOCALE = context.resources.getString(R.string.pref_key_locale)
        }

        return PreferenceManager.getDefaultSharedPreferences(context)
    }

    /**
     * @return the persisted locale in Java format: "en_US".
     */
    private fun getPersistedLocale(context: Context): String? {
        val settings: SharedPreferences = getSharedPreferences(context)
        return settings.getString(PREF_LOCALE, null)
    }

    /**
     * @return the current locale in Java format: "en_US".
     */
    fun getCurrentLocale(context: Context): Locale? {
        if (currentLocale != null) {
            return currentLocale
        }

        val current = getPersistedLocale(context) ?: return null
        return Locales.parseLocaleCode(current).also { currentLocale = it }
    }

    companion object {
        private var PREF_LOCALE: String? = null
        val instance = AtomicReference<LocaleManager?>()
            get() {
                var localeManager = field.get()
                if (localeManager != null) {
                    return field
                }
                localeManager = LocaleManager()
                return if (field.compareAndSet(null, localeManager)) {
                    AtomicReference(localeManager)
                } else {
                    field
                }
            }

        /**
         * Returns a list of supported locale codes
         */
        val packagedLocaleTags: Collection<String> = LocalesList.BUNDLED_LOCALES
    }
}
