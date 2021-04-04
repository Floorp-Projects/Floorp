/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.recentlyclosed

import mozilla.components.browser.state.state.recover.RecoverableTab
import org.mozilla.fenix.browser.browsingmode.BrowsingMode

/**
 * Interactor for the recently closed screen
 * Provides implementations for the RecentlyClosedInteractor
 */
class RecentlyClosedFragmentInteractor(
    private val recentlyClosedController: RecentlyClosedController
) : RecentlyClosedInteractor {
    override fun restore(item: RecoverableTab) {
        recentlyClosedController.handleRestore(item)
    }

    override fun onCopyPressed(item: RecoverableTab) {
        recentlyClosedController.handleCopyUrl(item)
    }

    override fun onSharePressed(item: RecoverableTab) {
        recentlyClosedController.handleShare(item)
    }

    override fun onOpenInNormalTab(item: RecoverableTab) {
        recentlyClosedController.handleOpen(item, BrowsingMode.Normal)
    }

    override fun onOpenInPrivateTab(item: RecoverableTab) {
        recentlyClosedController.handleOpen(item, BrowsingMode.Private)
    }

    override fun onDelete(tab: RecoverableTab) {
        recentlyClosedController.handleDelete(tab)
    }

    override fun onNavigateToHistory() {
        recentlyClosedController.handleNavigateToHistory()
    }

    override fun open(item: RecoverableTab) {
        recentlyClosedController.handleRestore(item)
    }

    override fun select(item: RecoverableTab) {
        recentlyClosedController.handleSelect(item)
    }

    override fun deselect(item: RecoverableTab) {
        recentlyClosedController.handleDeselect(item)
    }
}
