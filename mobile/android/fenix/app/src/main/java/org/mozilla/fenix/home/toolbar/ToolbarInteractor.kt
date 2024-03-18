/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.toolbar

/**
 * Interface for toolbar related actions.
 */
interface ToolbarInteractor {
    /**
     * Navigates to browser with clipboard text.
     *
     * @param clipboardText The current text content of the clipboard.
     */
    fun onPasteAndGo(clipboardText: String)

    /**
     * Navigates to search with clipboard text.
     *
     * @param clipboardText The current text content of the clipboard.
     */
    fun onPaste(clipboardText: String)

    /**
     * Navigates to the search dialog.
     */
    fun onNavigateSearch()
}
