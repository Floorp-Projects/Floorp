/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.feature.tab.collections.Tab
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.top.sites.TopSite
import org.mozilla.fenix.components.tips.Tip
import org.mozilla.fenix.home.recentbookmarks.controller.RecentBookmarksController
import org.mozilla.fenix.home.recentbookmarks.interactor.RecentBookmarksInteractor
import org.mozilla.fenix.home.recenttabs.controller.RecentTabController
import org.mozilla.fenix.home.recenttabs.interactor.RecentTabInteractor

/**
 * Interface for tab related actions in the [SessionControlInteractor].
 */
interface TabSessionInteractor {
    /**
     * Shows the Private Browsing Learn More page in a new tab. Called when a user clicks on the
     * "Common myths about private browsing" link in private mode.
     */
    fun onPrivateBrowsingLearnMoreClicked()
}

/**
 * Interface for collection related actions in the [SessionControlInteractor].
 */
@SuppressWarnings("TooManyFunctions")
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
    fun onCollectionRemoveTab(collection: TabCollection, tab: Tab, wasSwiped: Boolean)

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

    /**
     * Opens the collection creator
     */
    fun onAddTabsToCollectionTapped()

    /**
     * User has removed the collections placeholder from home.
     */
    fun onRemoveCollectionsPlaceholder()

    /**
     * User has opened collection 3 dot menu.
     */
    fun onCollectionMenuOpened()
}

interface ToolbarInteractor {
    /**
     * Navigates to browser with clipboard text.
     */
    fun onPasteAndGo(clipboardText: String)

    /**
     * Navigates to search with clipboard text.
     */
    fun onPaste(clipboardText: String)
}

/**
 * Interface for onboarding related actions in the [SessionControlInteractor].
 */
interface OnboardingInteractor {
    /**
     * Hides the onboarding and navigates to Search. Called when a user clicks on the "Start Browsing" button.
     */
    fun onStartBrowsingClicked()

    /**
     * Hides the onboarding and navigates to Settings. Called when a user clicks on the "Open settings" button.
     */
    fun onOpenSettingsClicked()

    /**
     * Opens a custom tab to what's new url. Called when a user clicks on the "Get answers here" link.
     */
    fun onWhatsNewGetAnswersClicked()

    /**
     * Opens a custom tab to privacy notice url. Called when a user clicks on the "read our privacy notice" button.
     */
    fun onReadPrivacyNoticeClicked()
}

interface TipInteractor {
    /**
     * Dismisses the tip view adapter
     */
    fun onCloseTip(tip: Tip)
}

/**
 * Interface for top site related actions in the [SessionControlInteractor].
 */
interface TopSiteInteractor {
    /**
     * Opens the given top site in private mode. Called when an user clicks on the "Open in private
     * tab" top site menu item.
     *
     * @param topSite The top site that will be open in private mode.
     */
    fun onOpenInPrivateTabClicked(topSite: TopSite)

    /**
     * Opens a dialog to rename the given top site. Called when an user clicks on the "Rename" top site menu item.
     *
     * @param topSite The top site that will be renamed.
     */
    fun onRenameTopSiteClicked(topSite: TopSite)

    /**
     * Removes the given top site. Called when an user clicks on the "Remove" top site menu item.
     *
     * @param topSite The top site that will be removed.
     */
    fun onRemoveTopSiteClicked(topSite: TopSite)

    /**
     * Selects the given top site. Called when a user clicks on a top site.
     *
     * @param url The URL of the top site.
     * @param type The type of the top site.
     */
    fun onSelectTopSite(url: String, type: TopSite.Type)

    /**
     * Called when top site menu is opened.
     */
    fun onTopSiteMenuOpened()
}

interface ExperimentCardInteractor {
    /**
     * Called when set default browser button is clicked
     */
    fun onSetDefaultBrowserClicked()

    /**
     * Called when close button on experiment card
     */
    fun onCloseExperimentCardClicked()
}

/**
 * Interactor for the Home screen. Provides implementations for the CollectionInteractor,
 * OnboardingInteractor, TopSiteInteractor, TipInteractor, TabSessionInteractor,
 * ToolbarInteractor, ExperimentCardInteractor, RecentTabInteractor, and RecentBookmarksInteractor.
 */
@SuppressWarnings("TooManyFunctions")
class SessionControlInteractor(
    private val controller: SessionControlController,
    private val recentTabController: RecentTabController,
    private val recentBookmarksController: RecentBookmarksController
) : CollectionInteractor, OnboardingInteractor, TopSiteInteractor, TipInteractor,
    TabSessionInteractor, ToolbarInteractor, ExperimentCardInteractor, RecentTabInteractor,
    RecentBookmarksInteractor {

    override fun onCollectionAddTabTapped(collection: TabCollection) {
        controller.handleCollectionAddTabTapped(collection)
    }

    override fun onCollectionOpenTabClicked(tab: Tab) {
        controller.handleCollectionOpenTabClicked(tab)
    }

    override fun onCollectionOpenTabsTapped(collection: TabCollection) {
        controller.handleCollectionOpenTabsTapped(collection)
    }

    override fun onCollectionRemoveTab(collection: TabCollection, tab: Tab, wasSwiped: Boolean) {
        controller.handleCollectionRemoveTab(collection, tab, wasSwiped)
    }

    override fun onCollectionShareTabsClicked(collection: TabCollection) {
        controller.handleCollectionShareTabsClicked(collection)
    }

    override fun onDeleteCollectionTapped(collection: TabCollection) {
        controller.handleDeleteCollectionTapped(collection)
    }

    override fun onOpenInPrivateTabClicked(topSite: TopSite) {
        controller.handleOpenInPrivateTabClicked(topSite)
    }

    override fun onRenameTopSiteClicked(topSite: TopSite) {
        controller.handleRenameTopSiteClicked(topSite)
    }

    override fun onRemoveTopSiteClicked(topSite: TopSite) {
        controller.handleRemoveTopSiteClicked(topSite)
    }

    override fun onRenameCollectionTapped(collection: TabCollection) {
        controller.handleRenameCollectionTapped(collection)
    }

    override fun onSelectTopSite(url: String, type: TopSite.Type) {
        controller.handleSelectTopSite(url, type)
    }

    override fun onStartBrowsingClicked() {
        controller.handleStartBrowsingClicked()
    }

    override fun onOpenSettingsClicked() {
        controller.handleOpenSettingsClicked()
    }

    override fun onWhatsNewGetAnswersClicked() {
        controller.handleWhatsNewGetAnswersClicked()
    }

    override fun onReadPrivacyNoticeClicked() {
        controller.handleReadPrivacyNoticeClicked()
    }

    override fun onToggleCollectionExpanded(collection: TabCollection, expand: Boolean) {
        controller.handleToggleCollectionExpanded(collection, expand)
    }

    override fun onAddTabsToCollectionTapped() {
        controller.handleCreateCollection()
    }

    override fun onCloseTip(tip: Tip) {
        controller.handleCloseTip(tip)
    }

    override fun onPrivateBrowsingLearnMoreClicked() {
        controller.handlePrivateBrowsingLearnMoreClicked()
    }

    override fun onPasteAndGo(clipboardText: String) {
        controller.handlePasteAndGo(clipboardText)
    }

    override fun onPaste(clipboardText: String) {
        controller.handlePaste(clipboardText)
    }

    override fun onRemoveCollectionsPlaceholder() {
        controller.handleRemoveCollectionsPlaceholder()
    }

    override fun onCollectionMenuOpened() {
        controller.handleMenuOpened()
    }

    override fun onTopSiteMenuOpened() {
        controller.handleMenuOpened()
    }

    override fun onSetDefaultBrowserClicked() {
        controller.handleSetDefaultBrowser()
    }

    override fun onCloseExperimentCardClicked() {
        controller.handleCloseExperimentCard()
    }

    override fun onRecentTabClicked(tabId: String) {
        recentTabController.handleRecentTabClicked(tabId)
    }

    override fun onRecentTabShowAllClicked() {
        recentTabController.handleRecentTabShowAllClicked()
    }

    /**
     * See [RecentBookmarksInteractor.onRecentBookmarkClicked].
     */
    override fun onRecentBookmarkClicked(bookmark: BookmarkNode) {
        recentBookmarksController.handleBookmarkClicked(bookmark)
    }

    /**
     * See [RecentBookmarksInteractor.onShowAllBookmarksClicked].
     */
    override fun onShowAllBookmarksClicked() {
        recentBookmarksController.handleShowAllBookmarksClicked()
    }
}
