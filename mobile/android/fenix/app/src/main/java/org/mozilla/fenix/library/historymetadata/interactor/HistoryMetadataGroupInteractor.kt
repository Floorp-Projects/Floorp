/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.historymetadata.interactor

import org.mozilla.fenix.library.history.History
import org.mozilla.fenix.library.historymetadata.controller.HistoryMetadataGroupController
import org.mozilla.fenix.selection.SelectionInteractor

/**
 * Interface for history metadata group related actions in the History view.
 */
interface HistoryMetadataGroupInteractor : SelectionInteractor<History.Metadata> {

    /**
     * Called on backpressed to deselect all the given [items].
     *
     * @param items The set of [History]s to deselect.
     */
    fun onBackPressed(items: Set<History.Metadata>): Boolean

    /**
     * Deletes the given set of history [items] that are selected. Called when a user clicks on the
     * "Delete" menu item.
     *
     * @param items The set of [History]s to delete.
     */
    fun onDeleteMenuItem(items: Set<History.Metadata>)

    /**
     * Deletes the all the history items in the history metadata group. Called when a user clicks
     * on the "Delete history" menu item.
     */
    fun onDeleteAllMenuItem()

    /**
     * Opens the share sheet for a set of history [items]. Called when a user clicks on the
     * "Share" menu item.
     *
     * @param items The set of [History]s to share.
     */
    fun onShareMenuItem(items: Set<History.Metadata>)
}

/**
 * The default implementation of [HistoryMetadataGroupInteractor].
 */
class DefaultHistoryMetadataGroupInteractor(
    private val controller: HistoryMetadataGroupController
) : HistoryMetadataGroupInteractor {

    override fun open(item: History.Metadata) {
        controller.handleOpen(item)
    }

    override fun select(item: History.Metadata) {
        controller.handleSelect(item)
    }

    override fun deselect(item: History.Metadata) {
        controller.handleDeselect(item)
    }

    override fun onBackPressed(items: Set<History.Metadata>): Boolean {
        return controller.handleBackPressed(items)
    }

    override fun onDeleteMenuItem(items: Set<History.Metadata>) {
        // no-op
    }

    override fun onDeleteAllMenuItem() {
        // no-op
    }

    override fun onShareMenuItem(items: Set<History.Metadata>) {
        controller.handleShare(items)
    }
}
