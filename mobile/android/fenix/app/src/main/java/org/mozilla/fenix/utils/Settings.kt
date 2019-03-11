package org.mozilla.fenix.utils

/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.content.Context
import android.content.SharedPreferences
import android.preference.PreferenceManager
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getPreferenceKey

/**
 * A simple wrapper for SharedPreferences that makes reading preference a little bit easier.
 */
class Settings private constructor(context: Context) {

    companion object {
        var instance: Settings? = null

        @JvmStatic
        @Synchronized
        fun getInstance(context: Context): Settings {
            if (instance == null) {
                instance = Settings(context.applicationContext)
            }
            return instance ?: throw AssertionError("Instance cleared")
        }
    }

    private val appContext = context.applicationContext

    private val preferences: SharedPreferences =
        PreferenceManager.getDefaultSharedPreferences(context)

    val defaultSearchEngineName: String
        get() = preferences.getString(appContext.getPreferenceKey(R.string.pref_key_search_engine), "") ?: ""

    val isTelemetryEnabled: Boolean
        get() = preferences.getBoolean(appContext.getPreferenceKey(R.string.pref_key_telemetry), true)

    fun setDefaultSearchEngineByName(name: String) {
        preferences.edit()
            .putString(appContext.getPreferenceKey(R.string.pref_key_search_engine), name)
            .apply()
    }

    fun showSearchSuggestions(): Boolean = preferences.getBoolean(
        appContext.getPreferenceKey(R.string.pref_key_show_search_suggestions),
        true
    )
}
