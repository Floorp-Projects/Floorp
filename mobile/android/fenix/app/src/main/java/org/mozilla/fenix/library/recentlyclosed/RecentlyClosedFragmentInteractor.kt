/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.recentlyclosed

import mozilla.components.browser.state.state.ClosedTab
import org.mozilla.fenix.browser.browsingmode.BrowsingMode

/**
 * Interactor for the recently closed screen
 * Provides implementations for the RecentlyClosedInteractor
 */
class RecentlyClosedFragmentInteractor(
    private val recentlyClosedController: RecentlyClosedController
) : RecentlyClosedInteractor {
    override fun restore(item: ClosedTab) {
        recentlyClosedController.handleRestore(item)
    }

    override fun onCopyPressed(item: ClosedTab) {
        recentlyClosedController.handleCopyUrl(item)
    }

    override fun onSharePressed(item: ClosedTab) {
        recentlyClosedController.handleShare(item)
    }

    override fun onOpenInNormalTab(item: ClosedTab) {
        recentlyClosedController.handleOpen(item, BrowsingMode.Normal)
    }

    override fun onOpenInPrivateTab(item: ClosedTab) {
        recentlyClosedController.handleOpen(item, BrowsingMode.Private)
    }

    override fun onDeleteOne(tab: ClosedTab) {
        recentlyClosedController.handleDeleteOne(tab)
    }

    override fun onNavigateToHistory() {
        recentlyClosedController.handleNavigateToHistory()
    }
}
