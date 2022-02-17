/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.historymetadata.controller

import androidx.navigation.NavController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.browser.state.action.HistoryMetadataAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.feature.tabs.TabsUseCases
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.R
import org.mozilla.fenix.library.history.History
import org.mozilla.fenix.library.historymetadata.HistoryMetadataGroupFragmentAction
import org.mozilla.fenix.library.historymetadata.HistoryMetadataGroupFragmentDirections
import org.mozilla.fenix.library.historymetadata.HistoryMetadataGroupFragmentStore

/**
 * An interface that handles the view manipulation of the history metadata group in the History
 * metadata group screen.
 */
interface HistoryMetadataGroupController {

    /**
     * Opens the given history [item] in a new tab.
     *
     * @param item The [History] to open in a new tab.
     */
    fun handleOpen(item: History.Metadata)

    /**
     * Toggles the given history [item] to be selected in multi-select mode.
     *
     * @param item The [History] to select.
     */
    fun handleSelect(item: History.Metadata)

    /**
     * Toggles the given history [item] to be deselected in multi-select mode.
     *
     * @param item The [History] to deselect.
     */
    fun handleDeselect(item: History.Metadata)

    /**
     * Called on backpressed to deselect all the given [items].
     *
     * @param items The set of [History]s to deselect.
     */
    fun handleBackPressed(items: Set<History.Metadata>): Boolean

    /**
     * Opens the share sheet for a set of history [items].
     *
     * @param items The set of [History]s to share.
     */
    fun handleShare(items: Set<History.Metadata>)

    /**
     * Deletes the given history metadata [items] from storage.
     */
    fun handleDelete(items: Set<History.Metadata>)

    /**
     * Deletes all the history metadata items in this group.
     */
    fun handleDeleteAll()
}

/**
 * The default implementation of [HistoryMetadataGroupController].
 */
class DefaultHistoryMetadataGroupController(
    private val historyStorage: PlacesHistoryStorage,
    private val browserStore: BrowserStore,
    private val store: HistoryMetadataGroupFragmentStore,
    private val selectOrAddUseCase: TabsUseCases.SelectOrAddUseCase,
    private val metrics: MetricController,
    private val navController: NavController,
    private val scope: CoroutineScope,
    private val searchTerm: String,
) : HistoryMetadataGroupController {

    override fun handleOpen(item: History.Metadata) {
        selectOrAddUseCase.invoke(item.url, item.historyMetadataKey)
        navController.navigate(R.id.browserFragment)
        metrics.track(Event.HistorySearchTermGroupOpenTab)
    }

    override fun handleSelect(item: History.Metadata) {
        store.dispatch(HistoryMetadataGroupFragmentAction.Select(item))
    }

    override fun handleDeselect(item: History.Metadata) {
        store.dispatch(HistoryMetadataGroupFragmentAction.Deselect(item))
    }

    override fun handleBackPressed(items: Set<History.Metadata>): Boolean {
        return if (items.isNotEmpty()) {
            store.dispatch(HistoryMetadataGroupFragmentAction.DeselectAll)
            true
        } else {
            false
        }
    }

    override fun handleShare(items: Set<History.Metadata>) {
        navController.navigate(
            HistoryMetadataGroupFragmentDirections.actionGlobalShareFragment(
                data = items.map { ShareData(url = it.url, title = it.title) }.toTypedArray()
            )
        )
    }

    override fun handleDelete(items: Set<History.Metadata>) {
        scope.launch {
            val isDeletingLastItem = items.containsAll(store.state.items)
            items.forEach {
                store.dispatch(HistoryMetadataGroupFragmentAction.Delete(it))
                historyStorage.deleteVisitsFor(it.url)
                metrics.track(Event.HistorySearchTermGroupRemoveTab)
            }
            // The method is called for both single and multiple items.
            // In case all items have been deleted, we have to disband the search group.
            if (isDeletingLastItem) {
                browserStore.dispatch(
                    HistoryMetadataAction.DisbandSearchGroupAction(searchTerm = searchTerm)
                )
            }
        }
    }

    override fun handleDeleteAll() {
        scope.launch {
            store.dispatch(HistoryMetadataGroupFragmentAction.DeleteAll)
            store.state.items.forEach {
                historyStorage.deleteVisitsFor(it.url)
            }
            browserStore.dispatch(
                HistoryMetadataAction.DisbandSearchGroupAction(searchTerm = searchTerm)
            )
            metrics.track(Event.HistorySearchTermGroupRemoveAll)
        }
    }
}
