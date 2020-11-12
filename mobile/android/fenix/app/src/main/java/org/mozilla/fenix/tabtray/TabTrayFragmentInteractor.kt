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
    fun onShareTabsOfTypeClicked(private: Boolean)

    /**
     * Called when user clicks button to share selected tabs in multiselect.
     */
    fun onShareSelectedTabsClicked(selectedTabs: Set<Tab>)

    /**
     * Called when user clicks bookmark button in menu to bookmark selected tabs in multiselect.
     */
    fun onBookmarkSelectedTabs(selectedTabs: Set<Tab>)

    /**
     * Called when user clicks delete button in menu to delete selected tabs in multiselect.
     */
    fun onDeleteSelectedTabs(selectedTabs: Set<Tab>)

    /**
     * Called when user clicks the tab settings button.
     */
    fun onTabSettingsClicked()

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
     * Called when user clicks on the action button prompt in the info banner CFR for
     * automatically closing tabs or changing the layout of open tabs.
     */
    fun onGoToTabsSettings()

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

    /**
     * Called when user clicks the recently closed tabs menu button.
     */
    fun onOpenRecentlyClosedClicked()
}

/**
 * Interactor for the tab tray fragment.
 */
@Suppress("TooManyFunctions")
class TabTrayFragmentInteractor(private val controller: TabTrayController) : TabTrayInteractor {
    override fun onNewTabTapped(private: Boolean) {
        controller.handleNewTabTapped(private)
    }

    override fun onTabTrayDismissed() {
        controller.handleTabTrayDismissed()
    }

    override fun onTabSettingsClicked() {
        controller.handleTabSettingsClicked()
    }

    override fun onOpenRecentlyClosedClicked() {
        controller.handleRecentlyClosedClicked()
    }

    override fun onShareTabsOfTypeClicked(private: Boolean) {
        controller.handleShareTabsOfTypeClicked(private)
    }

    override fun onShareSelectedTabsClicked(selectedTabs: Set<Tab>) {
        controller.handleShareSelectedTabsClicked(selectedTabs)
    }

    override fun onBookmarkSelectedTabs(selectedTabs: Set<Tab>) {
        controller.handleBookmarkSelectedTabs(selectedTabs)
    }

    override fun onDeleteSelectedTabs(selectedTabs: Set<Tab>) {
        controller.handleDeleteSelectedTabs(selectedTabs)
    }

    override fun onSaveToCollectionClicked(selectedTabs: Set<Tab>) {
        controller.handleSaveToCollectionClicked(selectedTabs)
    }

    override fun onCloseAllTabsClicked(private: Boolean) {
        controller.handleCloseAllTabsClicked(private)
    }

    override fun onSyncedTabClicked(syncTab: SyncTab) {
        controller.handleSyncedTabClicked(syncTab)
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

    override fun onGoToTabsSettings() {
        controller.handleGoToTabsSettingClicked()
    }
}
