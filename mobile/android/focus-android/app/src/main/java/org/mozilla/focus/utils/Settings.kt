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
import org.mozilla.focus.searchsuggestions.SearchSuggestionsPreferences
import org.mozilla.focus.web.GeckoWebViewProvider

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
    val hasAddedToHomeScreen = context.isInExperiment(homeScreenTipsExperimentDescriptor)

    val defaultSearchEngineName: String
        get() = preferences.getString(getPreferenceKey(R.string.pref_key_search_engine), "")!!

    fun shouldBlockImages(): Boolean =
            // Not shipping in v1 (#188)
            /* preferences.getBoolean(
                    resources.getString(R.string.pref_key_performance_block_images),
                    false); */
            false

    fun shouldEnableRemoteDebugging(): Boolean =
            preferences.getBoolean(
                    getPreferenceKey(R.string.pref_key_remote_debugging),
                    false)

    fun shouldShowSearchSuggestions(): Boolean =
            preferences.getBoolean(
                    getPreferenceKey(R.string.pref_key_show_search_suggestions),
                    false)

    fun shouldBlockWebFonts(): Boolean =
        preferences.getBoolean(
            getPreferenceKey(R.string.pref_key_performance_block_webfonts),
            false)

    fun shouldBlockJavaScript(): Boolean =
            preferences.getBoolean(
                getPreferenceKey(R.string.pref_key_performance_block_javascript),
                false)

    fun shouldBlockCookiesValue(): String =
            preferences.getString(getPreferenceKey(R.string
                    .pref_key_performance_enable_cookies),
                    resources.getString(R.string.preference_privacy_should_block_cookies_no_option))!!

    fun shouldBlockCookies(): Boolean =
            shouldBlockCookiesValue().equals(resources.getString(
                    R.string.preference_privacy_should_block_cookies_yes_option))

    fun shouldBlockThirdPartyCookies(): Boolean =
            shouldBlockCookiesValue().equals(
                    resources.getString(
                            R.string.preference_privacy_should_block_cookies_third_party_only_option)) ||
                    shouldBlockCookiesValue().equals(
                            resources.getString(
                                    R.string.preference_privacy_should_block_cookies_yes_option))

    fun shouldShowFirstrun(): Boolean =
            !preferences.getBoolean(FirstrunFragment.FIRSTRUN_PREF, false)

    fun isFirstGeckoRun(): Boolean =
            preferences.getBoolean(GeckoWebViewProvider.PREF_FIRST_GECKO_RUN, true)

    fun shouldUseBiometrics(): Boolean =
            preferences.getBoolean(getPreferenceKey(R.string.pref_key_biometric), false)

    fun shouldOpenNewTabs(): Boolean =
        preferences.getBoolean(getPreferenceKey(R.string.pref_key_open_new_tab), false)

    fun shouldUseSecureMode(): Boolean =
            preferences.getBoolean(getPreferenceKey(R.string.pref_key_secure), false)

    fun setDefaultSearchEngineByName(name: String) {
        preferences.edit()
                .putString(getPreferenceKey(R.string.pref_key_search_engine), name)
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

    fun userHasToggledSearchSuggestions(): Boolean =
            preferences.getBoolean(SearchSuggestionsPreferences.TOGGLED_SUGGESTIONS_PREF, false)

    fun userHasDismissedNoSuggestionsMessage(): Boolean =
            preferences.getBoolean(SearchSuggestionsPreferences.DISMISSED_NO_SUGGESTIONS_PREF, false)

    fun isDefaultBrowser() = preferences.getBoolean(
        getPreferenceKey(R.string.pref_key_default_browser),
        false)

    fun hasOpenedInNewTab() = preferences.getBoolean(
        getPreferenceKey(R.string.has_opened_new_tab),
        false)

    fun hasRequestedDesktop() = preferences.getBoolean(
        getPreferenceKey(R.string.has_requested_desktop),
        false)

    fun getAppLaunchCount() = preferences.getInt(
            getPreferenceKey(R.string.app_launch_count),
            0)

    private fun getPreferenceKey(resourceId: Int): String =
            resources.getString(resourceId)
}
