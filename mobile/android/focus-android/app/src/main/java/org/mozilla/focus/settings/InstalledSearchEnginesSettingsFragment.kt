/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import android.os.Bundle
import android.support.v7.preference.Preference
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import org.mozilla.focus.R
import org.mozilla.focus.search.CustomSearchEngineStore
import org.mozilla.focus.search.RadioSearchEngineListPreference
import org.mozilla.focus.telemetry.TelemetryWrapper

class InstalledSearchEnginesSettingsFragment : BaseSettingsFragment() {
    override fun onCreatePreferences(p0: Bundle?, p1: String?) {
        setHasOptionsMenu(true)
    }

    companion object {
        fun newInstance() = InstalledSearchEnginesSettingsFragment()
        var languageChanged: Boolean = false
    }

    override fun onResume() {
        super.onResume()
        getActionBarUpdater().apply {
            updateTitle(R.string.preference_search_installed_search_engines)
            updateIcon(R.drawable.ic_back)
        }

        if (languageChanged)
            restoreSearchEngines()
        else
            refetchSearchEngines()
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        super.onCreateOptionsMenu(menu, inflater)
        inflater.inflate(R.menu.menu_search_engines, menu)
    }

    override fun onPrepareOptionsMenu(menu: Menu?) {
        super.onPrepareOptionsMenu(menu)
        menu?.findItem(R.id.menu_restore_default_engines)?.let {
            it.isEnabled = !CustomSearchEngineStore.hasAllDefaultSearchEngines(activity!!)
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
                restoreSearchEngines()
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }

    private fun restoreSearchEngines() {
        CustomSearchEngineStore.restoreDefaultSearchEngines(activity!!)
        refetchSearchEngines()
        TelemetryWrapper.menuRestoreEnginesEvent()
    }

    override fun onPreferenceTreeClick(preference: Preference): Boolean {
        return when (preference.key) {
            resources.getString(R.string.pref_key_manual_add_search_engine) -> {
                navigateToFragment(ManualAddSearchEngineSettingsFragment())
                TelemetryWrapper.menuAddSearchEngineEvent()
                return true
            }
            else -> {
                super.onPreferenceTreeClick(preference)
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
