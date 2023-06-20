/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.net.Uri
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
import org.mozilla.fenix.helpers.HomeActivityComposeTestRule
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the recently closed tabs menu.
 */

class RecentlyClosedTabsRobot {

    fun waitForListToExist() =
        mDevice.findObject(UiSelector().resourceId("$packageName:id/recently_closed_list"))
            .waitForExists(waitingTime)

    fun verifyRecentlyClosedTabsMenuView() {
        onView(
            allOf(
                withText("Recently closed tabs"),
                withParent(withId(R.id.navigationToolbar)),
            ),
        ).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }

    fun verifyEmptyRecentlyClosedTabsList() {
        mDevice.waitForIdle()

        onView(
            allOf(
                withId(R.id.recently_closed_empty_view),
                withText(R.string.recently_closed_empty_message),
            ),
        ).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }

    fun verifyRecentlyClosedTabsPageTitle(title: String) =
        recentlyClosedTabsPageTitle(title)
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    fun verifyRecentlyClosedTabsUrl(expectedUrl: Uri) {
        onView(
            allOf(
                withId(R.id.url),
                withEffectiveVisibility(
                    Visibility.VISIBLE,
                ),
            ),
        ).check(matches(withText(Matchers.containsString(expectedUrl.toString()))))
    }

    fun clickDeleteRecentlyClosedTabs() = recentlyClosedTabDeleteButton().click()

    class Transition {
        fun clickRecentlyClosedItem(title: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            recentlyClosedTabsPageTitle(title).click()
            mDevice.waitForIdle()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickOpenInNewTab(testRule: HomeActivityComposeTestRule, interact: ComposeTabDrawerRobot.() -> Unit): ComposeTabDrawerRobot.Transition {
            openInNewTabOption.click()

            ComposeTabDrawerRobot(testRule).interact()
            return ComposeTabDrawerRobot.Transition(testRule)
        }

        fun clickOpenInPrivateTab(testRule: HomeActivityComposeTestRule, interact: ComposeTabDrawerRobot.() -> Unit): ComposeTabDrawerRobot.Transition {
            openInPrivateTabOption.click()

            ComposeTabDrawerRobot(testRule).interact()
            return ComposeTabDrawerRobot.Transition(testRule)
        }

        fun clickShare(interact: ShareOverlayRobot.() -> Unit): ShareOverlayRobot.Transition {
            multipleSelectionShareButton.click()

            ShareOverlayRobot().interact()
            return ShareOverlayRobot.Transition()
        }

        fun goBackToHistoryMenu(interact: HistoryRobot.() -> Unit): HistoryRobot.Transition {
            onView(withContentDescription("Navigate up")).click()

            HistoryRobot().interact()
            return HistoryRobot.Transition()
        }
    }
}

private fun recentlyClosedTabsPageTitle(title: String) = onView(
    allOf(
        withId(R.id.title),
        withText(title),
    ),
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

private val openInNewTabOption = onView(withText("Open in new tab"))

private val openInPrivateTabOption = onView(withText("Open in private tab"))

private val multipleSelectionShareButton = onView(withId(R.id.share_history_multi_select))
