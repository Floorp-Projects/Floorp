/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.historymetadata.controller

import androidx.navigation.NavController
import mozilla.components.concept.engine.prompt.ShareData
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
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
}

/**
 * The default implementation of [HistoryMetadataGroupController].
 */
class DefaultHistoryMetadataGroupController(
    private val activity: HomeActivity,
    private val store: HistoryMetadataGroupFragmentStore,
    private val navController: NavController,
) : HistoryMetadataGroupController {

    override fun handleOpen(item: History.Metadata) {
        activity.openToBrowserAndLoad(
            searchTermOrURL = item.url,
            newTab = true,
            from = BrowserDirection.FromHistoryMetadataGroup,
            historyMetadata = item.historyMetadataKey
        )
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
}
