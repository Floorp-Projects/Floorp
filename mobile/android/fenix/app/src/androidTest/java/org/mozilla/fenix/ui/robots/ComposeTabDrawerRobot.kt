/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import androidx.compose.ui.test.assertIsNotSelected
import androidx.compose.ui.test.assertIsSelected
import androidx.compose.ui.test.junit4.ComposeTestRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import org.mozilla.fenix.helpers.HomeActivityComposeTestRule
import org.mozilla.fenix.helpers.TestHelper.mDevice
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

    fun verifyExistingOpenTabs(vararg titles: String) {
        titles.forEach { title ->
            tabItem(title).assertExists()
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

    /**
     * Closes a tab when there is only one tab open.
     */
    fun closeTab() {
        composeTestRule.closeTabButton().performClick()
    }

    /**
     * Obtains the tab with the provided [title]
     */
    private fun tabItem(title: String) = composeTestRule.onNodeWithText(title)

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
    }
}

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
 * Obtains an open tab's close button when there's only one tab open.
 */
private fun ComposeTestRule.closeTabButton() = onNodeWithTag(TabsTrayTestTag.tabItemClose)

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
