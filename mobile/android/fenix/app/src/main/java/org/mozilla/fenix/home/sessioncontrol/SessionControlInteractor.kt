/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import android.view.View
import mozilla.components.feature.tab.collections.Tab
import mozilla.components.feature.tab.collections.TabCollection

/**
 * Interface for collection related actions in the [SessionControlInteractor].
 */
interface CollectionInteractor {
    /**
     * Shows the Collection Creation fragment for selecting the tabs to add to the given tab
     * collection. Called when a user taps on the "Add tab" collection menu item.
     *
     * @param collection The collection of tabs that will be modified.
     */
    fun onCollectionAddTabTapped(collection: TabCollection)

    /**
     * Opens the given tab. Called when a user clicks on a tab in the tab collection.
     *
     * @param tab The tab to open from the tab collection.
     */
    fun onCollectionOpenTabClicked(tab: Tab)

    /**
     * Opens all the tabs in a given tab collection. Called when a user taps on the "Open tabs"
     * collection menu item.
     *
     * @param collection The collection of tabs to open.
     */
    fun onCollectionOpenTabsTapped(collection: TabCollection)

    /**
     * Removes the given tab from the given tab collection. Called when a user swipes to remove a
     * tab or clicks on the tab close button.
     *
     * @param collection The collection of tabs that will be modified.
     * @param tab The tab to remove from the tab collection.
     */
    fun onCollectionRemoveTab(collection: TabCollection, tab: Tab)

    /**
     * Shares the tabs in the given tab collection. Called when a user clicks on the Collection
     * Share button.
     *
     * @param collection The collection of tabs to share.
     */
    fun onCollectionShareTabsClicked(collection: TabCollection)

    /**
     * Shows a prompt for deleting the given tab collection. Called when a user taps on the
     * "Delete collection" collection menu item.
     *
     * @param collection The collection of tabs to delete.
     */
    fun onDeleteCollectionTapped(collection: TabCollection)

    /**
     * Shows the Collection Creation fragment for renaming the given tab collection. Called when a
     * user taps on the "Rename collection" collection menu item.
     *
     * @param collection The collection of tabs to rename.
     */
    fun onRenameCollectionTapped(collection: TabCollection)

    /**
     * Toggles expanding or collapsing the given tab collection. Called when a user clicks on a
     * [CollectionViewHolder].
     *
     * @param collection The collection of tabs that will be collapsed.
     * @param expand True if the given tab collection should be expanded or collapse if false.
     */
    fun onToggleCollectionExpanded(collection: TabCollection, expand: Boolean)
}

/**
 * Interface for onboarding related actions in the [SessionControlInteractor].
 */
interface OnboardingInteractor {
    /**
     * Hides the onboarding and navigates to Search. Called when a user clicks on the "Start Browsing" button.
     */
    fun onStartBrowsingClicked()
}

/**
 * Interface for tab related actions in the [SessionControlInteractor].
 */
interface TabSessionInteractor {
    /**
     * Closes the given tab. Called when a user swipes to close a tab or clicks on the Close Tab
     * button in the tab view.
     *
     * @param sessionId The selected tab session id to close.
     */
    fun onCloseTab(sessionId: String)

    /**
     * Closes all the tabs. Called when a user clicks on the Close Tabs button or "Close all tabs"
     * tab header menu item.
     *
     * @param isPrivateMode True if the [BrowsingMode] is [Private] and false otherwise.
     */
    fun onCloseAllTabs(isPrivateMode: Boolean)

    /**
     * Pauses all playing [Media]. Called when a user clicks on the Pause button in the tab view.
     */
    fun onPauseMediaClicked()

    /**
     * Resumes playing all paused [Media]. Called when a user clicks on the Play button in the tab
     * view.
     */
    fun onPlayMediaClicked()

    /**
     * Shows the Private Browsing Learn More page in a new tab. Called when a user clicks on the
     * "Common myths about private browsing" link in private mode.
     */
    fun onPrivateBrowsingLearnMoreClicked()

    /**
     * Saves the given tab to collection. Called when a user clicks on the "Save to collection"
     * button or tab header menu item, and on long click of an open tab.
     *
     * @param sessionId The selected tab session id to save.
     */
    fun onSaveToCollection(sessionId: String?)

    /**
     * Selects the given tab. Called when a user clicks on a tab.
     *
     * @param tabView [View] of the current Fragment to match with a View in the Fragment being
     * navigated to.
     * @param sessionId The tab session id to select.
     */
    fun onSelectTab(tabView: View, sessionId: String)

    /**
     * Shares the current opened tabs. Called when a user clicks on the Share Tabs button in private
     * mode or tab header menu item.
     */
    fun onShareTabs()
}

/**
 * Interface for top site related actions in the [SessionControlInteractor].
 */
interface TopSiteInteractor {
    /**
     * Selects the given top site. Called when a user clicks on a top site.
     *
     * @param url The URL of the top site.
     */
    fun onSelectTopSite(url: String)
}

/**
 * Interactor for the Home screen.
 * Provides implementations for the CollectionInteractor, OnboardingInteractor,
 * TabSessionInteractor and TopSiteInteractor.
 */
@SuppressWarnings("TooManyFunctions")
class SessionControlInteractor(
    private val controller: SessionControlController
) : CollectionInteractor, OnboardingInteractor, TabSessionInteractor, TopSiteInteractor {
    override fun onCloseTab(sessionId: String) {
        controller.handleCloseTab(sessionId)
    }

    override fun onCloseAllTabs(isPrivateMode: Boolean) {
        controller.handleCloseAllTabs(isPrivateMode)
    }

    override fun onCollectionAddTabTapped(collection: TabCollection) {
        controller.handleCollectionAddTabTapped(collection)
    }

    override fun onCollectionOpenTabClicked(tab: Tab) {
        controller.handleCollectionOpenTabClicked(tab)
    }

    override fun onCollectionOpenTabsTapped(collection: TabCollection) {
        controller.handleCollectionOpenTabsTapped(collection)
    }

    override fun onCollectionRemoveTab(collection: TabCollection, tab: Tab) {
        controller.handleCollectionRemoveTab(collection, tab)
    }

    override fun onCollectionShareTabsClicked(collection: TabCollection) {
        controller.handleCollectionShareTabsClicked(collection)
    }

    override fun onDeleteCollectionTapped(collection: TabCollection) {
        controller.handleDeleteCollectionTapped(collection)
    }

    override fun onPauseMediaClicked() {
        controller.handlePauseMediaClicked()
    }

    override fun onPlayMediaClicked() {
        controller.handlePlayMediaClicked()
    }

    override fun onPrivateBrowsingLearnMoreClicked() {
        controller.handlePrivateBrowsingLearnMoreClicked()
    }

    override fun onRenameCollectionTapped(collection: TabCollection) {
        controller.handleRenameCollectionTapped(collection)
    }

    override fun onSaveToCollection(sessionId: String?) {
        controller.handleSaveTabToCollection(sessionId)
    }

    override fun onSelectTab(tabView: View, sessionId: String) {
        controller.handleSelectTab(tabView, sessionId)
    }

    override fun onSelectTopSite(url: String) {
        controller.handleSelectTopSite(url)
    }

    override fun onShareTabs() {
        controller.handleShareTabs()
    }

    override fun onStartBrowsingClicked() {
        controller.handleStartBrowsingClicked()
    }

    override fun onToggleCollectionExpanded(collection: TabCollection, expand: Boolean) {
        controller.handleToggleCollectionExpanded(collection, expand)
    }
}
