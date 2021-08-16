/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import android.os.Bundle
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import mozilla.components.browser.state.state.searchEngines
import org.mozilla.focus.R
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.search.MultiselectSearchEngineListPreference
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.utils.ViewUtils

class RemoveSearchEnginesSettingsFragment : BaseSettingsFragment() {
    override fun onCreatePreferences(p0: Bundle?, p1: String?) {
        setHasOptionsMenu(true)

        addPreferencesFromResource(R.xml.remove_search_engines)
    }

    companion object {
        fun newInstance() = RemoveSearchEnginesSettingsFragment()
    }

    override fun onResume() {
        super.onResume()

        updateTitle(R.string.preference_search_remove_title)
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        super.onCreateOptionsMenu(menu, inflater)
        inflater.inflate(R.menu.menu_remove_search_engines, menu)
    }

    override fun onPrepareOptionsMenu(menu: Menu) {
        super.onPrepareOptionsMenu(menu)
        view?.post {
            val pref = preferenceScreen
                .findPreference(resources.getString(R.string.pref_key_multiselect_search_engine_list))
                as? MultiselectSearchEngineListPreference

            menu.findItem(R.id.menu_delete_items)?.let {
                ViewUtils.setMenuItemEnabled(it, pref!!.atLeastOneEngineChecked())
            }
        }
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.menu_delete_items -> {
                val pref: MultiselectSearchEngineListPreference? = preferenceScreen
                    .findPreference(resources.getString(R.string.pref_key_multiselect_search_engine_list))

                val enginesToRemove = pref!!.checkedEngineIds
                TelemetryWrapper.removeSearchEnginesEvent(enginesToRemove.size)

                requireComponents.store.state.search.searchEngines.filter { searchEngine ->
                    searchEngine.id in enginesToRemove
                }.forEach { searchEngine ->
                    requireComponents.searchUseCases.removeSearchEngine(searchEngine)
                }

                requireComponents.appStore.dispatch(
                    AppAction.NavigateUp(requireComponents.store.state.selectedTabId)
                )
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }
}
