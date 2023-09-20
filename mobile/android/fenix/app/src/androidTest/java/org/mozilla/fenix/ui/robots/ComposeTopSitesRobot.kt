/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.filter
import androidx.compose.ui.test.hasAnyChild
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.hasText
import androidx.compose.ui.test.junit4.ComposeTestRule
import androidx.compose.ui.test.longClick
import androidx.compose.ui.test.onAllNodesWithTag
import androidx.compose.ui.test.onFirst
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.performTouchInput
import org.mozilla.fenix.helpers.HomeActivityComposeTestRule
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdContainingText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.home.topsites.TopSitesTestTag

/**
 * Implementation of Robot Pattern for the Compose Top Sites.
 */
class ComposeTopSitesRobot(private val composeTestRule: HomeActivityComposeTestRule) {

    fun verifyExistingTopSitesList() =
        composeTestRule.onNodeWithTag(TopSitesTestTag.topSites).assertExists()

    @OptIn(ExperimentalTestApi::class)
    fun verifyExistingTopSiteItem(vararg titles: String) {
        titles.forEach { title ->
            composeTestRule.waitUntilAtLeastOneExists(hasText(title), waitingTime)
            composeTestRule.topSiteItem(title).assertExists()
        }
    }

    fun verifyNotExistingTopSiteItem(vararg titles: String) {
        titles.forEach { title ->
            itemContainingText(title).waitForExists(waitingTime)
            composeTestRule.topSiteItem(title).assertDoesNotExist()
        }
    }

    fun verifyTopSiteContextMenuItems() {
        verifyTopSiteContextMenuOpenInPrivateTabButton()
        verifyTopSiteContextMenuRemoveButton()
        verifyTopSiteContextMenuRenameButton()
    }

    fun verifyTopSiteContextMenuOpenInPrivateTabButton() {
        composeTestRule.contextMenuItemOpenInPrivateTab().assertExists()
    }

    fun verifyTopSiteContextMenuRenameButton() {
        composeTestRule.contextMenuItemRename().assertExists()
    }

    fun verifyTopSiteContextMenuRemoveButton() {
        composeTestRule.contextMenuItemRemove().assertExists()
    }

    class Transition(private val composeTestRule: HomeActivityComposeTestRule) {

        fun openTopSiteTabWithTitle(
            title: String,
            interact: BrowserRobot.() -> Unit,
        ): BrowserRobot.Transition {
            composeTestRule.topSiteItem(title).performScrollTo().performClick()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openTopSiteInPrivate(
            interact: BrowserRobot.() -> Unit,
        ): BrowserRobot.Transition {
            composeTestRule.contextMenuItemOpenInPrivateTab().performClick()
            composeTestRule.waitForIdle()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openContextMenuOnTopSitesWithTitle(
            title: String,
            interact: ComposeTopSitesRobot.() -> Unit,
        ): Transition {
            composeTestRule.topSiteItem(title).performScrollTo().performTouchInput {
                longClick()
            }

            ComposeTopSitesRobot(composeTestRule).interact()
            return Transition(composeTestRule)
        }

        fun renameTopSite(
            title: String,
            interact: ComposeTopSitesRobot.() -> Unit,
        ): Transition {
            composeTestRule.contextMenuItemRename().performClick()
            itemWithResId("$packageName:id/top_site_title")
                .also {
                    it.waitForExists(waitingTimeShort)
                    it.setText(title)
                }
            itemWithResIdContainingText("android:id/button1", "OK").click()

            ComposeTopSitesRobot(composeTestRule).interact()
            return Transition(composeTestRule)
        }

        @OptIn(ExperimentalTestApi::class)
        fun removeTopSite(
            interact: ComposeTopSitesRobot.() -> Unit,
        ): Transition {
            composeTestRule.contextMenuItemRemove().performClick()
            composeTestRule.waitUntilDoesNotExist(hasTestTag(TopSitesTestTag.remove), waitingTime)

            ComposeTopSitesRobot(composeTestRule).interact()
            return Transition(composeTestRule)
        }

        @OptIn(ExperimentalTestApi::class)
        fun deleteTopSiteFromHistory(
            interact: BrowserRobot.() -> Unit,
        ): BrowserRobot.Transition {
            composeTestRule.contextMenuItemRemove().performClick()
            composeTestRule.waitUntilDoesNotExist(hasTestTag(TopSitesTestTag.remove), waitingTime)

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

/**
 * Obtains the top site with the provided [title].
 */
private fun ComposeTestRule.topSiteItem(title: String) =
    onAllNodesWithTag(TopSitesTestTag.topSiteItemRoot).filter(hasAnyChild(hasText(title))).onFirst()

/**
 * Obtains the option to open in private tab the top site
 */
private fun ComposeTestRule.contextMenuItemOpenInPrivateTab() =
    onAllNodesWithTag(TopSitesTestTag.openInPrivateTab).onFirst()

/**
 * Obtains the option to rename the top site
 */
private fun ComposeTestRule.contextMenuItemRename() = onAllNodesWithTag(TopSitesTestTag.rename).onFirst()

/**
 * Obtains the option to remove the top site
 */
private fun ComposeTestRule.contextMenuItemRemove() = onAllNodesWithTag(TopSitesTestTag.remove).onFirst()
