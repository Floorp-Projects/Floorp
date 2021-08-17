/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.selection.SelectionInteractor

/**
 * Interface for the HistoryInteractor. This interface is implemented by objects that want
 * to respond to user interaction on the HistoryView
 */
interface HistoryInteractor : SelectionInteractor<HistoryItem> {

    /**
     * Called on backpressed to exit edit mode
     */
    fun onBackPressed(): Boolean

    /**
     * Called when the mode is switched so we can invalidate the menu
     */
    fun onModeSwitched()

    /**
     * Copies the URL of a history item to the copy-paste buffer.
     *
     * @param item the history item to copy the URL from
     */
    fun onCopyPressed(item: HistoryItem)

    /**
     * Opens the share sheet for a history item.
     *
     * @param item the history item to share
     */
    fun onSharePressed(item: HistoryItem)

    /**
     * Opens a history item in a new tab.
     *
     * @param item the history item to open in a new tab
     */
    fun onOpenInNormalTab(item: HistoryItem)

    /**
     * Opens a history item in a private tab.
     *
     * @param item the history item to open in a private tab
     */
    fun onOpenInPrivateTab(item: HistoryItem)

    /**
     * Called when delete all is tapped
     */
    fun onDeleteAll()

    /**
     * Called when multiple history items are deleted
     * @param items the history items to delete
     */
    fun onDeleteSome(items: Set<HistoryItem>)

    /**
     * Called when the user requests a sync of the history
     */
    fun onRequestSync()

    /**
     * Called when the user clicks on recently closed tab button.
     */
    fun onRecentlyClosedClicked()
}

/**
 * Interactor for the history screen
 * Provides implementations for the HistoryInteractor
 */
@SuppressWarnings("TooManyFunctions")
class DefaultHistoryInteractor(
    private val historyController: HistoryController
) : HistoryInteractor {
    override fun open(item: HistoryItem) {
        historyController.handleOpen(item)
    }

    override fun select(item: HistoryItem) {
        historyController.handleSelect(item)
    }

    override fun deselect(item: HistoryItem) {
        historyController.handleDeselect(item)
    }

    override fun onBackPressed(): Boolean {
        return historyController.handleBackPressed()
    }

    override fun onModeSwitched() {
        historyController.handleModeSwitched()
    }

    override fun onCopyPressed(item: HistoryItem) {
        historyController.handleCopyUrl(item)
    }

    override fun onSharePressed(item: HistoryItem) {
        historyController.handleShare(item)
    }

    override fun onOpenInNormalTab(item: HistoryItem) {
        historyController.handleOpenInNewTab(item, BrowsingMode.Normal)
    }

    override fun onOpenInPrivateTab(item: HistoryItem) {
        historyController.handleOpenInNewTab(item, BrowsingMode.Private)
    }

    override fun onDeleteAll() {
        historyController.handleDeleteAll()
    }

    override fun onDeleteSome(items: Set<HistoryItem>) {
        historyController.handleDeleteSome(items)
    }

    override fun onRequestSync() {
        historyController.handleRequestSync()
    }

    override fun onRecentlyClosedClicked() {
        historyController.handleEnterRecentlyClosed()
    }
}
