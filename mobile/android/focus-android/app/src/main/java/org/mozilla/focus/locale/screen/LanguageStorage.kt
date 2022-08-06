/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.locale.screen

import android.content.Context
import android.content.SharedPreferences
import androidx.preference.PreferenceManager
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.focus.R
import org.mozilla.focus.locale.LocaleManager
import java.util.Arrays

class LanguageStorage(private val context: Context) {

    /**
     * Returns the current selected Language object or System default Language if nothing is selected
     */
    fun getSelectedLanguageTag(): Language {
        val sharedConfig: SharedPreferences = PreferenceManager.getDefaultSharedPreferences(context)
        val languageTag = sharedConfig.getString(
            context.resources.getString(R.string.pref_key_locale),
            LOCALE_SYSTEM_DEFAULT
        ) ?: LOCALE_SYSTEM_DEFAULT
        for (language in getLanguages()) {
            if (languageTag == language.tag) {
                return language
            }
        }
        return Language(context.getString(R.string.preference_language_systemdefault), LOCALE_SYSTEM_DEFAULT, 0)
    }

    /**
     * Returns The full list of languages available.System default Language  will be the first item in the list .
     */
    fun getLanguages(): List<Language> {
        val listLocaleNameAndTag = ArrayList<Language>()
        val descriptors = getUsableLocales()
        listLocaleNameAndTag.add(
            Language(
                context.getString(
                    R.string.preference_language_systemdefault
                ),
                LOCALE_SYSTEM_DEFAULT, 0
            )
        )
        descriptors.indices.forEach { i ->
            val displayName = descriptors[i]!!.getNativeName()
            val tag = descriptors[i]!!.getTag()
            Logger.info("$displayName => $tag ")
            listLocaleNameAndTag.add(Language(displayName = displayName, tag = tag, index = i + 1))
        }
        return listLocaleNameAndTag
    }

    /**
     * Saves the current selected language tag
     *
     * @property languageTag the tag of the language
     */
    fun saveCurrentLanguageInSharePref(languageTag: String) {
        val sharedPref = PreferenceManager.getDefaultSharedPreferences(context)
        with(sharedPref.edit()) {
            putString(context.getString(R.string.pref_key_locale), languageTag)
            apply()
        }
    }

    /**
     * This method generates the descriptor array.
     */
    private fun getUsableLocales(): Array<LocaleDescriptor?> {
        val shippingLocales = LocaleManager.getPackagedLocaleTags()
        val initialCount: Int = shippingLocales.size
        val locales: MutableSet<LocaleDescriptor> = HashSet(initialCount)
        for (tag in shippingLocales) {
            locales.add(LocaleDescriptor(tag))
        }
        val usableCount = locales.size
        val descriptors: Array<LocaleDescriptor?> = locales.toTypedArray()
        Arrays.sort(descriptors, 0, usableCount)
        return descriptors
    }

    companion object {
        const val LOCALE_SYSTEM_DEFAULT = "LOCALE_SYSTEM_DEFAULT"
    }
}
