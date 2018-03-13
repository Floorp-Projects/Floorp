/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils

import android.content.Context
import android.content.SharedPreferences
import android.content.res.Resources
import android.preference.PreferenceManager

import org.mozilla.focus.R
import org.mozilla.focus.fragment.FirstrunFragment
import org.mozilla.focus.search.SearchEngine

/**
 * A simple wrapper for SharedPreferences that makes reading preference a little bit easier.
 */
@Suppress("TooManyFunctions") // This class is designed to have a lot of (simple) functions
class Settings private constructor(context: Context) {
    companion object {
        private var instance: Settings? = null

        @JvmStatic
        @Synchronized
        fun getInstance(context: Context): Settings {
            if (instance == null) {
                instance = Settings(context.applicationContext)
            }
            return instance ?: throw AssertionError("Instance cleared")
        }
    }

    private val preferences: SharedPreferences = PreferenceManager.getDefaultSharedPreferences(context)
    private val resources: Resources = context.resources

    val defaultSearchEngineName: String?
        get() = preferences.getString(getPreferenceKey(R.string.pref_key_search_engine), null)

    fun shouldBlockImages(): Boolean =
            // Not shipping in v1 (#188)
            /* preferences.getBoolean(
                    resources.getString(R.string.pref_key_performance_block_images),
                    false); */
            false

    fun shouldBlockJavaScript(): Boolean =
            preferences.getBoolean(
                getPreferenceKey(R.string.pref_key_performance_block_javascript),
                false)

    fun shouldShowFirstrun(): Boolean =
            !preferences.getBoolean(FirstrunFragment.FIRSTRUN_PREF, false)

    fun shouldUseSecureMode(): Boolean =
            preferences.getBoolean(getPreferenceKey(R.string.pref_key_secure), false)

    fun setDefaultSearchEngine(searchEngine: SearchEngine) {
        preferences.edit()
                .putString(getPreferenceKey(R.string.pref_key_search_engine), searchEngine.name)
                .apply()
    }

    fun shouldAutocompleteFromShippedDomainList() =
            preferences.getBoolean(
                    getPreferenceKey(R.string.pref_key_autocomplete_preinstalled),
                    true)

    fun shouldAutocompleteFromCustomDomainList() =
            preferences.getBoolean(
                    getPreferenceKey(R.string.pref_key_autocomplete_custom),
                    false)

    fun shouldBlockAdTrackers() =
            preferences.getBoolean(
                    getPreferenceKey(R.string.pref_key_privacy_block_ads),
                    true)

    fun shouldBlockAnalyticTrackers() =
            preferences.getBoolean(
                    getPreferenceKey(R.string.pref_key_privacy_block_analytics),
                    true)

    fun shouldBlockSocialTrackers() =
            preferences.getBoolean(
                    getPreferenceKey(R.string.pref_key_privacy_block_social),
                    true)

    fun shouldBlockOtherTrackers() =
            preferences.getBoolean(
                    getPreferenceKey(R.string.pref_key_privacy_block_other),
                    false)

    private fun getPreferenceKey(resourceId: Int): String =
            resources.getString(resourceId)
}
