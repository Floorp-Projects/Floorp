/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.net.Uri
import android.util.Log
import androidx.compose.ui.test.onNodeWithTag
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withChild
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withParent
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.Until
import org.hamcrest.Matchers.allOf
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.HomeActivityComposeTestRule
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull
import org.mozilla.fenix.tabstray.TabsTrayTestTag

/*
 * Implementation of Robot Pattern for the multiple selection toolbar of History and Bookmarks menus.
 */
class LibrarySubMenusMultipleSelectionToolbarRobot {

    fun verifyMultiSelectionCheckmark() {
        Log.i(TAG, "verifyMultiSelectionCheckmark: Trying to verify that the multi-selection checkmark is displayed")
        onView(withId(R.id.checkmark)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyMultiSelectionCheckmark: Verified that the multi-selection checkmark is displayed")
    }

    fun verifyMultiSelectionCheckmark(url: Uri) {
        Log.i(TAG, "verifyMultiSelectionCheckmark: Trying to verify that the multi-selection checkmark for item with url: $url is displayed")
        onView(
            allOf(
                withId(R.id.checkmark),
                withParent(
                    withParent(
                        withChild(
                            allOf(
                                withId(R.id.url),
                                withText(url.toString()),
                            ),
                        ),
                    ),
                ),

                // This is used as part of the `multiSelectionToolbarItemsTest` test. Somehow, in the view hierarchy,
                // the match above is finding two checkmark views - one visible, one hidden, which is throwing off
                // the matcher. This 'isDisplayed' check is a hacky workaround for this, we're explicitly ignoring
                // the hidden one. Why are there two to begin with, though?
                isDisplayed(),
            ),
        ).check(matches(isDisplayed()))
        Log.i(Constants.TAG, "verifyMultiSelectionCheckmark: Verified that the multi-selection checkmark for item with url: $url is displayed")
    }

    fun verifyMultiSelectionCounter() {
        Log.i(TAG, "verifyMultiSelectionCounter: Trying to verify that the multi-selection toolbar containing: \"1 selected\" is displayed")
        onView(withText("1 selected")).check(matches(isDisplayed()))
        Log.i(TAG, "verifyMultiSelectionCounter: Verified that the multi-selection toolbar containing: \"1 selected\" is displayed")
    }

    fun verifyShareHistoryButton() {
        Log.i(TAG, "verifyShareHistoryButton: Trying to verify that the multi-selection share history button is displayed")
        shareHistoryButton().check(matches(isDisplayed()))
        Log.i(TAG, "verifyShareHistoryButton: Verified that the multi-selection share history button is displayed")
    }

    fun verifyShareBookmarksButton() {
        Log.i(TAG, "verifyShareBookmarksButton: Trying to verify that the multi-selection share bookmarks button is displayed")
        shareBookmarksButton().check(matches(isDisplayed()))
        Log.i(TAG, "verifyShareBookmarksButton: Verified that the multi-selection share bookmarks button is displayed")
    }

    fun verifyShareOverlay() {
        Log.i(TAG, "verifyShareOverlay: Trying to verify that the share overlay is displayed")
        onView(withId(R.id.shareWrapper)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyShareOverlay: Verified that the share overlay is displayed")
    }

    fun verifyShareTabFavicon() {
        Log.i(TAG, "verifyShareTabFavicon: Trying to verify that the shared tab favicon is displayed")
        onView(withId(R.id.share_tab_favicon)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyShareTabFavicon: Verified that the shared tab favicon is displayed")
    }

    fun verifyShareTabTitle() {
        Log.i(TAG, "verifyShareTabTitle: Trying to verify that the shared tab title is displayed")
        onView(withId(R.id.share_tab_title)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyShareTabTitle: Verified that the shared tab title is displayed")
    }

    fun verifyShareTabUrl() {
        Log.i(TAG, "verifyShareTabUrl: Trying to verify that the shared tab url is displayed")
        onView(withId(R.id.share_tab_url)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyShareTabUrl: Verified that the shared tab url is displayed")
    }

    fun verifyCloseToolbarButton() {
        Log.i(TAG, "verifyCloseToolbarButton: Trying to verify that the navigate up toolbar button is displayed")
        closeToolbarButton().check(matches(isDisplayed()))
        Log.i(TAG, "verifyCloseToolbarButton: Verified that the navigate up toolbar button is displayed")
    }

    fun clickShareHistoryButton() {
        Log.i(TAG, "clickShareHistoryButton: Trying to click the multi-selection share history button")
        shareHistoryButton().click()
        Log.i(TAG, "clickShareHistoryButton: Clicked the multi-selection share history button")

        mDevice.waitNotNull(
            Until.findObject(
                By.text("ALL ACTIONS"),
            ),
            waitingTime,
        )
    }

    fun clickShareBookmarksButton() {
        Log.i(TAG, "clickShareBookmarksButton: Trying to click the multi-selection share bookmarks button")
        shareBookmarksButton().click()
        Log.i(TAG, "clickShareBookmarksButton: Clicked the multi-selection share bookmarks button")

        mDevice.waitNotNull(
            Until.findObject(
                By.text("ALL ACTIONS"),
            ),
            waitingTime,
        )
    }

    fun clickMultiSelectionDelete() {
        Log.i(TAG, "clickMultiSelectionDelete: Trying to click the multi-selection delete button")
        deleteButton().click()
        Log.i(TAG, "clickMultiSelectionDelete: Clicked the multi-selection delete button")
    }

    class Transition {
        fun closeToolbarReturnToHistory(interact: HistoryRobot.() -> Unit): HistoryRobot.Transition {
            Log.i(TAG, "closeToolbarReturnToHistory: Trying to click the navigate up toolbar button")
            closeToolbarButton().click()
            Log.i(TAG, "closeToolbarReturnToHistory: Clicked the navigate up toolbar button")

            HistoryRobot().interact()
            return HistoryRobot.Transition()
        }

        fun closeToolbarReturnToBookmarks(interact: BookmarksRobot.() -> Unit): BookmarksRobot.Transition {
            Log.i(TAG, "closeToolbarReturnToBookmarks: Trying to click the navigate up toolbar button")
            closeToolbarButton().click()
            Log.i(TAG, "closeToolbarReturnToBookmarks: Clicked the navigate up toolbar button")

            BookmarksRobot().interact()
            return BookmarksRobot.Transition()
        }

        fun clickOpenNewTab(interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            Log.i(TAG, "clickOpenNewTab: Trying to click the multi-select \"Open in a new tab\" context menu button")
            openInNewTabButton().click()
            Log.i(TAG, "clickOpenNewTab: Clicked the multi-select \"Open in a new tab\" context menu button")

            mDevice.waitNotNull(
                Until.findObject(By.res("$packageName:id/tab_layout")),
                waitingTime,
            )

            TabDrawerRobot().interact()
            return TabDrawerRobot.Transition()
        }

        fun clickOpenNewTab(composeTestRule: HomeActivityComposeTestRule, interact: ComposeTabDrawerRobot.() -> Unit): ComposeTabDrawerRobot.Transition {
            Log.i(TAG, "clickOpenNewTab: Trying to click the multi-select \"Open in a new tab\" context menu button")
            openInNewTabButton().click()
            Log.i(TAG, "clickOpenNewTab: Clicked the multi-select \"Open in a new tab\" context menu button")
            Log.i(TAG, "clickOpenNewTab: Trying to verify that the tabs tray exists")
            composeTestRule.onNodeWithTag(TabsTrayTestTag.tabsTray).assertExists()
            Log.i(TAG, "clickOpenNewTab: Verified that the tabs tray exists")

            ComposeTabDrawerRobot(composeTestRule).interact()
            return ComposeTabDrawerRobot.Transition(composeTestRule)
        }

        fun clickOpenPrivateTab(interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            Log.i(TAG, "clickOpenPrivateTab: Trying to click the multi-select \"Open in a private tab\" context menu button")
            openInPrivateTabButton().click()
            Log.i(TAG, "clickOpenPrivateTab: Clicked the multi-select \"Open in a private tab\" context menu button")
            mDevice.waitNotNull(
                Until.findObject(By.res("$packageName:id/tab_layout")),
                waitingTime,
            )

            TabDrawerRobot().interact()
            return TabDrawerRobot.Transition()
        }

        fun clickOpenPrivateTab(composeTestRule: HomeActivityComposeTestRule, interact: ComposeTabDrawerRobot.() -> Unit): ComposeTabDrawerRobot.Transition {
            Log.i(TAG, "clickOpenPrivateTab: Trying to click the multi-select \"Open in a private tab\" context menu button")
            openInPrivateTabButton().click()
            Log.i(TAG, "clickOpenPrivateTab: Clicked the multi-select \"Open in a private tab\" context menu button")

            ComposeTabDrawerRobot(composeTestRule).interact()
            return ComposeTabDrawerRobot.Transition(composeTestRule)
        }
    }
}

fun multipleSelectionToolbar(interact: LibrarySubMenusMultipleSelectionToolbarRobot.() -> Unit): LibrarySubMenusMultipleSelectionToolbarRobot.Transition {
    LibrarySubMenusMultipleSelectionToolbarRobot().interact()
    return LibrarySubMenusMultipleSelectionToolbarRobot.Transition()
}

private fun closeToolbarButton() = onView(withContentDescription("Navigate up"))

private fun shareHistoryButton() = onView(withId(R.id.share_history_multi_select))

private fun shareBookmarksButton() = onView(withId(R.id.share_bookmark_multi_select))

private fun openInNewTabButton() = onView(withText("Open in new tab"))

private fun openInPrivateTabButton() = onView(withText("Open in private tab"))

private fun deleteButton() = onView(withText("Delete"))
