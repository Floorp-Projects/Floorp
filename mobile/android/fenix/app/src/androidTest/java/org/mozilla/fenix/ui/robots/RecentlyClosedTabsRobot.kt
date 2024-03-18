/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.net.Uri
import android.util.Log
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withParent
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.UiSelector
import org.hamcrest.Matchers
import org.hamcrest.Matchers.allOf
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.HomeActivityComposeTestRule
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdContainingText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the recently closed tabs menu.
 */

class RecentlyClosedTabsRobot {

    fun waitForListToExist() {
        Log.i(TAG, "waitForListToExist: Waiting for $waitingTime ms for recently closed tabs list to exist")
        mDevice.findObject(UiSelector().resourceId("$packageName:id/recently_closed_list"))
            .waitForExists(waitingTime)
        Log.i(TAG, "waitForListToExist: Waited for $waitingTime ms for recently closed tabs list to exist")
    }

    fun verifyRecentlyClosedTabsMenuView() {
        Log.i(TAG, "verifyRecentlyClosedTabsMenuView: Trying to verify that the recently closed tabs menu view is visible")
        onView(
            allOf(
                withText("Recently closed tabs"),
                withParent(withId(R.id.navigationToolbar)),
            ),
        ).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifyRecentlyClosedTabsMenuView: Verified that the recently closed tabs menu view is visible")
    }

    fun verifyEmptyRecentlyClosedTabsList() {
        Log.i(TAG, "verifyEmptyRecentlyClosedTabsList: Waiting for device to be idle")
        mDevice.waitForIdle()
        Log.i(TAG, "verifyEmptyRecentlyClosedTabsList: Waited for device to be idle")
        Log.i(TAG, "verifyEmptyRecentlyClosedTabsList: Trying to verify that the empty recently closed tabs list is visible")
        onView(
            allOf(
                withId(R.id.recently_closed_empty_view),
                withText(R.string.recently_closed_empty_message),
            ),
        ).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifyEmptyRecentlyClosedTabsList: Verified that the empty recently closed tabs list is visible")
    }

    fun verifyRecentlyClosedTabsPageTitle(title: String) =
        assertUIObjectExists(
            recentlyClosedTabsPageTitle(title),
        )

    fun verifyRecentlyClosedTabsUrl(expectedUrl: Uri) {
        Log.i(TAG, "verifyRecentlyClosedTabsUrl: Trying to verify that the recently closed tab with url: $expectedUrl is visible")
        onView(
            allOf(
                withId(R.id.url),
                withEffectiveVisibility(
                    Visibility.VISIBLE,
                ),
            ),
        ).check(matches(withText(Matchers.containsString(expectedUrl.toString()))))
        Log.i(TAG, "verifyRecentlyClosedTabsUrl: Verified that the recently closed tab with url: $expectedUrl is visible")
    }

    fun clickDeleteRecentlyClosedTabs() {
        Log.i(TAG, "clickDeleteRecentlyClosedTabs: Trying to click the recently closed tab item delete button")
        recentlyClosedTabDeleteButton().click()
        Log.i(TAG, "clickDeleteRecentlyClosedTabs: Clicked the recently closed tab item delete button")
    }

    class Transition {
        fun clickRecentlyClosedItem(title: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            recentlyClosedTabsPageTitle(title).also {
                Log.i(TAG, "clickRecentlyClosedItem: Waiting for $waitingTimeShort ms for recently closed tab with title: $title to exist")
                it.waitForExists(waitingTimeShort)
                Log.i(TAG, "clickRecentlyClosedItem: Waited for $waitingTimeShort ms for recently closed tab with title: $title to exist")
                Log.i(TAG, "clickRecentlyClosedItem: Trying to click the recently closed tab with title: $title")
                it.click()
                Log.i(TAG, "clickRecentlyClosedItem: Clicked the recently closed tab with title: $title")
            }
            Log.i(TAG, "clickRecentlyClosedItem: Waiting for device to be idle")
            mDevice.waitForIdle()
            Log.i(TAG, "clickRecentlyClosedItem: Waited for device to be idle")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickOpenInNewTab(testRule: HomeActivityComposeTestRule, interact: ComposeTabDrawerRobot.() -> Unit): ComposeTabDrawerRobot.Transition {
            Log.i(TAG, "clickOpenInNewTab: Trying to click the multi-select \"Open in a new tab\" context menu button")
            openInNewTabOption().click()
            Log.i(TAG, "clickOpenInNewTab: Clicked the multi-select \"Open in a new tab\" context menu button")

            ComposeTabDrawerRobot(testRule).interact()
            return ComposeTabDrawerRobot.Transition(testRule)
        }

        fun clickOpenInPrivateTab(testRule: HomeActivityComposeTestRule, interact: ComposeTabDrawerRobot.() -> Unit): ComposeTabDrawerRobot.Transition {
            Log.i(TAG, "clickOpenInPrivateTab: Trying to click the multi-select \"Open in a private tab\" context menu button")
            openInPrivateTabOption().click()
            Log.i(TAG, "clickOpenInPrivateTab: Clicked the multi-select \"Open in a private tab\" context menu button")

            ComposeTabDrawerRobot(testRule).interact()
            return ComposeTabDrawerRobot.Transition(testRule)
        }

        fun clickShare(interact: ShareOverlayRobot.() -> Unit): ShareOverlayRobot.Transition {
            Log.i(TAG, "clickShare: Trying to click the share recently closed tabs button")
            multipleSelectionShareButton().click()
            Log.i(TAG, "clickShare: Clicked the share recently closed tabs button")

            ShareOverlayRobot().interact()
            return ShareOverlayRobot.Transition()
        }

        fun goBackToHistoryMenu(interact: HistoryRobot.() -> Unit): HistoryRobot.Transition {
            Log.i(TAG, "goBackToHistoryMenu: Trying to click navigate up toolbar button")
            onView(withContentDescription("Navigate up")).click()
            Log.i(TAG, "goBackToHistoryMenu: Clicked navigate up toolbar button")

            HistoryRobot().interact()
            return HistoryRobot.Transition()
        }
    }
}

private fun recentlyClosedTabsPageTitle(title: String) =
    itemWithResIdContainingText(
        resourceId = "$packageName:id/title",
        text = title,
    )

private fun recentlyClosedTabDeleteButton() =
    onView(
        allOf(
            withId(R.id.overflow_menu),
            withEffectiveVisibility(
                Visibility.VISIBLE,
            ),
        ),
    )

private fun openInNewTabOption() = onView(withText("Open in new tab"))

private fun openInPrivateTabOption() = onView(withText("Open in private tab"))

private fun multipleSelectionShareButton() = onView(withId(R.id.share_history_multi_select))
