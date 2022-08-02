/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentvisits

/**
 * History items of previously accessed webpages.
 */
sealed class RecentlyVisitedItem {
    /**
     * A history highlight - previously accessed webpage of particular importance.
     *
     * @param title The title of the webpage. May be [url] if the title is unavailable.
     * @param url The URL of the webpage.
     */
    data class RecentHistoryHighlight(
        val title: String,
        val url: String
    ) : RecentlyVisitedItem()
}
