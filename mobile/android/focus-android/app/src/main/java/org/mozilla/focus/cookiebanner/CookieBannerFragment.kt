/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.cookiebanner

import android.os.Bundle
import androidx.preference.CheckBoxPreference
import org.mozilla.focus.GleanMetrics.CookieBanner
import org.mozilla.focus.R
import org.mozilla.focus.ext.components
import org.mozilla.focus.ext.requirePreference
import org.mozilla.focus.ext.settings
import org.mozilla.focus.ext.showToolbar
import org.mozilla.focus.settings.BaseSettingsFragment

class CookieBannerFragment : BaseSettingsFragment() {
    private lateinit var rejectAllCookies: CookieBannerRejectAllPreference
    private lateinit var rejectOrAcceptAllCookies: CheckBoxPreference

    override fun onStart() {
        super.onStart()
        showToolbar(getString(R.string.preferences_cookie_banner))
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        addPreferencesFromResource(R.xml.cookie_banner_settings)
        setupPreferences()
        setupInitialState()
        setupOnPreferenceChangeListener()
    }

    private fun setupPreferences() {
        rejectAllCookies = requirePreference(R.string.pref_key_cookie_banner_reject_all)
        rejectOrAcceptAllCookies = requirePreference(R.string.pref_key_cookie_banner_reject_or_accept)
    }

    private fun setupInitialState() {
        when (requireContext().settings.getCurrentCookieBannerOptionFromSharePref()) {
            is CookieBannerOption.CookieBannerDisabled -> {
                rejectAllCookies.isChecked = false
            }
            is CookieBannerOption.CookieBannerRejectAll -> {
                rejectAllCookies.isChecked = true
                rejectOrAcceptAllCookies.isVisible = true
                rejectOrAcceptAllCookies.isChecked = false
            }
            is CookieBannerOption.CookieBannerRejectOrAccept -> {
                rejectAllCookies.isChecked = true
                rejectOrAcceptAllCookies.isVisible = true
                rejectOrAcceptAllCookies.isChecked = true
            }
        }
    }

    private fun setupOnPreferenceChangeListener() {
        rejectAllCookies.setOnPreferenceChangeListener { _, newValue ->
            val enableRejectAllCookies = newValue as Boolean
            rejectOrAcceptAllCookies.isVisible = enableRejectAllCookies
            val cookieBannerOption: CookieBannerOption

            if (enableRejectAllCookies) {
                cookieBannerOption = CookieBannerOption.CookieBannerRejectAll()
            } else {
                rejectOrAcceptAllCookies.isChecked = false
                cookieBannerOption = CookieBannerOption.CookieBannerDisabled()
            }

            handleCookieBannerChange(cookieBannerOption)
            true
        }
        rejectOrAcceptAllCookies.setOnPreferenceChangeListener { _, newValue ->
            val enableRejectOrAcceptAllCookies = newValue as Boolean

            val cookieBannerOption: CookieBannerOption = if (enableRejectOrAcceptAllCookies) {
                CookieBannerOption.CookieBannerRejectOrAccept()
            } else {
                CookieBannerOption.CookieBannerRejectAll()
            }

            handleCookieBannerChange(cookieBannerOption)
            true
        }
    }

    private fun handleCookieBannerChange(cookieBannerOption: CookieBannerOption) {
        CookieBanner.settingChanged.record(CookieBanner.SettingChangedExtra(cookieBannerOption.metricTag))
        requireContext().settings.saveCurrentCookieBannerOptionInSharePref(cookieBannerOption)
        requireContext().components.engine.settings.cookieBannerHandlingModePrivateBrowsing = cookieBannerOption.mode
        requireContext().components.sessionUseCases.reload()
    }
}
