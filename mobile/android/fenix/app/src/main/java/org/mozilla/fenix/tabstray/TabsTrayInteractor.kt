/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import mozilla.components.concept.tabstray.Tab

interface TabsTrayInteractor {
    /**
     * Set the current tray item to the clamped [position].
     *
     * @param position The position on the tray to focus.
     * @param smoothScroll If true, animate the scrolling from the current tab to [position].
     */
    fun onTrayPositionSelected(position: Int, smoothScroll: Boolean)

    /**
     * Dismisses the tabs tray and navigates to the browser.
     */
    fun onBrowserTabSelected()

    /**
     * Invoked when a tab is removed from the tabs tray with the given [tabId].
     */
    fun onDeleteTab(tabId: String)

    /**
     * Invoked when [Tab]s need to be deleted.
     */
    fun onDeleteTabs(tabs: Collection<Tab>)
}

/**
 * Interactor to be called for any tabs tray user actions.
 *
 * @property controller [TabsTrayController] to which user actions can be delegated for actual app update.
 */
class DefaultTabsTrayInteractor(
    private val controller: TabsTrayController
) : TabsTrayInteractor {
    override fun onTrayPositionSelected(position: Int, smoothScroll: Boolean) {
        controller.handleTrayScrollingToPosition(position, smoothScroll)
    }

    override fun onBrowserTabSelected() {
        controller.handleNavigateToBrowser()
    }

    override fun onDeleteTab(tabId: String) {
        controller.handleTabDeletion(tabId)
    }

    override fun onDeleteTabs(tabs: Collection<Tab>) {
        controller.handleMultipleTabsDeletion(tabs)
    }
}
