/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import android.os.Bundle
import android.preference.Preference
import android.preference.PreferenceScreen
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import org.mozilla.focus.R
import org.mozilla.focus.search.RadioSearchEngineListPreference
import org.mozilla.focus.search.SearchEngineManager
import org.mozilla.focus.telemetry.TelemetryWrapper

class InstalledSearchEnginesSettingsFragment : BaseSettingsFragment() {

    companion object {
        fun newInstance() = InstalledSearchEnginesSettingsFragment()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setHasOptionsMenu(true)
    }

    override fun onResume() {
        super.onResume()
        getActionBarUpdater().apply {
            updateTitle(R.string.preference_search_installed_search_engines)
            updateIcon(R.drawable.ic_back)
        }

        refetchSearchEngines()
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        super.onCreateOptionsMenu(menu, inflater)
        inflater.inflate(R.menu.menu_search_engines, menu)
    }

    override fun onPrepareOptionsMenu(menu: Menu?) {
        super.onPrepareOptionsMenu(menu)
        menu?.findItem(R.id.menu_restore_default_engines)?.let {
            it.isEnabled = !SearchEngineManager.hasAllDefaultSearchEngines(getSearchEngineSharedPreferences())
        }
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.menu_remove_search_engines -> {
                navigateToFragment(RemoveSearchEnginesSettingsFragment())
                TelemetryWrapper.menuRemoveEnginesEvent()
                true
            }
            R.id.menu_restore_default_engines -> {
                SearchEngineManager.restoreDefaultSearchEngines(getSearchEngineSharedPreferences())
                refetchSearchEngines()
                TelemetryWrapper.menuRestoreEnginesEvent()
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }

    override fun onPreferenceTreeClick(preferenceScreen: PreferenceScreen?, preference: Preference?): Boolean {
        return when (preference?.key) {
            resources.getString(R.string.pref_key_manual_add_search_engine) -> {
                navigateToFragment(ManualAddSearchEngineSettingsFragment())
                TelemetryWrapper.menuAddSearchEngineEvent()
                return true
            }
            else -> {
                super.onPreferenceTreeClick(preferenceScreen, preference)
            }
        }
    }

    /**
     * Refresh search engines list.
     */
    private fun refetchSearchEngines() {
        // Refresh this preference screen to display changes.
        preferenceScreen?.removeAll()
        addPreferencesFromResource(R.xml.search_engine_settings)

        val pref = preferenceScreen.findPreference(resources.getString(R.string.pref_key_radio_search_engine_list))
        (pref as RadioSearchEngineListPreference).refetchSearchEngines()
    }
}
