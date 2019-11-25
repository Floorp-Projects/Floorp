/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Intent
import android.content.SharedPreferences
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import android.os.Bundle
import android.provider.Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS
import androidx.preference.CheckBoxPreference
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import mozilla.components.support.utils.Browsers
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar

/**
 * Lets the user control their default browser preferences
 */
class DefaultBrowserSettingsFragment : PreferenceFragmentCompat() {

    private val preferenceChangeListener =
        SharedPreferences.OnSharedPreferenceChangeListener { sharedPreferences, key ->
            when (key) {
                getPreferenceKey(R.string.pref_key_telemetry) -> {
                    if (sharedPreferences.getBoolean(
                            key,
                            requireContext().settings().isTelemetryEnabled
                        )
                    ) {
                        context?.components?.analytics?.metrics?.start()
                    } else {
                        context?.components?.analytics?.metrics?.stop()
                    }
                }
            }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        context?.let {
            preferenceManager.sharedPreferences.registerOnSharedPreferenceChangeListener(
                preferenceChangeListener
            )
        }

        val makeDefaultBrowserKey = getPreferenceKey(R.string.pref_key_make_default_browser)
        val preferenceMakeDefaultBrowser = findPreference<Preference>(makeDefaultBrowserKey)

        preferenceMakeDefaultBrowser?.onPreferenceClickListener =
            getClickListenerForMakeDefaultBrowser()
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_set_as_default_browser))

        updatePreferences()
    }

    override fun onDestroy() {
        context?.let {
            preferenceManager.sharedPreferences.unregisterOnSharedPreferenceChangeListener(
                preferenceChangeListener
            )
        }
        super.onDestroy()
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.default_browser_preferences, rootKey)
        updatePreferences()
    }

    private fun updatePreferences() {
        findPreference<DefaultBrowserPreference>(getPreferenceKey(R.string.pref_key_make_default_browser))
            ?.updateSwitch()

        findPreference<CheckBoxPreference>(getPreferenceKey(R.string.pref_key_open_links_in_a_private_tab))?.apply {
            isEnabled = Browsers.all(requireContext()).isDefaultBrowser
            isChecked = context.settings().openLinksInAPrivateTab
            onPreferenceChangeListener = SharedPreferenceUpdater()
        }
    }

    private fun getClickListenerForMakeDefaultBrowser(): Preference.OnPreferenceClickListener {
        return if (SDK_INT >= Build.VERSION_CODES.N) {
            Preference.OnPreferenceClickListener {
                val intent = Intent(
                    ACTION_MANAGE_DEFAULT_APPS_SETTINGS
                )
                startActivity(intent)
                true
            }
        } else {
            Preference.OnPreferenceClickListener {
                (activity as HomeActivity).openToBrowserAndLoad(
                    searchTermOrURL = SupportUtils.getSumoURLForTopic(
                        context!!,
                        SupportUtils.SumoTopic.SET_AS_DEFAULT_BROWSER
                    ),
                    newTab = true,
                    from = BrowserDirection.FromDefaultBrowserSettingsFragment
                )
                true
            }
        }
    }
}
