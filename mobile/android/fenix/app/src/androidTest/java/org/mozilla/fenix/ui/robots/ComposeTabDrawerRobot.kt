/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.view.View
import androidx.compose.ui.test.assert
import androidx.compose.ui.test.assertIsNotSelected
import androidx.compose.ui.test.assertIsSelected
import androidx.compose.ui.test.hasAnyChild
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.junit4.ComposeTestRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.test.espresso.Espresso
import androidx.test.espresso.UiController
import androidx.test.espresso.ViewAction
import androidx.test.espresso.action.GeneralLocation
import androidx.test.espresso.assertion.ViewAssertions
import androidx.test.espresso.matcher.ViewMatchers
import com.google.android.material.bottomsheet.BottomSheetBehavior
import org.hamcrest.Matcher
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.HomeActivityComposeTestRule
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
        } else {
            composeTestRule.normalBrowsingButton().assertIsNotSelected()
        }
    }

    fun verifyPrivateBrowsingButtonIsSelected(isSelected: Boolean = true) {
        if (isSelected) {
            composeTestRule.privateBrowsingButton().assertIsSelected()
        } else {
            composeTestRule.privateBrowsingButton().assertIsNotSelected()
        }
    }

    fun verifySyncedTabsButtonIsSelected(isSelected: Boolean = true) {
        if (isSelected) {
            composeTestRule.syncedTabsButton().assertIsSelected()
        } else {
            composeTestRule.syncedTabsButton().assertIsNotSelected()
        }
    }

    fun verifyExistingOpenTabs(vararg titles: String) {
        titles.forEach { title ->
            composeTestRule.tabItem(title).assertExists()
        }
    }

    fun verifyNormalTabsList() {
        composeTestRule.normalTabsList().assertExists()
    }

    fun verifyPrivateTabsList() {
        composeTestRule.privateTabsList().assertExists()
    }

    fun verifySyncedTabsList() {
        composeTestRule.syncedTabsList().assertExists()
    }

    fun verifyNoOpenTabsInNormalBrowsing() {
        composeTestRule.emptyNormalTabsList().assertExists()
    }

    fun verifyNoOpenTabsInPrivateBrowsing() {
        composeTestRule.emptyPrivateTabsList().assertExists()
    }

    fun verifyAccountSettingsButton() {
        composeTestRule.dropdownMenuItemAccountSettings().assertExists()
    }

    fun verifyCloseAllTabsButton() {
        composeTestRule.dropdownMenuItemCloseAllTabs().assertExists()
    }

    fun verifySelectTabsButton() {
        composeTestRule.dropdownMenuItemSelectTabs().assertExists()
    }

    fun verifyShareAllTabsButton() {
        composeTestRule.dropdownMenuItemShareAllTabs().assertExists()
    }

    fun verifyRecentlyClosedTabsButton() {
        composeTestRule.dropdownMenuItemRecentlyClosedTabs().assertExists()
    }

    fun verifyTabSettingsButton() {
        composeTestRule.dropdownMenuItemTabSettings().assertExists()
    }

    fun verifyThreeDotButton() {
        composeTestRule.threeDotButton().assertExists()
    }

    fun verifyFab() {
        composeTestRule.tabsTrayFab().assertExists()
    }

    fun verifyNormalTabCounter() {
        composeTestRule.normalTabsCounter().assertExists()
    }

    /**
     * Verifies a tab's thumbnail when there is only one tab open.
     */
    fun verifyTabThumbnail() {
        composeTestRule.tabThumbnail().assertExists()
    }

    /**
     * Verifies a tab with [title] has a close button.
     */
    fun verifyTabCloseButton(title: String) {
        composeTestRule.tabItem(title).assert(
            hasAnyChild(
                hasTestTag(TabsTrayTestTag.tabItemClose),
            ),
        )
    }

    fun verifyTabsTrayBehaviorState(expectedState: Int) {
        tabsTrayView().check(ViewAssertions.matches(BottomSheetBehaviorStateMatcher(expectedState)))
    }

    fun verifyMinusculeHalfExpandedRatio() {
        tabsTrayView().check(ViewAssertions.matches(BottomSheetBehaviorHalfExpandedMaxRatioMatcher(0.001f)))
    }

    fun verifyTabTrayIsClosed() {
        composeTestRule.tabsTray().assertDoesNotExist()
    }

    /**
     * Closes a tab when there is only one tab open.
     */
    fun closeTab() {
        composeTestRule.closeTabButton().performClick()
    }

    class Transition(private val composeTestRule: HomeActivityComposeTestRule) {

        fun openNewTab(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            mDevice.waitForIdle()

            composeTestRule.tabsTrayFab().performClick()
            SearchRobot().interact()
            return SearchRobot.Transition()
        }

        fun toggleToNormalTabs(interact: ComposeTabDrawerRobot.() -> Unit): Transition {
            composeTestRule.normalBrowsingButton().performClick()
            ComposeTabDrawerRobot(composeTestRule).interact()
            return Transition(composeTestRule)
        }

        fun toggleToPrivateTabs(interact: ComposeTabDrawerRobot.() -> Unit): Transition {
            composeTestRule.privateBrowsingButton().performClick()
            ComposeTabDrawerRobot(composeTestRule).interact()
            return Transition(composeTestRule)
        }

        fun openThreeDotMenu(interact: ComposeTabDrawerRobot.() -> Unit): Transition {
            composeTestRule.threeDotButton().performClick()
            ComposeTabDrawerRobot(composeTestRule).interact()
            return Transition(composeTestRule)
        }

        fun closeAllTabs(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            composeTestRule.dropdownMenuItemCloseAllTabs().performClick()
            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openTab(title: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            composeTestRule.tabItem(title)
                .performScrollTo()
                .performClick()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickTopBar(interact: ComposeTabDrawerRobot.() -> Unit): Transition {
            // The topBar contains other views.
            // Don't do the default click in the middle, rather click in some free space - top right.
            Espresso.onView(ViewMatchers.withId(R.id.topBar)).clickAtLocationInView(GeneralLocation.TOP_RIGHT)
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
    }
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
private fun ComposeTestRule.tabItem(title: String) = onNodeWithText(title)

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
