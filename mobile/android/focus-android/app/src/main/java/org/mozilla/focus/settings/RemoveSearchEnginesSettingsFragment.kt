/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import android.os.Bundle
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import mozilla.components.browser.state.state.searchEngines
import org.mozilla.focus.GleanMetrics.SearchEngines
import org.mozilla.focus.R
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.ext.showToolbar
import org.mozilla.focus.search.MultiselectSearchEngineListPreference
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.utils.ViewUtils

class RemoveSearchEnginesSettingsFragment : BaseSettingsFragment() {
    override fun onCreatePreferences(p0: Bundle?, p1: String?) {
        addPreferencesFromResource(R.xml.remove_search_engines)
    }

    companion object {
        fun newInstance() = RemoveSearchEnginesSettingsFragment()
    }

    override fun onResume() {
        super.onResume()

        showToolbar(getString(R.string.preference_search_remove_title))
    }

    override fun onCreateMenu(menu: Menu, menuInflater: MenuInflater) {
        super.onCreateMenu(menu, menuInflater)
        menuInflater.inflate(R.menu.menu_remove_search_engines, menu)
    }

    override fun onPrepareMenu(menu: Menu) {
        super.onPrepareMenu(menu)
        view?.post {
            val pref = preferenceScreen
                .findPreference(resources.getString(R.string.pref_key_multiselect_search_engine_list))
                as? MultiselectSearchEngineListPreference

            menu.findItem(R.id.menu_delete_items)?.let {
                ViewUtils.setMenuItemEnabled(it, pref!!.atLeastOneEngineChecked())
            }
        }
    }

    override fun onMenuItemSelected(menuItem: MenuItem): Boolean {
        return when (menuItem.itemId) {
            R.id.menu_delete_items -> {
                val pref: MultiselectSearchEngineListPreference? = preferenceScreen
                    .findPreference(resources.getString(R.string.pref_key_multiselect_search_engine_list))

                val enginesToRemove = pref!!.checkedEngineIds

                requireComponents.store.state.search.searchEngines.filter { searchEngine ->
                    searchEngine.id in enginesToRemove
                }.forEach { searchEngine ->
                    requireComponents.searchUseCases.removeSearchEngine(searchEngine)
                }

                SearchEngines.removeEngines.record(SearchEngines.RemoveEnginesExtra(enginesToRemove.size))

                requireComponents.appStore.dispatch(
                    AppAction.NavigateUp(requireComponents.store.state.selectedTabId),
                )
                true
            }
            else -> false
        }
    }
}
