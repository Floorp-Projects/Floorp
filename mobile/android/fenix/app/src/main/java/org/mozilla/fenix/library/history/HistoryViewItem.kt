/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.os.Parcelable
import kotlinx.parcelize.Parcelize

/**
 * The class represents the items types used by [HistoryAdapter] to populate the list.
 * It contains the data for viewHolders. Subclasses match the variety of viewHolders.
 */
sealed class HistoryViewItem : Parcelable {

    /**
     * A class representing a regular history record in the history and synced history lists.
     *
     * @param data history item that will be displayed.
     * @param collapsed state flag to support collapsed header feature; collapsed items will be
     * filtered out from the list of displayed items.
     */
    @Parcelize
    data class HistoryItem(
        val data: History.Regular,
        val collapsed: Boolean = false
    ) : HistoryViewItem()

    /**
     * A class representing a search group (a group of history items) in the history list.
     *
     * @param data History group item that will be displayed.
     * @param collapsed State flag to support collapsed header feature; collapsed items will be
     * filtered out from the list of displayed items.
     */
    @Parcelize
    data class HistoryGroupItem(
        val data: History.Group,
        val collapsed: Boolean = false
    ) : HistoryViewItem()

    /**
     * A class representing a header in the history and synced history lists.
     *
     * @param title inside a time group header.
     * @param timeGroup A time group associated with a Header.
     * @param collapsed state flag to support collapsed header feature; collapsed items will be
     * filtered out from the list of displayed items.
     */
    @Parcelize
    data class TimeGroupHeader(
        val title: String,
        val timeGroup: HistoryItemTimeGroup,
        val collapsed: Boolean = false
    ) : HistoryViewItem()

    /**
     * A class representing a recently closed button in the history list.
     *
     * @param title of a recently closed button inside History screen.
     * @param body of a recently closed button inside History screen.
     */
    @Parcelize
    data class RecentlyClosedItem(
        val title: String,
        val body: String
    ) : HistoryViewItem()

    /**
     * A class representing a synced history button in the history list.
     *
     * @param title of a recently closed button inside History screen.
     */
    @Parcelize
    data class SyncedHistoryItem(
        val title: String
    ) : HistoryViewItem()

    /**
     * A class representing empty state in history and synced history screens.
     *
     * @param emptyMessage of an emptyView inside History screen.
     */
    @Parcelize
    data class EmptyHistoryItem(
        val emptyMessage: String
    ) : HistoryViewItem()

    /**
     * A class representing a sign-in window inside the synced history screen.
     */
    @Parcelize
    object SignInHistoryItem : HistoryViewItem()

    /**
     * A class representing an extra space that header items have above them when they are
     * not in a collapsed state.
     *
     * @param timeGroup A time group associated with a separator; separator relates to the time group
     * of items above, not below. In case the time group is collapsed, it should be hidden with its
     * time group as well, so collapsed groups wouldn't have extra spacing in between.
     */
    @Parcelize
    data class TimeGroupSeparatorHistoryItem(
        val timeGroup: HistoryItemTimeGroup?
    ) : HistoryViewItem()

    /**
     * A class representing a space at the top of history and synced history lists.
     */
    @Parcelize
    object TopSeparatorHistoryItem : HistoryViewItem()
}
