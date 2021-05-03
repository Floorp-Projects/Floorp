/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import org.mozilla.fenix.browser.browsingmode.BrowsingMode

/**
 * Interactor for the history screen
 * Provides implementations for the HistoryViewInteractor
 */
@SuppressWarnings("TooManyFunctions")
class HistoryInteractor(
    private val historyController: HistoryController
) : HistoryViewInteractor {
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
