/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.util.Log
import android.view.View
import androidx.compose.ui.semantics.SemanticsActions
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.assert
import androidx.compose.ui.test.assertIsNotSelected
import androidx.compose.ui.test.assertIsSelected
import androidx.compose.ui.test.filter
import androidx.compose.ui.test.hasAnyChild
import androidx.compose.ui.test.hasContentDescription
import androidx.compose.ui.test.hasParent
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.hasText
import androidx.compose.ui.test.junit4.ComposeTestRule
import androidx.compose.ui.test.longClick
import androidx.compose.ui.test.onAllNodesWithTag
import androidx.compose.ui.test.onChildAt
import androidx.compose.ui.test.onChildren
import androidx.compose.ui.test.onFirst
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.performSemanticsAction
import androidx.compose.ui.test.performTouchInput
import androidx.compose.ui.test.swipeLeft
import androidx.compose.ui.test.swipeRight
import androidx.test.espresso.Espresso
import androidx.test.espresso.UiController
import androidx.test.espresso.ViewAction
import androidx.test.espresso.action.GeneralLocation
import androidx.test.espresso.assertion.ViewAssertions
import androidx.test.espresso.matcher.ViewMatchers
import com.google.android.material.bottomsheet.BottomSheetBehavior
import org.hamcrest.Matcher
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.HomeActivityComposeTestRule
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.clickAtLocationInView
import org.mozilla.fenix.helpers.idlingresource.BottomSheetBehaviorStateIdlingResource
import org.mozilla.fenix.helpers.matchers.BottomSheetBehaviorHalfExpandedMaxRatioMatcher
import org.mozilla.fenix.helpers.matchers.BottomSheetBehaviorStateMatcher
import org.mozilla.fenix.tabstray.TabsTrayTestTag

/**
 * Implementation of Robot Pattern for the Tabs Tray.
 */
class ComposeTabDrawerRobot(private val composeTestRule: HomeActivityComposeTestRule) {

    fun verifyNormalBrowsingButtonIsSelected(isSelected: Boolean = true) {
        if (isSelected) {
            composeTestRule.normalBrowsingButton().assertIsSelected()
            Log.i(TAG, "verifyNormalBrowsingButtonIsSelected: Verified normal browsing button is selected")
        } else {
            composeTestRule.normalBrowsingButton().assertIsNotSelected()
            Log.i(TAG, "verifyNormalBrowsingButtonIsSelected: Verified normal browsing button is not selected")
        }
    }

    fun verifyPrivateBrowsingButtonIsSelected(isSelected: Boolean = true) {
        if (isSelected) {
            composeTestRule.privateBrowsingButton().assertIsSelected()
            Log.i(TAG, "verifyPrivateBrowsingButtonIsSelected: Verified private browsing button is selected")
        } else {
            composeTestRule.privateBrowsingButton().assertIsNotSelected()
            Log.i(TAG, "verifyPrivateBrowsingButtonIsSelected: Verified private browsing button is not selected")
        }
    }

    fun verifySyncedTabsButtonIsSelected(isSelected: Boolean = true) {
        if (isSelected) {
            composeTestRule.syncedTabsButton().assertIsSelected()
            Log.i(TAG, "verifySyncedTabsButtonIsSelected: Verified synced tabs button is selected")
        } else {
            composeTestRule.syncedTabsButton().assertIsNotSelected()
            Log.i(TAG, "verifySyncedTabsButtonIsSelected: Verified synced tabs button is not selected")
        }
    }

    fun verifySyncedTabsListWhenUserIsNotSignedIn() {
        verifySyncedTabsList()
        assertUIObjectExists(
            itemContainingText(getStringResource(R.string.synced_tabs_sign_in_message)),
            itemContainingText(getStringResource(R.string.sync_sign_in)),
            itemContainingText(getStringResource(R.string.tab_drawer_fab_sync)),
        )
    }

    fun verifyExistingOpenTabs(vararg titles: String) {
        titles.forEach { title ->
            itemContainingText(title).waitForExists(waitingTime)
            composeTestRule.tabItem(title).assertExists()
            Log.i(TAG, "verifyExistingOpenTabs: Verified open tab with title: $title exists")
        }
    }

    fun verifyOpenTabsOrder(title: String, position: Int) {
        composeTestRule.normalTabsList()
            .onChildAt(position - 1)
            .assert(hasTestTag(TabsTrayTestTag.tabItemRoot))
            .assert(hasAnyChild(hasText(title)))
        Log.i(TAG, "verifyOpenTabsOrder: Verified open tab at position: $position has title: $title")
    }

    fun verifyNoExistingOpenTabs(vararg titles: String) {
        titles.forEach { title ->
            assertUIObjectExists(
                itemContainingText(title),
                exists = false,
            )
        }
    }

    fun verifyNormalTabsList() {
        composeTestRule.normalTabsList().assertExists()
        Log.i(TAG, "verifyNormalTabsList: Verified normal tabs list exists")
    }

    fun verifyPrivateTabsList() {
        composeTestRule.privateTabsList().assertExists()
        Log.i(TAG, "verifyPrivateTabsList: Verified private tabs list exists")
    }

    fun verifySyncedTabsList() {
        composeTestRule.syncedTabsList().assertExists()
        Log.i(TAG, "verifySyncedTabsList: Verified synced tabs list exists")
    }

    fun verifyNoOpenTabsInNormalBrowsing() {
        composeTestRule.emptyNormalTabsList().assertExists()
        Log.i(TAG, "verifyNoOpenTabsInNormalBrowsing: Verified empty normal tabs list exists")
    }

    fun verifyNoOpenTabsInPrivateBrowsing() {
        composeTestRule.emptyPrivateTabsList().assertExists()
        Log.i(TAG, "verifyNoOpenTabsInPrivateBrowsing: Verified empty private tabs list exists")
    }

    fun verifyAccountSettingsButton() {
        composeTestRule.dropdownMenuItemAccountSettings().assertExists()
        Log.i(TAG, "verifyAccountSettingsButton: Verified \"Account settings\" menu button exists")
    }

    fun verifyCloseAllTabsButton() {
        composeTestRule.dropdownMenuItemCloseAllTabs().assertExists()
        Log.i(TAG, "verifyCloseAllTabsButton: Verified \"Close all tabs\" menu button exists")
    }

    fun verifySelectTabsButton() {
        composeTestRule.dropdownMenuItemSelectTabs().assertExists()
        Log.i(TAG, "verifySelectTabsButton: Verified \"Select tabs\" menu button exists")
    }

    fun verifyShareAllTabsButton() {
        composeTestRule.dropdownMenuItemShareAllTabs().assertExists()
        Log.i(TAG, "verifyShareAllTabsButton: Verified \"Share all tabs\" menu button exists")
    }

    fun verifyRecentlyClosedTabsButton() {
        composeTestRule.dropdownMenuItemRecentlyClosedTabs().assertExists()
        Log.i(TAG, "verifyRecentlyClosedTabsButton: Verified \"Recently closed tabs\" menu button exists")
    }

    fun verifyTabSettingsButton() {
        composeTestRule.dropdownMenuItemTabSettings().assertExists()
        Log.i(TAG, "verifyTabSettingsButton: Verified \"Tab settings\" menu button exists")
    }

    fun verifyThreeDotButton() {
        composeTestRule.threeDotButton().assertExists()
        Log.i(TAG, "verifyThreeDotButton: Verified three dot button exists")
    }

    fun verifyFab() {
        composeTestRule.tabsTrayFab().assertExists()
        Log.i(TAG, "verifyFab: Verified new tab FAB button exists")
    }

    fun verifyNormalTabCounter() {
        composeTestRule.normalTabsCounter().assertExists()
        Log.i(TAG, "verifyNormalTabCounter: Verified normal tabs list counter exists")
    }

    /**
     * Verifies a tab's thumbnail when there is only one tab open.
     */
    fun verifyTabThumbnail() {
        composeTestRule.tabThumbnail().assertExists()
        Log.i(TAG, "verifyTabThumbnail: Verified tab thumbnail exists")
    }

    /**
     * Verifies a tab's close button when there is only one tab open.
     */
    fun verifyTabCloseButton() {
        composeTestRule.closeTabButton().assertExists()
        Log.i(TAG, "verifyTabCloseButton: Verified close tab button exists")
    }

    fun verifyTabsTrayBehaviorState(expectedState: Int) {
        tabsTrayView().check(ViewAssertions.matches(BottomSheetBehaviorStateMatcher(expectedState)))
        Log.i(TAG, "verifyTabsTrayBehaviorState: Verified that the tabs tray state matches: $expectedState")
    }

    fun verifyMinusculeHalfExpandedRatio() {
        tabsTrayView().check(ViewAssertions.matches(BottomSheetBehaviorHalfExpandedMaxRatioMatcher(0.001f)))
        Log.i(TAG, "verifyMinusculeHalfExpandedRatio: Verified that the tabs tray half expanded ratio")
    }

    fun verifyTabTrayIsOpen() {
        composeTestRule.tabsTray().assertExists()
        Log.i(TAG, "verifyTabTrayIsOpen: Verified that the open tabs tray exists")
    }

    fun verifyTabTrayIsClosed() {
        composeTestRule.tabsTray().assertDoesNotExist()
        Log.i(TAG, "verifyTabTrayIsClosed: Verified that the tabs tray is closed")
    }

    /**
     * Closes a tab when there is only one tab open.
     */
    @OptIn(ExperimentalTestApi::class)
    fun closeTab() {
        composeTestRule.waitUntilAtLeastOneExists(hasTestTag(TabsTrayTestTag.tabItemClose))
        composeTestRule.closeTabButton().assertExists()
        composeTestRule.closeTabButton().performClick()
        Log.i(TAG, "closeTab: Clicked close tab button")
    }

    /**
     * Swipes a tab with [title] left.
     */
    fun swipeTabLeft(title: String) {
        composeTestRule.tabItem(title).performTouchInput { swipeLeft() }
        Log.i(TAG, "swipeTabLeft: Performed swipe left action on tab: $title")
    }

    /**
     * Swipes a tab with [title] right.
     */
    fun swipeTabRight(title: String) {
        composeTestRule.tabItem(title).performTouchInput { swipeRight() }
        Log.i(TAG, "swipeTabRight: Performed swipe right action on tab: $title")
    }

    /**
     * Creates a collection from the provided [tabTitles].
     */
    fun createCollection(
        vararg tabTitles: String,
        collectionName: String,
        firstCollection: Boolean = true,
    ) {
        composeTestRule.threeDotButton().performClick()
        Log.i(TAG, "createCollection: Clicked 3 dot button")
        composeTestRule.dropdownMenuItemSelectTabs().performClick()
        Log.i(TAG, "createCollection: Clicked \"Select tabs\" menu button")

        for (tab in tabTitles) {
            selectTab(tab)
        }

        clickCollectionsButton(composeTestRule) {
            if (!firstCollection) {
                clickAddNewCollection()
            }
            typeCollectionNameAndSave(collectionName)
        }
    }

    /**
     * Selects a tab with [title].
     */
    @OptIn(ExperimentalTestApi::class)
    fun selectTab(title: String) {
        Log.i(TAG, "selectTab: Waiting for tab with title: $title to exist")
        composeTestRule.waitUntilExactlyOneExists(hasText(title), waitingTime)
        composeTestRule.tabItem(title).performClick()
        Log.i(TAG, "selectTab: Clicked tab with title: $title")
    }

    /**
     * Performs a long click on a tab with [title].
     */
    fun longClickTab(title: String) {
        composeTestRule.tabItem(title)
            .performTouchInput { longClick(durationMillis = Constants.LONG_CLICK_DURATION) }
        Log.i(TAG, "longClickTab: Long clicked tab with title: $title")
    }

    /**
     * Verifies the multi selection counter displays [numOfTabs].
     */
    fun verifyTabsMultiSelectionCounter(numOfTabs: Int) {
        composeTestRule.multiSelectionCounter()
            .assert(hasText("$numOfTabs selected"))
        Log.i(TAG, "verifyTabsMultiSelectionCounter: Verified $numOfTabs are selected")
    }

    /**
     * Verifies a tab's media button matches [action] when there is only one tab with media.
     */
    @OptIn(ExperimentalTestApi::class)
    fun verifyTabMediaControlButtonState(action: String) {
        Log.i(TAG, "verifyTabMediaControlButtonStateTab: Waiting for media tab control button: $action to exist")
        composeTestRule.waitUntilAtLeastOneExists(hasContentDescription(action), waitingTime)
        composeTestRule.tabMediaControlButton(action)
            .assertExists()
        Log.i(TAG, "verifyTabMediaControlButtonStateTab: Verified media tab control button: $action exists")
    }

    /**
     * Clicks a tab's media button when there is only one tab with media.
     */
    @OptIn(ExperimentalTestApi::class)
    fun clickTabMediaControlButton(action: String) {
        Log.i(TAG, "clickTabMediaControlButton: Waiting for media tab control button: $action to exist")
        composeTestRule.waitUntilAtLeastOneExists(hasContentDescription(action), waitingTime)
        composeTestRule.tabMediaControlButton(action)
            .performClick()
        Log.i(TAG, "clickTabMediaControlButton: Clicked media tab control button: $action")
    }

    /**
     * Closes a tab with a given [title].
     */
    fun closeTabWithTitle(title: String) {
        composeTestRule.onAllNodesWithTag(TabsTrayTestTag.tabItemClose)
            .filter(hasParent(hasText(title)))
            .onFirst()
            .performClick()
        Log.i(TAG, "closeTabWithTitle: Closed tab with title: $title")
    }

    class Transition(private val composeTestRule: HomeActivityComposeTestRule) {

        fun openNewTab(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            mDevice.waitForIdle()

            composeTestRule.tabsTrayFab().performClick()
            Log.i(TAG, "openNewTab: Clicked new tab FAB button")
            SearchRobot().interact()
            return SearchRobot.Transition()
        }

        fun toggleToNormalTabs(interact: ComposeTabDrawerRobot.() -> Unit): Transition {
            composeTestRule.normalBrowsingButton().performClick()
            Log.i(TAG, "toggleToNormalTabs: Clicked normal browsing button")
            ComposeTabDrawerRobot(composeTestRule).interact()
            return Transition(composeTestRule)
        }

        fun toggleToPrivateTabs(interact: ComposeTabDrawerRobot.() -> Unit): Transition {
            composeTestRule.privateBrowsingButton().performClick()
            Log.i(TAG, "toggleToPrivateTabs: Clicked private browsing button")
            ComposeTabDrawerRobot(composeTestRule).interact()
            return Transition(composeTestRule)
        }

        fun toggleToSyncedTabs(interact: ComposeTabDrawerRobot.() -> Unit): Transition {
            composeTestRule.syncedTabsButton().performClick()
            Log.i(TAG, "toggleToSyncedTabs: Clicked synced tabs button")
            ComposeTabDrawerRobot(composeTestRule).interact()
            return Transition(composeTestRule)
        }

        fun clickSignInToSyncButton(interact: SyncSignInRobot.() -> Unit): SyncSignInRobot.Transition {
            itemContainingText(getStringResource(R.string.sync_sign_in))
                .clickAndWaitForNewWindow(TestAssetHelper.waitingTimeShort)
            Log.i(TAG, "clickSignInToSyncButton: Clicked sign in to sync button")
            SyncSignInRobot().interact()
            return SyncSignInRobot.Transition()
        }

        fun openThreeDotMenu(interact: ComposeTabDrawerRobot.() -> Unit): Transition {
            composeTestRule.threeDotButton().performClick()
            Log.i(TAG, "openThreeDotMenu: Clicked three dot button")
            ComposeTabDrawerRobot(composeTestRule).interact()
            return Transition(composeTestRule)
        }

        fun closeAllTabs(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            composeTestRule.dropdownMenuItemCloseAllTabs().performClick()
            Log.i(TAG, "closeAllTabs: Clicked \"Close all tabs\" menu button")
            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun openTab(title: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            composeTestRule.tabItem(title)
                .performScrollTo()
                .performClick()
            Log.i(TAG, "openTab: Scrolled and clicked tab with title: $title")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openPrivateTab(position: Int, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            composeTestRule.privateTabsList()
                .onChildren()[position]
                .performClick()
            Log.i(TAG, "openPrivateTab: Opened private tab at position: ${position + 1}")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openNormalTab(position: Int, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            composeTestRule.normalTabsList()
                .onChildren()[position]
                .performClick()
            Log.i(TAG, "openNormalTab: Opened tab at position: ${position + 1}")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickTopBar(interact: ComposeTabDrawerRobot.() -> Unit): Transition {
            // The topBar contains other views.
            // Don't do the default click in the middle, rather click in some free space - top right.
            Espresso.onView(ViewMatchers.withId(R.id.topBar)).clickAtLocationInView(GeneralLocation.TOP_RIGHT)
            Log.i(TAG, "clickTopBar: Clicked tabs tray top bar")
            ComposeTabDrawerRobot(composeTestRule).interact()
            return Transition(composeTestRule)
        }

        fun waitForTabTrayBehaviorToIdle(interact: ComposeTabDrawerRobot.() -> Unit): Transition {
            // Need to get the behavior of tab_wrapper and wait for that to idle.
            var behavior: BottomSheetBehavior<*>? = null

            // Null check here since it's possible that the view is already animated away from the screen.
            tabsTrayView()?.perform(
                object : ViewAction {
                    override fun getDescription(): String {
                        return "Postpone actions to after the BottomSheetBehavior has settled"
                    }

                    override fun getConstraints(): Matcher<View> {
                        return ViewMatchers.isAssignableFrom(View::class.java)
                    }

                    override fun perform(uiController: UiController?, view: View?) {
                        behavior = BottomSheetBehavior.from(view!!)
                    }
                },
            )

            behavior?.let {
                runWithIdleRes(BottomSheetBehaviorStateIdlingResource(it)) {
                    ComposeTabDrawerRobot(composeTestRule).interact()
                }
            }

            return Transition(composeTestRule)
        }

        fun advanceToHalfExpandedState(interact: ComposeTabDrawerRobot.() -> Unit): Transition {
            tabsTrayView().perform(
                object : ViewAction {
                    override fun getDescription(): String {
                        return "Advance a BottomSheetBehavior to STATE_HALF_EXPANDED"
                    }

                    override fun getConstraints(): Matcher<View> {
                        return ViewMatchers.isAssignableFrom(View::class.java)
                    }

                    override fun perform(uiController: UiController?, view: View?) {
                        val behavior = BottomSheetBehavior.from(view!!)
                        behavior.state = BottomSheetBehavior.STATE_HALF_EXPANDED
                    }
                },
            )
            ComposeTabDrawerRobot(composeTestRule).interact()
            return Transition(composeTestRule)
        }

        fun closeTabDrawer(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            composeTestRule.bannerHandle().performSemanticsAction(SemanticsActions.OnClick)
            Log.i(TAG, "closeTabDrawer: Closed tabs tray clicking the handle")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickSaveCollection(interact: CollectionRobot.() -> Unit): CollectionRobot.Transition {
            composeTestRule.collectionsButton().performClick()
            Log.i(TAG, "clickSaveCollection: Clicked collections button")

            CollectionRobot().interact()
            return CollectionRobot.Transition()
        }

        fun clickShareAllTabsButton(interact: ShareOverlayRobot.() -> Unit): ShareOverlayRobot.Transition {
            composeTestRule.dropdownMenuItemShareAllTabs().performClick()
            Log.i(TAG, "clickShareAllTabsButton: Clicked \"Share all tabs\" menu button button")

            ShareOverlayRobot().interact()
            return ShareOverlayRobot.Transition()
        }
    }
}

/**
 * Opens a transition in the [ComposeTabDrawerRobot].
 */
fun composeTabDrawer(composeTestRule: HomeActivityComposeTestRule, interact: ComposeTabDrawerRobot.() -> Unit): ComposeTabDrawerRobot.Transition {
    ComposeTabDrawerRobot(composeTestRule).interact()
    return ComposeTabDrawerRobot.Transition(composeTestRule)
}

/**
 * Clicks on the Collections button in the Tabs Tray banner and opens a transition in the [CollectionRobot].
 */
private fun clickCollectionsButton(composeTestRule: HomeActivityComposeTestRule, interact: CollectionRobot.() -> Unit): CollectionRobot.Transition {
    composeTestRule.collectionsButton().performClick()
    Log.i(TAG, "clickCollectionsButton: Clicked collections button")

    CollectionRobot().interact()
    return CollectionRobot.Transition()
}

/**
 * Obtains the root [View] that wraps the Tabs Tray.
 */
private fun tabsTrayView() = Espresso.onView(ViewMatchers.withId(R.id.tabs_tray_root))

/**
 * Obtains the root Tabs Tray.
 */
private fun ComposeTestRule.tabsTray() = onNodeWithTag(TabsTrayTestTag.tabsTray)

/**
 * Obtains the Tabs Tray FAB.
 */
private fun ComposeTestRule.tabsTrayFab() = onNodeWithTag(TabsTrayTestTag.fab)

/**
 * Obtains the normal browsing page button of the Tabs Tray banner.
 */
private fun ComposeTestRule.normalBrowsingButton() = onNodeWithTag(TabsTrayTestTag.normalTabsPageButton)

/**
 * Obtains the private browsing page button of the Tabs Tray banner.
 */
private fun ComposeTestRule.privateBrowsingButton() = onNodeWithTag(TabsTrayTestTag.privateTabsPageButton)

/**
 * Obtains the synced tabs page button of the Tabs Tray banner.
 */
private fun ComposeTestRule.syncedTabsButton() = onNodeWithTag(TabsTrayTestTag.syncedTabsPageButton)

/**
 * Obtains the normal tabs list.
 */
private fun ComposeTestRule.normalTabsList() = onNodeWithTag(TabsTrayTestTag.normalTabsList)

/**
 * Obtains the private tabs list.
 */
private fun ComposeTestRule.privateTabsList() = onNodeWithTag(TabsTrayTestTag.privateTabsList)

/**
 * Obtains the synced tabs list.
 */
private fun ComposeTestRule.syncedTabsList() = onNodeWithTag(TabsTrayTestTag.syncedTabsList)

/**
 * Obtains the empty normal tabs list.
 */
private fun ComposeTestRule.emptyNormalTabsList() = onNodeWithTag(TabsTrayTestTag.emptyNormalTabsList)

/**
 * Obtains the empty private tabs list.
 */
private fun ComposeTestRule.emptyPrivateTabsList() = onNodeWithTag(TabsTrayTestTag.emptyPrivateTabsList)

/**
 * Obtains the tab with the provided [title]
 */
private fun ComposeTestRule.tabItem(title: String) = onAllNodesWithTag(TabsTrayTestTag.tabItemRoot)
    .filter(hasAnyChild(hasText(title)))
    .onFirst()

/**
 * Obtains an open tab's close button when there's only one tab open.
 */
private fun ComposeTestRule.closeTabButton() = onNodeWithTag(TabsTrayTestTag.tabItemClose)

/**
 * Obtains an open tab's thumbnail when there's only one tab open.
 */
private fun ComposeTestRule.tabThumbnail() = onNodeWithTag(TabsTrayTestTag.tabItemThumbnail)

/**
 * Obtains the three dot button in the Tabs Tray banner.
 */
private fun ComposeTestRule.threeDotButton() = onNodeWithTag(TabsTrayTestTag.threeDotButton)

/**
 * Obtains the dropdown menu item to access account settings.
 */
private fun ComposeTestRule.dropdownMenuItemAccountSettings() = onNodeWithTag(TabsTrayTestTag.accountSettings)

/**
 * Obtains the dropdown menu item to close all tabs.
 */
private fun ComposeTestRule.dropdownMenuItemCloseAllTabs() = onNodeWithTag(TabsTrayTestTag.closeAllTabs)

/**
 * Obtains the dropdown menu item to access recently closed tabs.
 */
private fun ComposeTestRule.dropdownMenuItemRecentlyClosedTabs() = onNodeWithTag(TabsTrayTestTag.recentlyClosedTabs)

/**
 * Obtains the dropdown menu item to select tabs.
 */
private fun ComposeTestRule.dropdownMenuItemSelectTabs() = onNodeWithTag(TabsTrayTestTag.selectTabs)

/**
 * Obtains the dropdown menu item to share all tabs.
 */
private fun ComposeTestRule.dropdownMenuItemShareAllTabs() = onNodeWithTag(TabsTrayTestTag.shareAllTabs)

/**
 * Obtains the dropdown menu item to access tab settings.
 */
private fun ComposeTestRule.dropdownMenuItemTabSettings() = onNodeWithTag(TabsTrayTestTag.tabSettings)

/**
 * Obtains the normal tabs counter.
 */
private fun ComposeTestRule.normalTabsCounter() = onNodeWithTag(TabsTrayTestTag.normalTabsCounter)

/**
 * Obtains the Tabs Tray banner collections button.
 */
private fun ComposeTestRule.collectionsButton() = onNodeWithTag(TabsTrayTestTag.collectionsButton)

/**
 * Obtains the Tabs Tray banner multi selection counter.
 */
private fun ComposeTestRule.multiSelectionCounter() = onNodeWithTag(TabsTrayTestTag.selectionCounter)

/**
 * Obtains the Tabs Tray banner handle.
 */
private fun ComposeTestRule.bannerHandle() = onNodeWithTag(TabsTrayTestTag.bannerHandle)

/**
 * Obtains the media control button with the given [action] as its content description.
 */
private fun ComposeTestRule.tabMediaControlButton(action: String) = onNodeWithContentDescription(action)
