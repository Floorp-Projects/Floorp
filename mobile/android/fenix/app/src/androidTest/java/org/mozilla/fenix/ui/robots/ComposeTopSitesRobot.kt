/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.util.Log
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.filter
import androidx.compose.ui.test.hasAnyChild
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.hasText
import androidx.compose.ui.test.junit4.ComposeTestRule
import androidx.compose.ui.test.longClick
import androidx.compose.ui.test.onAllNodesWithTag
import androidx.compose.ui.test.onFirst
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.performTouchInput
import org.mozilla.fenix.helpers.Constants.TAG
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

    @OptIn(ExperimentalTestApi::class)
    fun verifyExistingTopSitesList() {
        Log.i(TAG, "verifyExistingTopSitesList: Waiting for top sites list to exist")
        composeTestRule.waitUntilAtLeastOneExists(hasTestTag(TopSitesTestTag.topSites), timeoutMillis = waitingTime)
    }

    @OptIn(ExperimentalTestApi::class)
    fun verifyExistingTopSiteItem(vararg titles: String) {
        titles.forEach { title ->
            Log.i(TAG, "verifyExistingTopSiteItem: Waiting for top site: $title to exist")
            composeTestRule.waitUntilAtLeastOneExists(hasText(title), timeoutMillis = waitingTime)
            composeTestRule.topSiteItem(title).assertExists()
            Log.i(TAG, "verifyExistingTopSiteItem: Top site with $title exists")
        }
    }

    fun verifyNotExistingTopSiteItem(vararg titles: String) {
        titles.forEach { title ->
            itemContainingText(title).waitForExists(waitingTime)
            composeTestRule.topSiteItem(title).assertDoesNotExist()
            Log.i(TAG, "verifyNotExistingTopSiteItem: Top site with $title does not exist")
        }
    }

    fun verifyTopSiteContextMenuItems() {
        verifyTopSiteContextMenuOpenInPrivateTabButton()
        verifyTopSiteContextMenuRemoveButton()
        verifyTopSiteContextMenuRenameButton()
    }

    fun verifyTopSiteContextMenuOpenInPrivateTabButton() {
        composeTestRule.contextMenuItemOpenInPrivateTab().assertExists()
        Log.i(TAG, "verifyTopSiteContextMenuOpenInPrivateTabButton: Verified \"Open in private tab\" menu button exists")
    }

    fun verifyTopSiteContextMenuRenameButton() {
        composeTestRule.contextMenuItemRename().assertExists()
        Log.i(TAG, "verifyTopSiteContextMenuRenameButton: Verified \"Rename\" menu button exists")
    }

    fun verifyTopSiteContextMenuRemoveButton() {
        composeTestRule.contextMenuItemRemove().assertExists()
        Log.i(TAG, "verifyTopSiteContextMenuRemoveButton: Verified \"Remove\" menu button exists")
    }

    class Transition(private val composeTestRule: HomeActivityComposeTestRule) {

        fun openTopSiteTabWithTitle(
            title: String,
            interact: BrowserRobot.() -> Unit,
        ): BrowserRobot.Transition {
            composeTestRule.topSiteItem(title).performScrollTo().performClick()
            Log.i(TAG, "openTopSiteTabWithTitle: Scrolled and clicked top site with title: $title")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openTopSiteInPrivate(
            interact: BrowserRobot.() -> Unit,
        ): BrowserRobot.Transition {
            composeTestRule.contextMenuItemOpenInPrivateTab().performClick()
            Log.i(TAG, "openTopSiteInPrivate: Clicked \"Open in private tab\" menu button")
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
            Log.i(TAG, "openContextMenuOnTopSitesWithTitle: Long clicked top site with title: $title")

            ComposeTopSitesRobot(composeTestRule).interact()
            return Transition(composeTestRule)
        }

        fun renameTopSite(
            title: String,
            interact: ComposeTopSitesRobot.() -> Unit,
        ): Transition {
            composeTestRule.contextMenuItemRename().performClick()
            Log.i(TAG, "renameTopSite: Clicked \"Rename\" menu button")
            itemWithResId("$packageName:id/top_site_title")
                .also {
                    it.waitForExists(waitingTimeShort)
                    it.setText(title)
                }
            Log.i(TAG, "renameTopSite: Top site rename text box was set to: $title")
            itemWithResIdContainingText("android:id/button1", "OK").click()
            Log.i(TAG, "renameTopSite: Clicked \"Ok\" dialog button")

            ComposeTopSitesRobot(composeTestRule).interact()
            return Transition(composeTestRule)
        }

        @OptIn(ExperimentalTestApi::class)
        fun removeTopSite(
            interact: ComposeTopSitesRobot.() -> Unit,
        ): Transition {
            composeTestRule.contextMenuItemRemove().performClick()
            Log.i(TAG, "removeTopSite: Clicked \"Remove\" menu button")
            composeTestRule.waitUntilDoesNotExist(hasTestTag(TopSitesTestTag.remove), waitingTime)
            Log.i(TAG, "removeTopSite: Waited for \"Remove\" menu button to not exist")

            ComposeTopSitesRobot(composeTestRule).interact()
            return Transition(composeTestRule)
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
