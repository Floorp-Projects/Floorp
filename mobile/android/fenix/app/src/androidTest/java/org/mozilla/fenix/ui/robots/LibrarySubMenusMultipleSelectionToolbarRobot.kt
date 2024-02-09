/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.net.Uri
import android.widget.TextView
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
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.hamcrest.Matchers.allOf
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.HomeActivityComposeTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
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

    fun verifyMultiSelectionCheckmark() = onView(withId(R.id.checkmark)).check(matches(isDisplayed()))

    fun verifyMultiSelectionCheckmark(url: Uri) =
        onView(
            allOf(
                withId(R.id.checkmark),
                withParent(withParent(withChild(allOf(withId(R.id.url), withText(url.toString()))))),

                // This is used as part of the `multiSelectionToolbarItemsTest` test. Somehow, in the view hierarchy,
                // the match above is finding two checkmark views - one visible, one hidden, which is throwing off
                // the matcher. This 'isDisplayed' check is a hacky workaround for this, we're explicitly ignoring
                // the hidden one. Why are there two to begin with, though?
                isDisplayed(),
            ),
        ).check(matches(isDisplayed()))

    fun verifyMultiSelectionCounter() = onView(withText("1 selected")).check(matches(isDisplayed()))

    fun verifyShareHistoryButton() = shareHistoryButton().check(matches(isDisplayed()))

    fun verifyShareBookmarksButton() = shareBookmarksButton().check(matches(isDisplayed()))

    fun verifyShareOverlay() = onView(withId(R.id.shareWrapper)).check(matches(isDisplayed()))

    fun verifyShareAppsLayout() {
        val sendToDeviceTitle = mDevice.findObject(
            UiSelector()
                .instance(0)
                .className(TextView::class.java),
        )
        sendToDeviceTitle.waitForExists(TestAssetHelper.waitingTime)
    }

    fun verifyShareTabFavicon() = onView(withId(R.id.share_tab_favicon)).check(matches(isDisplayed()))

    fun verifyShareTabTitle() = onView(withId(R.id.share_tab_title)).check(matches(isDisplayed()))

    fun verifyShareTabUrl() = onView(withId(R.id.share_tab_url))

    fun verifyCloseToolbarButton() = closeToolbarButton().check(matches(isDisplayed()))

    fun clickShareHistoryButton() {
        shareHistoryButton().click()

        mDevice.waitNotNull(
            Until.findObject(
                By.text("ALL ACTIONS"),
            ),
            waitingTime,
        )
    }

    fun clickShareBookmarksButton() {
        shareBookmarksButton().click()

        mDevice.waitNotNull(
            Until.findObject(
                By.text("ALL ACTIONS"),
            ),
            waitingTime,
        )
    }

    fun clickMultiSelectionDelete() {
        deleteButton().click()
    }

    class Transition {
        fun closeShareDialogReturnToPage(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun closeToolbarReturnToHistory(interact: HistoryRobot.() -> Unit): HistoryRobot.Transition {
            closeToolbarButton().click()

            HistoryRobot().interact()
            return HistoryRobot.Transition()
        }

        fun closeToolbarReturnToBookmarks(interact: BookmarksRobot.() -> Unit): BookmarksRobot.Transition {
            closeToolbarButton().click()

            BookmarksRobot().interact()
            return BookmarksRobot.Transition()
        }

        fun clickOpenNewTab(interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            openInNewTabButton().click()
            mDevice.waitNotNull(
                Until.findObject(By.res("$packageName:id/tab_layout")),
                waitingTime,
            )

            TabDrawerRobot().interact()
            return TabDrawerRobot.Transition()
        }

        fun clickOpenNewTab(composeTestRule: HomeActivityComposeTestRule, interact: ComposeTabDrawerRobot.() -> Unit): ComposeTabDrawerRobot.Transition {
            openInNewTabButton().click()
            composeTestRule.onNodeWithTag(TabsTrayTestTag.tabsTray).assertExists()

            ComposeTabDrawerRobot(composeTestRule).interact()
            return ComposeTabDrawerRobot.Transition(composeTestRule)
        }

        fun clickOpenPrivateTab(interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            openInPrivateTabButton().click()
            mDevice.waitNotNull(
                Until.findObject(By.res("$packageName:id/tab_layout")),
                waitingTime,
            )

            TabDrawerRobot().interact()
            return TabDrawerRobot.Transition()
        }

        fun clickOpenPrivateTab(composeTestRule: HomeActivityComposeTestRule, interact: ComposeTabDrawerRobot.() -> Unit): ComposeTabDrawerRobot.Transition {
            openInPrivateTabButton().click()

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
