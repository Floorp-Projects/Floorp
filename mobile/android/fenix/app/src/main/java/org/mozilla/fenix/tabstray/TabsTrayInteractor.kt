/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import mozilla.components.browser.state.state.TabSessionState

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
     * @param source app feature from which the [TabSessionState] with [tabId] was closed.
     */
    fun onDeleteTab(tabId: String, source: String? = null)

    /**
     * Invoked when the user confirmed tab removal that would lead to cancelled private downloads.
     * @param source is the app feature from which the [TabSessionState] with [tabId] was closed.
     */
    fun onDeletePrivateTabWarningAccepted(tabId: String, source: String? = null)

    /**
     * Invoked when [TabSessionState]s need to be deleted.
     */
    fun onDeleteTabs(tabs: Collection<TabSessionState>)

    /**
     * Called when clicking the debug menu option for inactive tabs.
     */
    fun onInactiveDebugClicked(tabs: Collection<TabSessionState>)

    /**
     * Invoked when [tabId] should be moved to before/after [targetId] from a drag-drop operation
     */
    fun onTabsMove(
        tabId: String,
        targetId: String?,
        placeAfter: Boolean
    )
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

    override fun onDeleteTab(tabId: String, source: String?) {
        controller.handleTabDeletion(tabId, source)
    }

    override fun onDeletePrivateTabWarningAccepted(tabId: String, source: String?) {
        controller.handleDeleteTabWarningAccepted(tabId, source)
    }

    override fun onDeleteTabs(tabs: Collection<TabSessionState>) {
        controller.handleMultipleTabsDeletion(tabs)
    }

    override fun onTabsMove(
        tabId: String,
        targetId: String?,
        placeAfter: Boolean
    ) {
        controller.handleTabsMove(tabId, targetId, placeAfter)
    }

    override fun onInactiveDebugClicked(tabs: Collection<TabSessionState>) {
        controller.forceTabsAsInactive(tabs)
    }
}
