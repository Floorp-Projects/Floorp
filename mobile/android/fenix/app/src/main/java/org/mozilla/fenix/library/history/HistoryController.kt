/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import androidx.navigation.NavController
import androidx.navigation.NavOptions
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.navigateSafe

@Suppress("TooManyFunctions")
interface HistoryController {
    fun handleOpen(item: History)
    fun handleSelect(item: History)
    fun handleDeselect(item: History)
    fun handleBackPressed(): Boolean
    fun handleModeSwitched()
    fun handleSearch()
    fun handleDeleteAll()
    fun handleDeleteSome(items: Set<History>)
    fun handleRequestSync()
    fun handleEnterRecentlyClosed()
}

@Suppress("TooManyFunctions")
class DefaultHistoryController(
    private val store: HistoryFragmentStore,
    private val navController: NavController,
    private val scope: CoroutineScope,
    private val openToBrowser: (item: History.Regular) -> Unit,
    private val displayDeleteAll: () -> Unit,
    private val invalidateOptionsMenu: () -> Unit,
    private val deleteHistoryItems: (Set<History>) -> Unit,
    private val syncHistory: suspend () -> Unit,
    private val metrics: MetricController
) : HistoryController {

    override fun handleOpen(item: History) {
        when (item) {
            is History.Regular -> openToBrowser(item)
            is History.Group -> {
                metrics.track(Event.HistorySearchTermGroupTapped)
                navController.navigate(
                    HistoryFragmentDirections.actionGlobalHistoryMetadataGroup(
                        title = item.title,
                        historyMetadataItems = item.items.toTypedArray()
                    ),
                    NavOptions.Builder().setPopUpTo(R.id.historyMetadataGroupFragment, true).build()
                )
            }
            else -> { /* noop */ }
        }
    }

    override fun handleSelect(item: History) {
        if (store.state.mode === HistoryFragmentState.Mode.Syncing) {
            return
        }

        store.dispatch(HistoryFragmentAction.AddItemForRemoval(item))
    }

    override fun handleDeselect(item: History) {
        store.dispatch(HistoryFragmentAction.RemoveItemForRemoval(item))
    }

    override fun handleBackPressed(): Boolean {
        return if (store.state.mode is HistoryFragmentState.Mode.Editing) {
            store.dispatch(HistoryFragmentAction.ExitEditMode)
            true
        } else {
            false
        }
    }

    override fun handleModeSwitched() {
        invalidateOptionsMenu.invoke()
    }

    override fun handleSearch() {
        val directions =
            HistoryFragmentDirections.actionGlobalHistorySearchDialog()
        navController.navigateSafe(R.id.historyFragment, directions)
    }

    override fun handleDeleteAll() {
        displayDeleteAll.invoke()
    }

    override fun handleDeleteSome(items: Set<History>) {
        deleteHistoryItems.invoke(items)
    }

    override fun handleRequestSync() {
        scope.launch {
            store.dispatch(HistoryFragmentAction.StartSync)
            syncHistory.invoke()
            store.dispatch(HistoryFragmentAction.FinishSync)
        }
    }

    override fun handleEnterRecentlyClosed() {
        navController.navigate(
            HistoryFragmentDirections.actionGlobalRecentlyClosed(),
            NavOptions.Builder().setPopUpTo(R.id.recentlyClosedFragment, true).build()
        )
        metrics.track(Event.RecentlyClosedTabsOpenedOld)
    }
}
