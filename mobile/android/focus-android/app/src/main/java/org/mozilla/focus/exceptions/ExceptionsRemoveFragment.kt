/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.exceptions

import android.content.Context
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.launch
import org.mozilla.focus.GleanMetrics.TrackingProtectionExceptions
import org.mozilla.focus.R
import org.mozilla.focus.ext.components
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.ext.showToolbar
import org.mozilla.focus.state.AppAction
import kotlin.collections.forEach as withEach

class ExceptionsRemoveFragment : ExceptionsListFragment() {

    override fun onCreateMenu(menu: Menu, menuInflater: MenuInflater) {
        menuInflater.inflate(R.menu.menu_autocomplete_remove, menu)
    }

    override fun onMenuItemSelected(menuItem: MenuItem): Boolean = when (menuItem.itemId) {
        R.id.remove -> {
            removeSelectedDomains(requireActivity().applicationContext)
            true
        }
        else -> false
    }

    private fun removeSelectedDomains(context: Context) {
        val exceptions = (binding.exceptionList.adapter as DomainListAdapter).selection()
        TrackingProtectionExceptions.selectedItemsRemoved.record(
            TrackingProtectionExceptions.SelectedItemsRemovedExtra(exceptions.size),
        )

        if (exceptions.isNotEmpty()) {
            launch(Main) {
                exceptions.withEach { exception ->
                    context.components.trackingProtectionUseCases.removeException(exception)
                }

                requireComponents.appStore.dispatch(
                    AppAction.NavigateUp(requireComponents.store.state.selectedTabId),
                )
            }
        }
    }

    override fun isSelectionMode() = true

    override fun onResume() {
        super.onResume()

        showToolbar(getString(R.string.preference_autocomplete_title_remove))
    }
}
