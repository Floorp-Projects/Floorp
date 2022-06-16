/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.content.Context
import androidx.navigation.NavController
import androidx.navigation.NavOptions
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.browser.state.action.HistoryMetadataAction
import mozilla.components.service.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.R
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.history.DefaultPagedHistoryProvider
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.navigateSafe
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.GleanMetrics.History as GleanHistory

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
    /**
     * Navigates to [org.mozilla.fenix.library.syncedhistory.SyncedHistoryFragment]
     */
    fun handleEnterSyncedHistory()
}

@Suppress("TooManyFunctions", "LongParameterList")
class DefaultHistoryController(
    private val store: HistoryFragmentStore,
    private val appStore: AppStore,
    private var historyProvider: DefaultPagedHistoryProvider,
    private val navController: NavController,
    private val scope: CoroutineScope,
    private val openToBrowser: (item: History.Regular) -> Unit,
    private val displayDeleteAll: () -> Unit,
    private val invalidateOptionsMenu: () -> Unit,
    private val deleteSnackbar: (
        items: Set<History>,
        undo: suspend (Set<History>) -> Unit,
        delete: (Set<History>) -> suspend (context: Context) -> Unit
    ) -> Unit,
    private val syncHistory: suspend () -> Unit,
    private val settings: Settings,
) : HistoryController {

    override fun handleOpen(item: History) {
        when (item) {
            is History.Regular -> openToBrowser(item)
            is History.Group -> {
                GleanHistory.searchTermGroupTapped.record(NoExtras())
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
        val directions = if (settings.showUnifiedSearchFeature) {
            HistoryFragmentDirections.actionGlobalSearchDialog(null)
        } else {
            HistoryFragmentDirections.actionGlobalHistorySearchDialog()
        }

        navController.navigateSafe(R.id.historyFragment, directions)
    }

    override fun handleDeleteAll() {
        displayDeleteAll.invoke()
    }

    override fun handleDeleteSome(items: Set<History>) {
        val pendingDeletionItems = items.map { it.toPendingDeletionHistory() }.toSet()
        appStore.dispatch(AppAction.AddPendingDeletionSet(pendingDeletionItems))
        deleteSnackbar.invoke(items, ::undo, ::delete)
    }

    private fun undo(items: Set<History>) {
        val pendingDeletionItems = items.map { it.toPendingDeletionHistory() }.toSet()
        appStore.dispatch(AppAction.UndoPendingDeletionSet(pendingDeletionItems))
    }

    private fun delete(items: Set<History>): suspend (context: Context) -> Unit {
        return { context ->
            CoroutineScope(Dispatchers.IO).launch {
                store.dispatch(HistoryFragmentAction.EnterDeletionMode)
                for (item in items) {
                    GleanHistory.removed.record(NoExtras())

                    when (item) {
                        is History.Regular -> context.components.core.historyStorage.deleteVisitsFor(item.url)
                        is History.Group -> {
                            // NB: If we have non-search groups, this logic needs to be updated.
                            historyProvider.deleteMetadataSearchGroup(item)
                            context.components.core.store.dispatch(
                                HistoryMetadataAction.DisbandSearchGroupAction(searchTerm = item.title)
                            )
                        }
                        // We won't encounter individual metadata entries outside of groups.
                        is History.Metadata -> {}
                    }
                }
                store.dispatch(HistoryFragmentAction.ExitDeletionMode)
            }
        }
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
        Events.recentlyClosedTabsOpened.record(NoExtras())
    }

    override fun handleEnterSyncedHistory() {
        navController.navigate(
            HistoryFragmentDirections.actionHistoryFragmentToSyncedHistoryFragment()
        )
    }
}
