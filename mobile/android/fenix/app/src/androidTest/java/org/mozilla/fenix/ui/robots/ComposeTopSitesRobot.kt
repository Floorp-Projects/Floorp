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
        Log.i(TAG, "verifyExistingTopSitesList: Waiting for $waitingTime ms until the top sites list exists")
        composeTestRule.waitUntilAtLeastOneExists(hasTestTag(TopSitesTestTag.topSites), timeoutMillis = waitingTime)
        Log.i(TAG, "verifyExistingTopSitesList: Waited for $waitingTime ms until the top sites list to exists")
    }

    @OptIn(ExperimentalTestApi::class)
    fun verifyExistingTopSiteItem(vararg titles: String) {
        titles.forEach { title ->
            Log.i(TAG, "verifyExistingTopSiteItem: Waiting for $waitingTime ms until the top site with title: $title exists")
            composeTestRule.waitUntilAtLeastOneExists(hasText(title), timeoutMillis = waitingTime)
            Log.i(TAG, "verifyExistingTopSiteItem: Waited for $waitingTime ms until the top site with title: $title exists")
            Log.i(TAG, "verifyExistingTopSiteItem: Trying to verify that the top site with title: $title exists")
            composeTestRule.topSiteItem(title).assertExists()
            Log.i(TAG, "verifyExistingTopSiteItem: Verified that the top site with title: $title exists")
        }
    }

    fun verifyNotExistingTopSiteItem(vararg titles: String) {
        titles.forEach { title ->
            Log.i(TAG, "verifyNotExistingTopSiteItem: Waiting for $waitingTime ms for top site with title: $title to exist")
            itemContainingText(title).waitForExists(waitingTime)
            Log.i(TAG, "verifyNotExistingTopSiteItem: Waited for $waitingTime ms for top site with title: $title to exist")
            Log.i(TAG, "verifyNotExistingTopSiteItem: Trying to verify that top site with title: $title does not exist")
            composeTestRule.topSiteItem(title).assertDoesNotExist()
            Log.i(TAG, "verifyNotExistingTopSiteItem: Verified that top site with title: $title does not exist")
        }
    }

    fun verifyTopSiteContextMenuItems() {
        verifyTopSiteContextMenuOpenInPrivateTabButton()
        verifyTopSiteContextMenuRemoveButton()
        verifyTopSiteContextMenuRenameButton()
    }

    fun verifyTopSiteContextMenuOpenInPrivateTabButton() {
        Log.i(TAG, "verifyTopSiteContextMenuOpenInPrivateTabButton: Trying to verify that the \"Open in private tab\" menu button exists")
        composeTestRule.contextMenuItemOpenInPrivateTab().assertExists()
        Log.i(TAG, "verifyTopSiteContextMenuOpenInPrivateTabButton: Verified that the \"Open in private tab\" menu button exists")
    }

    fun verifyTopSiteContextMenuRenameButton() {
        Log.i(TAG, "verifyTopSiteContextMenuRenameButton: Trying to verify that the \"Rename\" menu button exists")
        composeTestRule.contextMenuItemRename().assertExists()
        Log.i(TAG, "verifyTopSiteContextMenuRenameButton: Verified that the \"Rename\" menu button exists")
    }

    fun verifyTopSiteContextMenuRemoveButton() {
        Log.i(TAG, "verifyTopSiteContextMenuRemoveButton: Trying to verify that the \"Remove\" menu button exists")
        composeTestRule.contextMenuItemRemove().assertExists()
        Log.i(TAG, "verifyTopSiteContextMenuRemoveButton: Verified that the \"Remove\" menu button exists")
    }

    class Transition(private val composeTestRule: HomeActivityComposeTestRule) {

        fun openTopSiteTabWithTitle(
            title: String,
            interact: BrowserRobot.() -> Unit,
        ): BrowserRobot.Transition {
            Log.i(TAG, "openTopSiteTabWithTitle: Trying to scroll to top site with title: $title")
            composeTestRule.topSiteItem(title).performScrollTo()
            Log.i(TAG, "openTopSiteTabWithTitle: Scrolled to top site with title: $title")
            Log.i(TAG, "openTopSiteTabWithTitle: Trying to click top site with title: $title")
            composeTestRule.topSiteItem(title).performClick()
            Log.i(TAG, "openTopSiteTabWithTitle: Clicked top site with title: $title")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openTopSiteInPrivate(
            interact: BrowserRobot.() -> Unit,
        ): BrowserRobot.Transition {
            Log.i(TAG, "openTopSiteInPrivate: Trying to click the \"Open in private tab\" menu button")
            composeTestRule.contextMenuItemOpenInPrivateTab().performClick()
            Log.i(TAG, "openTopSiteInPrivate: Clicked the \"Open in private tab\" menu button")
            composeTestRule.waitForIdle()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openContextMenuOnTopSitesWithTitle(
            title: String,
            interact: ComposeTopSitesRobot.() -> Unit,
        ): Transition {
            Log.i(TAG, "openContextMenuOnTopSitesWithTitle: Trying to scroll to top site with title: $title")
            composeTestRule.topSiteItem(title).performScrollTo()
            Log.i(TAG, "openContextMenuOnTopSitesWithTitle: Scrolled to top site with title: $title")
            Log.i(TAG, "openContextMenuOnTopSitesWithTitle: Trying to long click top site with title: $title")
            composeTestRule.topSiteItem(title).performTouchInput { longClick() }
            Log.i(TAG, "openContextMenuOnTopSitesWithTitle: Long clicked top site with title: $title")

            ComposeTopSitesRobot(composeTestRule).interact()
            return Transition(composeTestRule)
        }

        fun renameTopSite(
            title: String,
            interact: ComposeTopSitesRobot.() -> Unit,
        ): Transition {
            Log.i(TAG, "renameTopSite: Trying to click the \"Rename\" menu button")
            composeTestRule.contextMenuItemRename().performClick()
            Log.i(TAG, "renameTopSite: Clicked the \"Rename\" menu button")
            itemWithResId("$packageName:id/top_site_title")
                .also {
                    Log.i(TAG, "renameTopSite: Waiting for $waitingTimeShort ms for top site rename text box to exist")
                    it.waitForExists(waitingTimeShort)
                    Log.i(TAG, "renameTopSite: Waited for $waitingTimeShort ms for top site rename text box to exist")
                    Log.i(TAG, "renameTopSite: Trying to set top site rename text box text to: $title")
                    it.setText(title)
                    Log.i(TAG, "renameTopSite: Top site rename text box text was set to: $title")
                }
            Log.i(TAG, "renameTopSite: Trying to click the \"Ok\" dialog button")
            itemWithResIdContainingText("android:id/button1", "OK").click()
            Log.i(TAG, "renameTopSite: Clicked the \"Ok\" dialog button")

            ComposeTopSitesRobot(composeTestRule).interact()
            return Transition(composeTestRule)
        }

        @OptIn(ExperimentalTestApi::class)
        fun removeTopSite(
            interact: ComposeTopSitesRobot.() -> Unit,
        ): Transition {
            Log.i(TAG, "removeTopSite: Trying to click the \"Remove\" menu button")
            composeTestRule.contextMenuItemRemove().performClick()
            Log.i(TAG, "removeTopSite: Clicked the \"Remove\" menu button")
            Log.i(TAG, "removeTopSite: Waiting for $waitingTime ms until the \"Remove\" menu button does not exist")
            composeTestRule.waitUntilDoesNotExist(hasTestTag(TopSitesTestTag.remove), waitingTime)
            Log.i(TAG, "removeTopSite: Waited for $waitingTime ms until the \"Remove\" menu button does not exist")

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
