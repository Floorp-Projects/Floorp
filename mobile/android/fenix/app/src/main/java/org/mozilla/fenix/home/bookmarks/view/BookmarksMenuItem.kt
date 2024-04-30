/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.bookmarks.view

import org.mozilla.fenix.home.bookmarks.Bookmark

/**
 * A menu item in the bookmarks dropdown menu.
 *
 * @property title The menu item title.
 * @property onClick Invoked when the user clicks on the menu item.
 */
data class BookmarksMenuItem(
    val title: String,
    val onClick: (Bookmark) -> Unit,
)
