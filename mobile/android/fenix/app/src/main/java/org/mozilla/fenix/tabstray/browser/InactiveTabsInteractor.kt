/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import mozilla.components.browser.state.state.TabSessionState

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

class DefaultInactiveTabsInteractor(
    private val controller: InactiveTabsController
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
        controller.close()
    }

    /**
     * See [InactiveTabsAutoCloseDialogInteractor.onEnabledAutoCloseClicked].
     */
    override fun onEnabledAutoCloseClicked() {
        controller.enableAutoClosed()
    }

    /**
     * See [InactiveTabsInteractor.onTabClicked].
     */
    override fun onTabClicked(tab: TabSessionState) {
        controller.openInactiveTab(tab)
    }

    /**
     * See [InactiveTabsInteractor.onTabClosed].
     */
    override fun onTabClosed(tab: TabSessionState) {
        controller.closeInactiveTab(tab)
    }
}
