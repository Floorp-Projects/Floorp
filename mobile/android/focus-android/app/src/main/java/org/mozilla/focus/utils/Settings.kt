/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils

import android.content.Context
import android.content.SharedPreferences
import android.content.res.Resources
import androidx.preference.PreferenceManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import org.mozilla.focus.R
import org.mozilla.focus.engine.EngineSharedPreferencesListener
import org.mozilla.focus.fragment.FirstrunFragment
import org.mozilla.focus.searchsuggestions.SearchSuggestionsPreferences

/**
 * A simple wrapper for SharedPreferences that makes reading preference a little bit easier.
 */
@Suppress("TooManyFunctions") // This class is designed to have a lot of (simple) functions
class Settings private constructor(
    private val context: Context
) {
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

    private val preferencesListener = EngineSharedPreferencesListener(context)

    private val preferences: SharedPreferences = PreferenceManager.getDefaultSharedPreferences(context).apply {
        registerOnSharedPreferenceChangeListener(preferencesListener)
    }

    fun createTrackingProtectionPolicy(): EngineSession.TrackingProtectionPolicy {
        val trackingCategories: MutableList<EngineSession.TrackingProtectionPolicy.TrackingCategory> = mutableListOf()

        if (shouldBlockSocialTrackers()) {
            trackingCategories.add(EngineSession.TrackingProtectionPolicy.TrackingCategory.SOCIAL)
        }
        if (shouldBlockAdTrackers()) {
            trackingCategories.add(EngineSession.TrackingProtectionPolicy.TrackingCategory.AD)
        }
        if (shouldBlockAnalyticTrackers()) {
            trackingCategories.add(EngineSession.TrackingProtectionPolicy.TrackingCategory.ANALYTICS)
        }
        if (shouldBlockOtherTrackers()) {
            trackingCategories.add(EngineSession.TrackingProtectionPolicy.TrackingCategory.SCRIPTS_AND_SUB_RESOURCES)
        }

        val cookiePolicy = when (shouldBlockCookiesValue()) {
            context.getString(R.string.preference_privacy_should_block_cookies_yes_option2) ->
                EngineSession.TrackingProtectionPolicy.CookiePolicy.ACCEPT_NONE

            context.getString(R.string.preference_privacy_should_block_cookies_third_party_tracker_cookies_option) ->
                EngineSession.TrackingProtectionPolicy.CookiePolicy.ACCEPT_NON_TRACKERS

            context.getString(R.string.preference_privacy_should_block_cookies_third_party_only_option) ->
                EngineSession.TrackingProtectionPolicy.CookiePolicy.ACCEPT_ONLY_FIRST_PARTY

            else -> EngineSession.TrackingProtectionPolicy.CookiePolicy.ACCEPT_ALL
        }

        return EngineSession.TrackingProtectionPolicy.select(
            cookiePolicy = cookiePolicy,
            trackingCategories = trackingCategories.toTypedArray(),
            strictSocialTrackingProtection = shouldBlockSocialTrackers()
        )
    }

    fun setupSafeBrowsing(engine: Engine) {
        if (shouldUseSafeBrowsing()) {
            engine.settings.safeBrowsingPolicy = arrayOf(EngineSession.SafeBrowsingPolicy.RECOMMENDED)
        } else {
            engine.settings.safeBrowsingPolicy = arrayOf(EngineSession.SafeBrowsingPolicy.NONE)
        }
    }

    private val resources: Resources = context.resources
    val hasAddedToHomeScreen: Boolean
        get() = preferences.getBoolean(getPreferenceKey(R.string.has_added_to_home_screen), false)

    @Deprecated("This is no longer used. Read search engines from BrowserStore instead")
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
            false
        )

    fun shouldDisplayHomescreenTips() =
        preferences.getBoolean(
            getPreferenceKey(R.string.pref_key_homescreen_tips),
            true
        )

    fun shouldShowSearchSuggestions(): Boolean =
        preferences.getBoolean(
            getPreferenceKey(R.string.pref_key_show_search_suggestions),
            false
        )

    fun shouldBlockWebFonts(): Boolean =
        preferences.getBoolean(
            getPreferenceKey(R.string.pref_key_performance_block_webfonts),
            false
        )

    fun shouldBlockJavaScript(): Boolean =
        preferences.getBoolean(
            getPreferenceKey(R.string.pref_key_performance_block_javascript),
            false
        )

    fun shouldBlockCookiesValue(): String =
        preferences.getString(
            getPreferenceKey(
                R.string
                    .pref_key_performance_enable_cookies
            ),
            resources.getString(R.string.preference_privacy_should_block_cookies_third_party_tracker_cookies_option)
        )!!

    fun shouldBlockCookies(): Boolean =
        shouldBlockCookiesValue() == resources.getString(
            R.string.preference_privacy_should_block_cookies_yes_option2
        )

    fun shouldBlockThirdPartyCookies(): Boolean =
        shouldBlockCookiesValue() == resources.getString(
            R.string.preference_privacy_should_block_cookies_third_party_only_option
        ) ||
            shouldBlockCookiesValue() == resources.getString(
                R.string.preference_privacy_should_block_cookies_yes_option2
            )

    fun shouldShowFirstrun(): Boolean =
        !preferences.getBoolean(FirstrunFragment.FIRSTRUN_PREF, false)

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
            true
        )

    fun shouldAutocompleteFromCustomDomainList() =
        preferences.getBoolean(
            getPreferenceKey(R.string.pref_key_autocomplete_custom),
            true
        )

    fun shouldBlockAdTrackers() =
        preferences.getBoolean(
            getPreferenceKey(R.string.pref_key_privacy_block_ads),
            true
        )

    fun shouldUseSafeBrowsing() =
        preferences.getBoolean(
            getPreferenceKey(R.string.pref_key_safe_browsing),
            true
        )

    fun shouldBlockAnalyticTrackers() =
        preferences.getBoolean(
            getPreferenceKey(R.string.pref_key_privacy_block_analytics),
            true
        )

    fun shouldBlockSocialTrackers() =
        preferences.getBoolean(
            getPreferenceKey(R.string.pref_key_privacy_block_social),
            true
        )

    fun shouldBlockOtherTrackers() =
        preferences.getBoolean(
            getPreferenceKey(R.string.pref_key_privacy_block_other2),
            true
        )

    fun userHasToggledSearchSuggestions(): Boolean =
        preferences.getBoolean(SearchSuggestionsPreferences.TOGGLED_SUGGESTIONS_PREF, false)

    fun userHasDismissedNoSuggestionsMessage(): Boolean =
        preferences.getBoolean(SearchSuggestionsPreferences.DISMISSED_NO_SUGGESTIONS_PREF, false)

    fun isDefaultBrowser() = preferences.getBoolean(
        getPreferenceKey(R.string.pref_key_default_browser),
        false
    )

    fun hasOpenedInNewTab() = preferences.getBoolean(
        getPreferenceKey(R.string.has_opened_new_tab),
        false
    )

    fun hasRequestedDesktop() = preferences.getBoolean(
        getPreferenceKey(R.string.has_requested_desktop),
        false
    )

    fun getAppLaunchCount() = preferences.getInt(
        getPreferenceKey(R.string.app_launch_count),
        0
    )

    fun getTotalBlockedTrackersCount() = preferences.getInt(
        getPreferenceKey(R.string.pref_key_privacy_total_trackers_blocked_count),
        0
    )

    private fun getPreferenceKey(resourceId: Int): String =
        resources.getString(resourceId)
}
