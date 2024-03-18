/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history.state

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.HistoryMetadataAction
import mozilla.components.browser.state.action.RecentlyClosedAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.history.DefaultPagedHistoryProvider
import org.mozilla.fenix.library.history.History
import org.mozilla.fenix.library.history.HistoryFragmentAction
import org.mozilla.fenix.library.history.HistoryFragmentState
import org.mozilla.fenix.library.history.HistoryFragmentStore
import org.mozilla.fenix.library.history.toPendingDeletionHistory

/**
 * A [Middleware] for initiating storage side-effects based on [HistoryFragmentAction]s that are
 * dispatched to the [HistoryFragmentStore].
 *
 * @param appStore To dispatch Actions to update global state.
 * @param browserStore To dispatch Actions to update global state.
 * @param historyProvider To update storage as a result of some Actions.
 * @param historyStorage To update storage as a result of some Actions.
 * @param undoDeleteSnackbar Called when items are deleted to offer opportunity of undoing before
 *  deletion is fully completed.
 * @param onTimeFrameDeleted Called when a time range of items is successfully deleted.
 * @param scope CoroutineScope to launch storage operations into.
 */
class HistoryStorageMiddleware(
    private val appStore: AppStore,
    private val browserStore: BrowserStore,
    private val historyProvider: DefaultPagedHistoryProvider,
    private val historyStorage: PlacesHistoryStorage,
    private val undoDeleteSnackbar: (
        items: Set<History>,
        undo: suspend (Set<History>) -> Unit,
        delete: suspend (Set<History>) -> Unit,
    ) -> Unit,
    private val onTimeFrameDeleted: () -> Unit,
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.IO),
) : Middleware<HistoryFragmentState, HistoryFragmentAction> {
    override fun invoke(
        context: MiddlewareContext<HistoryFragmentState, HistoryFragmentAction>,
        next: (HistoryFragmentAction) -> Unit,
        action: HistoryFragmentAction,
    ) {
        next(action)
        when (action) {
            is HistoryFragmentAction.DeleteItems -> {
                appStore.dispatch(AppAction.AddPendingDeletionSet(action.items.toPendingDeletionHistory()))
                undoDeleteSnackbar(action.items, ::undo, delete(context.store))
            }
            is HistoryFragmentAction.DeleteTimeRange -> {
                context.store.dispatch(HistoryFragmentAction.EnterDeletionMode)
                scope.launch {
                    if (action.timeFrame == null) {
                        historyStorage.deleteEverything()
                    } else {
                        val longRange = action.timeFrame.toLongRange()
                        historyStorage.deleteVisitsBetween(
                            startTime = longRange.first,
                            endTime = longRange.last,
                        )
                    }
                    browserStore.dispatch(RecentlyClosedAction.RemoveAllClosedTabAction)
                    browserStore.dispatch(EngineAction.PurgeHistoryAction).join()

                    context.store.dispatch(HistoryFragmentAction.ExitDeletionMode)
                    launch(Dispatchers.Main) {
                        onTimeFrameDeleted()
                    }
                }
            }
            else -> Unit
        }
    }

    private fun undo(items: Set<History>) {
        val pendingDeletionItems = items.map { it.toPendingDeletionHistory() }.toSet()
        appStore.dispatch(AppAction.UndoPendingDeletionSet(pendingDeletionItems))
    }

    private fun delete(store: Store<HistoryFragmentState, HistoryFragmentAction>): suspend (Set<History>) -> Unit {
        return { items ->
            scope.launch {
                store.dispatch(HistoryFragmentAction.EnterDeletionMode)
                for (item in items) {
                    when (item) {
                        is History.Regular -> historyStorage.deleteVisitsFor(item.url)
                        is History.Group -> {
                            // NB: If we have non-search groups, this logic needs to be updated.
                            historyProvider.deleteMetadataSearchGroup(item)
                            browserStore.dispatch(
                                HistoryMetadataAction.DisbandSearchGroupAction(searchTerm = item.title),
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
}
