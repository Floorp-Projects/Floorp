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
     * Deletes the given set of history metadata [items]. Called when a user clicks on the
     * "Delete" menu item or the "x" button associated with a history metadata item.
     *
     * @param items The set of [History]s to delete.
     */
    fun onDelete(items: Set<History.Metadata>)

    /**
     * Called when a user clicks on the "Delete history" menu item.
     */
    fun onDeleteAll()

    /**
     * Called when a user has confirmed the deletion of the group.
     */
    fun onDeleteAllConfirmed()

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
    private val controller: HistoryMetadataGroupController,
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

    override fun onDelete(items: Set<History.Metadata>) {
        controller.handleDelete(items)
    }

    override fun onDeleteAll() {
        controller.handleDeleteAll()
    }

    override fun onDeleteAllConfirmed() {
        controller.handleDeleteAllConfirmed()
    }

    override fun onShareMenuItem(items: Set<History.Metadata>) {
        controller.handleShare(items)
    }
}
