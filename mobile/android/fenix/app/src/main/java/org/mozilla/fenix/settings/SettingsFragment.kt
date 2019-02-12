/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Intent
import android.os.Bundle
import android.provider.Settings
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.navigation.Navigation
import androidx.preference.Preference
import androidx.preference.Preference.OnPreferenceClickListener
import androidx.preference.PreferenceFragmentCompat
import mozilla.components.browser.session.Session
import org.mozilla.fenix.R
import org.mozilla.fenix.R.string.pref_key_about
import org.mozilla.fenix.R.string.pref_key_make_default_browser
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.requireComponents

class SettingsFragment : PreferenceFragmentCompat() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        (activity as AppCompatActivity).supportActionBar?.show()
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.preferences, rootKey)
    }

    override fun onResume() {
        super.onResume()

        setupPreferences()
    }

    private fun setupPreferences() {
        val makeDefaultBrowserKey = context?.getPreferenceKey(pref_key_make_default_browser)
        val aboutKey = context?.getPreferenceKey(pref_key_about)

        val preferenceMakeDefaultBrowser = findPreference<Preference>(makeDefaultBrowserKey)
        val preferenceAbout = findPreference<Preference>(aboutKey)

        preferenceMakeDefaultBrowser.onPreferenceClickListener =
            getClickListenerForMakeDefaultBrowser()
        preferenceAbout.onPreferenceClickListener = getAboutPageListener()
    }

    private val defaultClickListener = OnPreferenceClickListener { preference ->
        Toast.makeText(context, "${preference.title} Clicked", Toast.LENGTH_SHORT).show()
        true
    }

    private fun getClickListenerForMakeDefaultBrowser(): OnPreferenceClickListener {
        return if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.N) {
            OnPreferenceClickListener {
                val intent = Intent(
                    Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS
                )
                startActivity(intent)
                true
            }
        } else {
            defaultClickListener
        }
    }

    private fun getAboutPageListener(): OnPreferenceClickListener {
        return OnPreferenceClickListener {
            requireComponents.core.sessionManager.add(Session(aboutURL), true)
            view?.let {
                Navigation.findNavController(it)
                    .navigate(
                        SettingsFragmentDirections.actionGlobalBrowser(
                            requireComponents.core.sessionManager.selectedSession?.id
                        )
                    )
            }
            true
        }
    }

    companion object {
        const val aboutURL = "about:version"
    }
}
