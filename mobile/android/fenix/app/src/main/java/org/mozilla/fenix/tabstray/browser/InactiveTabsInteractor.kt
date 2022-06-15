/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import mozilla.components.browser.state.state.TabSessionState
import org.mozilla.fenix.tabstray.TrayPagerAdapter

/**
 * Interactor for all things related to inactive tabs in the tabs tray.
 */
interface InactiveTabsInteractor : InactiveTabsAutoCloseDialogInteractor {
    /**
     * Invoked when the header is clicked.
     *
     * @param activated true when the tap should expand the inactive section.
     */
    fun onHeaderClicked(activated: Boolean)

    /**
     * Invoked when an inactive tab is clicked.
     *
     * @param tab [TabSessionState] that was clicked.
     */
    fun onTabClicked(tab: TabSessionState)

    /**
     * Invoked when an inactive tab is closed.
     *
     * @param tab [TabSessionState] that was closed.
     */
    fun onTabClosed(tab: TabSessionState)

    /**
     * Invoked when the user clicks on the delete all inactive tabs button.
     */
    fun onDeleteAllInactiveTabsClicked()
}

/**
 * Interactor for the auto-close dialog in the inactive tabs section.
 */
interface InactiveTabsAutoCloseDialogInteractor {

    /**
     * Invoked when the close button is clicked.
     */
    fun onCloseClicked()

    /**
     * Invoked when the dialog is clicked.
     */
    fun onEnabledAutoCloseClicked()
}

/**
 * Interactor to be called for any user interactions with the Inactive Tabs feature.
 *
 * @param controller [InactiveTabsController] todo.
 * @param browserInteractor [BrowserTrayInteractor] used to respond to interactions with specific inactive tabs.
 */
class DefaultInactiveTabsInteractor(
    private val controller: InactiveTabsController,
    private val browserInteractor: BrowserTrayInteractor,
) : InactiveTabsInteractor {

    /**
     * See [InactiveTabsInteractor.onHeaderClicked].
     */
    override fun onHeaderClicked(activated: Boolean) {
        controller.updateCardExpansion(activated)
    }

    /**
     * See [InactiveTabsAutoCloseDialogInteractor.onCloseClicked].
     */
    override fun onCloseClicked() {
        controller.dismissAutoCloseDialog()
    }

    /**
     * See [InactiveTabsAutoCloseDialogInteractor.onEnabledAutoCloseClicked].
     */
    override fun onEnabledAutoCloseClicked() {
        controller.enableInactiveTabsAutoClose()
    }

    /**
     * See [InactiveTabsInteractor.onTabClicked].
     */
    override fun onTabClicked(tab: TabSessionState) {
        controller.openInactiveTab(tab)
        browserInteractor.onTabSelected(tab, TrayPagerAdapter.INACTIVE_TABS_FEATURE_NAME)
    }

    /**
     * See [InactiveTabsInteractor.onTabClosed].
     */
    override fun onTabClosed(tab: TabSessionState) {
        controller.closeInactiveTab(tab)
        browserInteractor.onTabClosed(tab, TrayPagerAdapter.INACTIVE_TABS_FEATURE_NAME)
    }

    /**
     * See [InactiveTabsInteractor.onDeleteAllInactiveTabsClicked].
     */
    override fun onDeleteAllInactiveTabsClicked() {
        controller.deleteAllInactiveTabs()
    }
}
