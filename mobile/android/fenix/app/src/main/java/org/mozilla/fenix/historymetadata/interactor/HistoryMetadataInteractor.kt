/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.historymetadata.interactor

import mozilla.components.concept.storage.HistoryMetadataKey
import org.mozilla.fenix.historymetadata.HistoryMetadataGroup

/**
 * Interface for history metadata related actions in the Home screen.
 */
interface HistoryMetadataInteractor {

    /**
     * Selects an existing tab with the matching [HistoryMetadataKey] or adds a new tab with the
     * given [url]. Called when a user clicks on a history metadata item.
     *
     * @param url The URL to open.
     * @param historyMetadata The [HistoryMetadataKey] to match for an existing tab.
     */
    fun onHistoryMetadataItemClicked(url: String, historyMetadata: HistoryMetadataKey)

    /**
     * Shows the history fragment. Called when a user clicks on the "Show all" button besides the
     * history metadata header.
     */
    fun onHistoryMetadataShowAllClicked()

    /**
     * Toggles whether or not a history metadata group is expanded. Called when a user clicks on
     * a history metadata group.
     *
     * @param historyMetadataGroup The [HistoryMetadataGroup] to toggle its expanded state.
     */
    fun onToggleHistoryMetadataGroupExpanded(historyMetadataGroup: HistoryMetadataGroup)
}
