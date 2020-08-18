/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

import mozilla.components.concept.tabstray.Tab
import mozilla.components.browser.storage.sync.Tab as SyncTab

@Suppress("TooManyFunctions")
interface TabTrayInteractor {
    /**
     * Called when user clicks the new tab button.
     */
    fun onNewTabTapped(private: Boolean)

    /**
     * Called when tab tray should be dismissed.
     */
    fun onTabTrayDismissed()

    /**
     * Called when user clicks the share tabs button.
     */
    fun onShareTabsClicked(private: Boolean)

    /**
     * Called when user clicks button to save selected tabs to a collection.
     */
    fun onSaveToCollectionClicked(selectedTabs: Set<Tab>)

    /**
     * Called when user clicks the close all tabs button.
     */
    fun onCloseAllTabsClicked(private: Boolean)

    /**
     * Called when the user clicks on a synced tab entry.
     */
    fun onSyncedTabClicked(syncTab: SyncTab)

    /**
     * Called when the physical back button is clicked.
     */
    fun onBackPressed(): Boolean

    /**
     * Called when a requester needs to know the current mode of the tab tray.
     */
    fun onModeRequested(): TabTrayDialogFragmentState.Mode

    /**
     * Called when a tab should be opened in the browser.
     */
    fun onOpenTab(tab: Tab)

    /**
     * Called when a tab should be selected in multiselect mode.
     */
    fun onAddSelectedTab(tab: Tab)

    /**
     * Called when a tab should be unselected in multiselect mode.
     */
    fun onRemoveSelectedTab(tab: Tab)

    /**
     * Called when multiselect mode should be entered with no tabs selected.
     */
    fun onEnterMultiselect()
}

/**
 * Interactor for the tab tray fragment.
 */
@Suppress("TooManyFunctions")
class TabTrayFragmentInteractor(private val controller: TabTrayController) : TabTrayInteractor {
    override fun onNewTabTapped(private: Boolean) {
        controller.onNewTabTapped(private)
    }

    override fun onTabTrayDismissed() {
        controller.onTabTrayDismissed()
    }

    override fun onShareTabsClicked(private: Boolean) {
        controller.onShareTabsClicked(private)
    }

    override fun onSaveToCollectionClicked(selectedTabs: Set<Tab>) {
        controller.onSaveToCollectionClicked(selectedTabs)
    }

    override fun onCloseAllTabsClicked(private: Boolean) {
        controller.onCloseAllTabsClicked(private)
    }

    override fun onSyncedTabClicked(syncTab: SyncTab) {
        controller.onSyncedTabClicked(syncTab)
    }

    override fun onBackPressed(): Boolean {
        return controller.handleBackPressed()
    }

    override fun onModeRequested(): TabTrayDialogFragmentState.Mode {
        return controller.onModeRequested()
    }

    override fun onAddSelectedTab(tab: Tab) {
        controller.handleAddSelectedTab(tab)
    }

    override fun onRemoveSelectedTab(tab: Tab) {
        controller.handleRemoveSelectedTab(tab)
    }

    override fun onOpenTab(tab: Tab) {
        controller.handleOpenTab(tab)
    }

    override fun onEnterMultiselect() {
        controller.handleEnterMultiselect()
    }
}
