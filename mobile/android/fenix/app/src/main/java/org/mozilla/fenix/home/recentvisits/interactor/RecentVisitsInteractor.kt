/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentvisits.interactor

import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem.RecentHistoryHighlight

/**
 * All possible user interactions with the "Recent visits" section.
 */
interface RecentVisitsInteractor {

    /**
     * Callback for when the user clicks on the "Show all" button besides the recent visits header.
     */
    fun onHistoryShowAllClicked()

    /**
     * Callback for when the user clicks on a [RecentHistoryHighlight].
     *
     * @param recentHistoryHighlight The just clicked [RecentHistoryHighlight].
     */
    fun onRecentHistoryHighlightClicked(recentHistoryHighlight: RecentHistoryHighlight)

    /**
     * Callback for when the user selected an option to remove a [RecentHistoryHighlight].
     *
     * @param highlightUrl [RecentHistoryHighlight.url] of the item to remove.
     */
    fun onRemoveRecentHistoryHighlight(highlightUrl: String)
}
