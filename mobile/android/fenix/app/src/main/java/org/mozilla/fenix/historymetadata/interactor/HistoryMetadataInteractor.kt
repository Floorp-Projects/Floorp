/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.historymetadata.interactor

import org.mozilla.fenix.historymetadata.HistoryMetadataGroup

/**
 * Interface for history metadata related actions in the Home screen.
 */
interface HistoryMetadataInteractor {

    /**
     * Shows the history fragment. Called when a user clicks on the "Show all" button besides the
     * history metadata header.
     */
    fun onHistoryMetadataShowAllClicked()

    /**
     * Navigates to the history metadata group fragment to display the group. Called when a user
     * clicks on a history metadata group.
     *
     * @param historyMetadataGroup The [HistoryMetadataGroup] to toggle its expanded state.
     */
    fun onHistoryMetadataGroupClicked(historyMetadataGroup: HistoryMetadataGroup)

    /**
     * Removes a history metadata group with the given search term from the homescreen.
     *
     * @param searchTerm The search term to be removed.
     */
    fun onRemoveGroup(searchTerm: String)
}
