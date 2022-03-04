/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity

/**
 * An interface that handles the view manipulation of the Bookmark Search, triggered by the Interactor
 */
interface BookmarkSearchController {
    fun handleEditingCancelled()
    fun handleTextChanged(text: String)
    fun handleUrlTapped(url: String, flags: LoadUrlFlags = LoadUrlFlags.none())
}

class BookmarkSearchDialogController(
    private val activity: HomeActivity,
    private val fragmentStore: BookmarkSearchFragmentStore,
    private val clearToolbarFocus: () -> Unit,
) : BookmarkSearchController {

    override fun handleEditingCancelled() {
        clearToolbarFocus()
    }

    override fun handleTextChanged(text: String) {
        fragmentStore.dispatch(BookmarkSearchFragmentAction.UpdateQuery(text))
    }

    override fun handleUrlTapped(url: String, flags: LoadUrlFlags) {
        clearToolbarFocus()

        activity.openToBrowserAndLoad(
            searchTermOrURL = url,
            newTab = true,
            from = BrowserDirection.FromBookmarkSearchDialog,
            flags = flags
        )
    }
}
