/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import android.content.SharedPreferences
import android.os.Bundle
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import androidx.appcompat.app.AppCompatActivity
import mozilla.components.browser.state.state.SessionState
import org.mozilla.focus.R
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.Screen
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.utils.FeatureFlags
import org.mozilla.focus.utils.SupportUtils
import org.mozilla.focus.whatsnew.WhatsNew

class SettingsFragment : BaseSettingsFragment(), SharedPreferences.OnSharedPreferenceChangeListener {

    override fun onCreatePreferences(bundle: Bundle?, s: String?) {
        setHasOptionsMenu(true)
        addPreferencesFromResource(R.xml.settings)
    }

    override fun onResume() {
        super.onResume()

        (requireActivity() as AppCompatActivity).supportActionBar?.customView

        preferenceManager.sharedPreferences.registerOnSharedPreferenceChangeListener(this)

        updateTitle(R.string.menu_settings)
    }

    override fun onPause() {
        preferenceManager.sharedPreferences.unregisterOnSharedPreferenceChangeListener(this)
        super.onPause()
    }

    override fun onPreferenceTreeClick(preference: androidx.preference.Preference): Boolean {
        val resources = resources

        val page = when (preference.key) {
            resources.getString(R.string.pref_key_general_screen) -> Screen.Settings.Page.General
            resources.getString(R.string.pref_key_privacy_security_screen) -> Screen.Settings.Page.Privacy
            resources.getString(R.string.pref_key_search_screen) -> Screen.Settings.Page.Search
            resources.getString(R.string.pref_key_advanced_screen) -> Screen.Settings.Page.Advanced
            resources.getString(R.string.pref_key_mozilla_screen) -> Screen.Settings.Page.Mozilla
            else -> throw IllegalStateException("Unknown preference: ${preference.key}")
        }

        requireComponents.appStore.dispatch(
            AppAction.OpenSettings(page)
        )

        return super.onPreferenceTreeClick(preference)
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences, key: String) {
        TelemetryWrapper.settingsEvent(key, sharedPreferences.all[key].toString())
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        if (FeatureFlags.isMvp) {
            inflater.inflate(R.menu.menu_settings_main, menu)
        }
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        if (item.itemId == R.id.menu_whats_new) {
            whatsNewClicked()
            return true
        }
        return super.onOptionsItemSelected(item)
    }

    private fun whatsNewClicked() {
        val context = requireContext()

        TelemetryWrapper.openWhatsNewEvent(WhatsNew.shouldHighlightWhatsNew(context))
        WhatsNew.userViewedWhatsNew(context)

        val url = SupportUtils.getSumoURLForTopic(context, SupportUtils.SumoTopic.WHATS_NEW)
        requireComponents.tabsUseCases.addTab(url, source = SessionState.Source.MENU, private = true)
    }

    companion object {
        const val TAG = "settings"

        fun newInstance(): SettingsFragment {
            return SettingsFragment()
        }
    }
}
