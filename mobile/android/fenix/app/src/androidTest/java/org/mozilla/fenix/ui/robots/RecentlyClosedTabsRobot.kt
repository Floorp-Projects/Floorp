/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.net.Uri
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withParent
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.UiSelector
import org.hamcrest.Matchers
import org.hamcrest.Matchers.allOf
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the recently closed tabs menu.
 */

class RecentlyClosedTabsRobot {

    fun waitForListToExist() =
        mDevice.findObject(UiSelector().resourceId("$packageName:id/recently_closed_list"))
            .waitForExists(
                TestAssetHelper.waitingTime
            )

    fun verifyRecentlyClosedTabsMenuView() = assertRecentlyClosedTabsMenuView()

    fun verifyEmptyRecentlyClosedTabsList() = assertEmptyRecentlyClosedTabsList()

    fun verifyRecentlyClosedTabsPageTitle(title: String) = assertRecentlyClosedTabsPageTitle(title)

    fun verifyRecentlyClosedTabsUrl(expectedUrl: Uri) = assertPageUrl(expectedUrl)

    fun clickDeleteRecentlyClosedTabs() = recentlyClosedTabsDeleteButton().click()

    class Transition {
        fun clickRecentlyClosedItem(title: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            recentlyClosedTabsPageTitle(title).click()
            mDevice.waitForIdle()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

private fun assertRecentlyClosedTabsMenuView() {
    onView(
        allOf(
            withText("Recently closed tabs"),
            withParent(withId(R.id.navigationToolbar))
        )
    )
        .check(
            matches(withEffectiveVisibility(Visibility.VISIBLE))
        )
}

private fun assertEmptyRecentlyClosedTabsList() {
    mDevice.waitForIdle()

    onView(
        allOf(
            withId(R.id.recently_closed_empty_view),
            withText(R.string.recently_closed_empty_message)
        )
    ).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertPageUrl(expectedUrl: Uri) = onView(
    allOf(
        withId(R.id.url),
        withEffectiveVisibility(
            Visibility.VISIBLE
        )
    )
)
    .check(
        matches(withText(Matchers.containsString(expectedUrl.toString())))
    )

private fun recentlyClosedTabsPageTitle(title: String) = onView(
    allOf(
        withId(R.id.title),
        withText(title)
    )
)

private fun assertRecentlyClosedTabsPageTitle(title: String) {
    recentlyClosedTabsPageTitle(title)
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun recentlyClosedTabsDeleteButton() =
    onView(
        allOf(
            withId(R.id.overflow_menu),
            withEffectiveVisibility(
                Visibility.VISIBLE
            )
        )
    )
