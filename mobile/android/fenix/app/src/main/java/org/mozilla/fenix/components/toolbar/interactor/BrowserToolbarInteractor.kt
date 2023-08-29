/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar.interactor

import mozilla.components.ui.tabcounter.TabCounterMenu
import org.mozilla.fenix.components.toolbar.BrowserToolbarController
import org.mozilla.fenix.components.toolbar.BrowserToolbarMenuController
import org.mozilla.fenix.components.toolbar.ToolbarMenu

/**
 * Interface for the browser toolbar interactor. This interface is implemented by objects that
 * want to respond to user interaction on the browser toolbar.
 */
interface BrowserToolbarInteractor {
    fun onBrowserToolbarPaste(text: String)
    fun onBrowserToolbarPasteAndGo(text: String)
    fun onBrowserToolbarClicked()
    fun onBrowserToolbarMenuItemTapped(item: ToolbarMenu.Item)
    fun onTabCounterClicked()
    fun onTabCounterMenuItemTapped(item: TabCounterMenu.Item)
    fun onScrolled(offset: Int)
    fun onReaderModePressed(enabled: Boolean)

    /**
     * Navigates to the Home screen. Called when a user taps on the Home button.
     */
    fun onHomeButtonClicked()

    /**
     * Deletes all tabs and navigates to the Home screen. Called when a user taps on the erase button.
     */
    fun onEraseButtonClicked()
}

/**
 * The default implementation of [BrowserToolbarInteractor].
 *
 * @property browserToolbarController [BrowserToolbarController] to which user actions can be
 * delegated for all interactions on the browser toolbar.
 * @property menuController [BrowserToolbarMenuController] to which user actions can be delegated
 * for all interactions on the the browser toolbar menu.
 */
class DefaultBrowserToolbarInteractor(
    private val browserToolbarController: BrowserToolbarController,
    private val menuController: BrowserToolbarMenuController,
) : BrowserToolbarInteractor {

    override fun onTabCounterClicked() {
        browserToolbarController.handleTabCounterClick()
    }

    override fun onTabCounterMenuItemTapped(item: TabCounterMenu.Item) {
        browserToolbarController.handleTabCounterItemInteraction(item)
    }

    override fun onBrowserToolbarPaste(text: String) {
        browserToolbarController.handleToolbarPaste(text)
    }

    override fun onBrowserToolbarPasteAndGo(text: String) {
        browserToolbarController.handleToolbarPasteAndGo(text)
    }

    override fun onBrowserToolbarClicked() {
        browserToolbarController.handleToolbarClick()
    }

    override fun onBrowserToolbarMenuItemTapped(item: ToolbarMenu.Item) {
        menuController.handleToolbarItemInteraction(item)
    }

    override fun onScrolled(offset: Int) {
        browserToolbarController.handleScroll(offset)
    }

    override fun onReaderModePressed(enabled: Boolean) {
        browserToolbarController.handleReaderModePressed(enabled)
    }

    override fun onHomeButtonClicked() {
        browserToolbarController.handleHomeButtonClick()
    }

    override fun onEraseButtonClicked() {
        browserToolbarController.handleEraseButtonClick()
    }
}
