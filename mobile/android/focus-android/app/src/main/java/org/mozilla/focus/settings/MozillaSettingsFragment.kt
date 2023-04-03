/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import android.os.Bundle
import androidx.preference.Preference
import mozilla.components.browser.state.state.SessionState
import org.mozilla.focus.R
import org.mozilla.focus.browser.LocalizedContent
import org.mozilla.focus.ext.components
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.ext.showToolbar
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.Screen
import org.mozilla.focus.utils.AppConstants
import org.mozilla.focus.utils.SupportUtils

class MozillaSettingsFragment :
    BaseSettingsFragment() {
    override fun onCreatePreferences(p0: Bundle?, p1: String?) {
        addPreferencesFromResource(R.xml.mozilla_settings)
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preference_category_mozilla))
    }

    override fun onPreferenceTreeClick(preference: Preference): Boolean {
        // AppCompatActivity has a Toolbar that is used as the ActionBar, and it conflicts with the ActionBar
        // used by PreferenceScreen to create the headers (with title, back navigation), so we wrap all these
        // "preference screens" into separate activities.
        val activity = activity ?: return super.onPreferenceTreeClick(preference)

        when (preference.key) {
            resources.getString(R.string.pref_key_about) -> {
                requireComponents.appStore.dispatch(
                    AppAction.OpenSettings(Screen.Settings.Page.About),
                )
            }
            resources.getString(R.string.pref_key_help) -> run {
                val tabId = activity.components.tabsUseCases.addTab(
                    SupportUtils.HELP_URL,
                    source = SessionState.Source.Internal.Menu,
                    selectTab = true,
                    private = true,
                )
                requireComponents.appStore.dispatch(AppAction.OpenTab(tabId))
            }
            resources.getString(R.string.pref_key_rights) -> run {
                val tabId = activity.components.tabsUseCases.addTab(
                    LocalizedContent.URL_RIGHTS,
                    source = SessionState.Source.Internal.Menu,
                    selectTab = true,
                    private = true,
                )
                requireComponents.appStore.dispatch(AppAction.OpenTab(tabId))
            }
            resources.getString(R.string.pref_key_privacy_notice) -> {
                val url = if (AppConstants.isKlarBuild) {
                    SupportUtils.PRIVACY_NOTICE_KLAR_URL
                } else {
                    SupportUtils.PRIVACY_NOTICE_URL
                }

                val tabId = activity.components.tabsUseCases.addTab(
                    url,
                    source = SessionState.Source.Internal.Menu,
                    selectTab = true,
                    private = true,
                )
                requireComponents.appStore.dispatch(AppAction.OpenTab(tabId))
            }
            resources.getString(R.string.pref_key_licensing_info) -> {
                val tabId = activity.components.tabsUseCases.addTab(
                    "about:license",
                    source = SessionState.Source.Internal.Menu,
                    selectTab = true,
                    private = true,
                )
                requireComponents.appStore.dispatch(AppAction.OpenTab(tabId))
            }
            resources.getString(R.string.pref_key_libraries_we_use) -> {
                requireComponents.appStore.dispatch(
                    AppAction.OpenSettings(Screen.Settings.Page.Licenses),
                )
            }
        }
        return super.onPreferenceTreeClick(preference)
    }

    companion object {
        fun newInstance(): MozillaSettingsFragment = MozillaSettingsFragment()
    }
}
