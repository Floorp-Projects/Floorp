/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentvisits

import mozilla.components.concept.storage.HistoryMetadata
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem.RecentHistoryGroup

/**
 * History items as individual or groups of previously accessed webpages.
 */
sealed class RecentlyVisitedItem {
    /**
     * A history highlight - previously accessed webpage of particular importance.
     *
     * @property title The title of the webpage. May be [url] if the title is unavailable.
     * @property url The URL of the webpage.
     */
    data class RecentHistoryHighlight(
        val title: String,
        val url: String,
    ) : RecentlyVisitedItem()

    /**
     * A group of previously accessed webpages related by their search terms.
     *
     * @property title The title of the group.
     * @property historyMetadata A list of [HistoryMetadata] records that matches the title.
     */
    data class RecentHistoryGroup(
        val title: String,
        val historyMetadata: List<HistoryMetadata> = emptyList(),
    ) : RecentlyVisitedItem()
}

// The last updated time of the group is based on the most recently updated item in the group
fun RecentHistoryGroup.lastUpdated(): Long = historyMetadata.maxOf { it.updatedAt }
