/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import android.os.Bundle
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import org.mozilla.focus.R
import org.mozilla.focus.search.MultiselectSearchEngineListPreference
import org.mozilla.focus.search.SearchEngineManager
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.utils.ViewUtils

class RemoveSearchEnginesSettingsFragment : BaseSettingsFragment() {

    companion object {
        fun newInstance() = RemoveSearchEnginesSettingsFragment()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setHasOptionsMenu(true)

        addPreferencesFromResource(R.xml.remove_search_engines)
    }

    override fun onResume() {
        super.onResume()
        getActionBarUpdater().apply {
            updateTitle(R.string.preference_search_remove_title)
            updateIcon(R.drawable.ic_back)
        }
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        super.onCreateOptionsMenu(menu, inflater)
        inflater.inflate(R.menu.menu_remove_search_engines, menu)
    }

    override fun onPrepareOptionsMenu(menu: Menu?) {
        super.onPrepareOptionsMenu(menu)
        view?.post({
            val pref = preferenceScreen
                .findPreference(resources.getString(R.string.pref_key_multiselect_search_engine_list))
                as MultiselectSearchEngineListPreference

            menu?.findItem(R.id.menu_delete_items)?.let {
                ViewUtils.setMenuItemEnabled(it, pref.atLeastOneEngineChecked())
            }
        })
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.menu_delete_items -> {
                val pref = preferenceScreen
                    .findPreference(resources.getString(R.string.pref_key_multiselect_search_engine_list))

                val enginesToRemove = (pref as MultiselectSearchEngineListPreference).checkedEngineIds
                TelemetryWrapper.removeSearchEnginesEvent(enginesToRemove.size)
                SearchEngineManager.removeSearchEngines(enginesToRemove, getSearchEngineSharedPreferences())
                fragmentManager.popBackStack()
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }
}
