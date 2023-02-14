/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.recentlyclosed

import mozilla.components.browser.state.state.recover.TabState

/**
 * Interactor for the recently closed screen
 * Provides implementations for the RecentlyClosedInteractor
 */
class RecentlyClosedFragmentInteractor(
    private val recentlyClosedController: RecentlyClosedController,
) : RecentlyClosedInteractor {

    override fun onDelete(tab: TabState) {
        recentlyClosedController.handleDelete(tab)
    }

    override fun onNavigateToHistory() {
        recentlyClosedController.handleNavigateToHistory()
    }

    override fun open(item: TabState) {
        recentlyClosedController.handleRestore(item)
    }

    override fun select(item: TabState) {
        recentlyClosedController.handleSelect(item)
    }

    override fun deselect(item: TabState) {
        recentlyClosedController.handleDeselect(item)
    }
}
