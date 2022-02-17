/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentbookmarks.view

import org.mozilla.fenix.home.recentbookmarks.RecentBookmark

/**
 * A menu item in the recent bookmarks dropdown menu.
 *
 * @property title The menu item title.
 * @property onClick Invoked when the user clicks on the menu item.
 */
data class RecentBookmarksMenuItem(
    val title: String,
    val onClick: (RecentBookmark) -> Unit
)
