/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.locale

import android.content.Context
import android.content.SharedPreferences
import android.content.res.Configuration
import androidx.appcompat.app.AppCompatActivity
import mozilla.components.support.base.R
import mozilla.components.support.base.log.logger.Logger
import java.util.Locale

/**
 * Helper for apps that want to change locale defined by the system.
 */
object LocaleManager {
    private val logger = Logger("LocaleManager")

    /**
     * Change the system defined locale to the indicated in the [language] parameter.
     * This new [language] will be stored and will be the new current locale returned by [getCurrentLocale].
     *
     * After calling this function, to visualize the locale changes you have to make sure all your visible activities
     * get recreated. If your app is using the single activity approach, this will be trivial just call
     * [AppCompatActivity.recreate]. On the other hand, if you have multiple activity this could be tricky, one
     * alternative could be restarting your application process see https://github.com/JakeWharton/ProcessPhoenix
     * @return A new Context object for whose resources are adjusted to match the new [language].
     */
    fun setNewLocale(context: Context, language: String): Context {
        Storage.save(context, language)
        return updateResources(context)
    }

    /**
     * The latest stored locale saved by [setNewLocale].
     */
    fun getCurrentLocale(context: Context): Locale? {
        var currentLocale: Locale? = null

        if (currentLocale == null) {
            val locale = Storage.getLocale(context)
            if (locale != null) {
                currentLocale = locale.toLocale()
            }
        }
        return currentLocale
    }

    internal fun updateResources(baseContext: Context): Context {
        val locale = getCurrentLocale(baseContext)
        return if (locale != null) {
            updateSystemLocale(locale)
            updateConfiguration(baseContext, locale)
        } else {
            baseContext
        }
    }

    private fun updateConfiguration(context: Context, locale: Locale): Context {
        val configuration = Configuration(context.resources.configuration)
        configuration.setLocale(locale)
        configuration.setLayoutDirection(locale)
        return context.createConfigurationContext(configuration)
    }

    private fun updateSystemLocale(locale: Locale) {
        Locale.setDefault(locale)
    }

    internal fun clear(context: Context) {
        Storage.clear(context)
    }

    private object Storage {
        private const val PREFERENCE_FILE = "mozac_support_base_locale_manager_preference"
        private var currentLocal: String? = null

        fun getLocale(context: Context): String? {
            return if (currentLocal == null) {
                val settings = getSharedPreferences(context)
                val key = context.getString(R.string.mozac_support_base_locale_preference_key_locale)
                currentLocal = settings.getString(key, null)
                currentLocal
            } else {
                currentLocal
            }
        }

        @Synchronized
        fun save(context: Context, localeCode: String) {
            val settings = getSharedPreferences(context)
            val key = context.getString(R.string.mozac_support_base_locale_preference_key_locale)
            settings.edit().putString(key, localeCode).apply()
            currentLocal = localeCode
        }

        fun clear(context: Context) {
            val settings = getSharedPreferences(context)
            settings.edit().clear().apply()
            currentLocal = null
        }

        private fun getSharedPreferences(context: Context): SharedPreferences {
            return context.getSharedPreferences(PREFERENCE_FILE, Context.MODE_PRIVATE)
        }
    }
}
