/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import org.mozilla.fenix.library.bookmarks.awesomebar.AwesomeBarInteractor
import org.mozilla.fenix.library.bookmarks.toolbar.ToolbarInteractor

/**
 * Interactor for the bookmark search
 * Provides implementations for the AwesomeBarView and ToolbarView
 */
class BookmarkSearchDialogInteractor(
    private val bookmarkSearchController: BookmarkSearchDialogController
) : AwesomeBarInteractor, ToolbarInteractor {

    override fun onEditingCanceled() {
        bookmarkSearchController.handleEditingCancelled()
    }

    override fun onTextChanged(text: String) {
        bookmarkSearchController.handleTextChanged(text)
    }

    override fun onUrlTapped(url: String, flags: LoadUrlFlags) {
        bookmarkSearchController.handleUrlTapped(url, flags)
    }
}
