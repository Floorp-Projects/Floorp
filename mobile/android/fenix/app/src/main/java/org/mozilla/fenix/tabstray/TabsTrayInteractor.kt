/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.storage.sync.Tab
import mozilla.components.browser.tabstray.TabsTray
import org.mozilla.fenix.tabstray.browser.InactiveTabsInteractor
import org.mozilla.fenix.tabstray.browser.TabsTrayFabInteractor

/**
 * Interactor for responding to all user actions in the tabs tray.
 */
interface TabsTrayInteractor :
    SyncedTabsInteractor,
    TabsTray.Delegate,
    InactiveTabsInteractor,
    TabsTrayFabInteractor {

    /**
     * Invoked when a page in the tabs tray is selected.
     *
     * @param position The position on the tray to focus.
     * @param smoothScroll If true, animate the scrolling from the current tab to [position].
     */
    fun onTrayPositionSelected(position: Int, smoothScroll: Boolean)

    /**
     * Invoked when the user confirmed tab removal that would lead to cancelled private downloads.
     *
     * @param tabId ID of the tab being removed.
     * @param source is the app feature from which the [TabSessionState] with [tabId] was closed.
     */
    fun onDeletePrivateTabWarningAccepted(tabId: String, source: String? = null)

    /**
     * Invoked when the selected tabs are requested to be deleted.
     */
    fun onDeleteSelectedTabsClicked()

    /**
     * Invoked when the debug menu option for inactive tabs is clicked.
     */
    fun onForceSelectedTabsAsInactiveClicked()

    /**
     * Invoked when the bookmark button in the multi selection banner is clicked.
     */
    fun onBookmarkSelectedTabsClicked()

    /**
     * Invoked when the collections button in the multi selection banner is clicked.
     */
    fun onAddSelectedTabsToCollectionClicked()

    /**
     * Invoked when the share button in the multi selection banner is clicked.
     */
    fun onShareSelectedTabs()

    /**
     * Invoked when a drag-drop operation with a tab is completed.
     *
     * @param tabId ID of the tab being moved.
     * @param targetId ID of the tab the moved tab's new neighbor.
     * @param placeAfter [Boolean] indicating whether the moved tab is being placed before or after [targetId].
     */
    fun onTabsMove(
        tabId: String,
        targetId: String?,
        placeAfter: Boolean,
    )

    /**
     * Invoked when the recently closed item is clicked.
     */
    fun onRecentlyClosedClicked()

    /**
     * Invoked when the a tab's media controls are clicked.
     *
     * @param tab [TabSessionState] to close.
     */
    fun onMediaClicked(tab: TabSessionState)

    /**
     * Invoked when a tab is long clicked.
     *
     * @param tab [TabSessionState] that was clicked.
     */
    fun onTabLongClicked(tab: TabSessionState): Boolean

    /**
     * Invoked when the back button is pressed.
     *
     * @return true if the back button press was consumed.
     */
    fun onBackPressed(): Boolean

    /**
     * Invoked when a tab is unselected.
     *
     * @param tab [TabSessionState] that was unselected.
     */
    fun onTabUnselected(tab: TabSessionState)
}

/**
 * Default implementation of [TabsTrayInteractor].
 *
 * @property controller [TabsTrayController] to which user actions can be delegated for app updates.
 */
@Suppress("TooManyFunctions")
class DefaultTabsTrayInteractor(
    private val controller: TabsTrayController,
) : TabsTrayInteractor {

    override fun onTrayPositionSelected(position: Int, smoothScroll: Boolean) {
        controller.handleTrayScrollingToPosition(position, smoothScroll)
    }

    override fun onDeletePrivateTabWarningAccepted(tabId: String, source: String?) {
        controller.handleDeleteTabWarningAccepted(tabId, source)
    }

    override fun onDeleteSelectedTabsClicked() {
        controller.handleDeleteSelectedTabsClicked()
    }

    override fun onTabsMove(
        tabId: String,
        targetId: String?,
        placeAfter: Boolean,
    ) {
        controller.handleTabsMove(tabId, targetId, placeAfter)
    }

    override fun onForceSelectedTabsAsInactiveClicked() {
        controller.handleForceSelectedTabsAsInactiveClicked()
    }

    override fun onBookmarkSelectedTabsClicked() {
        controller.handleBookmarkSelectedTabsClicked()
    }

    override fun onAddSelectedTabsToCollectionClicked() {
        controller.handleAddSelectedTabsToCollectionClicked()
    }

    override fun onShareSelectedTabs() {
        controller.handleShareSelectedTabsClicked()
    }

    override fun onSyncedTabClicked(tab: Tab) {
        controller.handleSyncedTabClicked(tab)
    }

    override fun onBackPressed(): Boolean = controller.handleBackPressed()

    override fun onTabClosed(tab: TabSessionState, source: String?) {
        controller.handleTabDeletion(tab.id, source)
    }

    override fun onTabSelected(tab: TabSessionState, source: String?) {
        controller.handleTabSelected(tab, source)
    }

    override fun onNormalTabsFabClicked() {
        controller.handleNormalTabsFabClick()
    }

    override fun onPrivateTabsFabClicked() {
        controller.handlePrivateTabsFabClick()
    }

    override fun onSyncedTabsFabClicked() {
        controller.handleSyncedTabsFabClick()
    }

    override fun onRecentlyClosedClicked() {
        controller.handleNavigateToRecentlyClosed()
    }

    override fun onMediaClicked(tab: TabSessionState) {
        controller.handleMediaClicked(tab)
    }

    override fun onTabLongClicked(tab: TabSessionState): Boolean {
        return controller.handleTabLongClick(tab)
    }

    override fun onTabUnselected(tab: TabSessionState) {
        controller.handleTabUnselected(tab)
    }

    /**
     * See [InactiveTabsInteractor.onInactiveTabsHeaderClicked].
     */
    override fun onInactiveTabsHeaderClicked(expanded: Boolean) {
        controller.handleInactiveTabsHeaderClicked(expanded)
    }

    /**
     * See [InactiveTabsInteractor.onAutoCloseDialogCloseButtonClicked].
     */
    override fun onAutoCloseDialogCloseButtonClicked() {
        controller.handleInactiveTabsAutoCloseDialogDismiss()
    }

    /**
     * See [InactiveTabsInteractor.onEnableAutoCloseClicked].
     */
    override fun onEnableAutoCloseClicked() {
        controller.handleEnableInactiveTabsAutoCloseClicked()
    }

    /**
     * See [InactiveTabsInteractor.onInactiveTabClicked].
     */
    override fun onInactiveTabClicked(tab: TabSessionState) {
        controller.handleInactiveTabClicked(tab)
    }

    /**
     * See [InactiveTabsInteractor.onInactiveTabClosed].
     */
    override fun onInactiveTabClosed(tab: TabSessionState) {
        controller.handleCloseInactiveTabClicked(tab)
    }

    /**
     * See [InactiveTabsInteractor.onDeleteAllInactiveTabsClicked].
     */
    override fun onDeleteAllInactiveTabsClicked() {
        controller.handleDeleteAllInactiveTabsClicked()
    }
}
