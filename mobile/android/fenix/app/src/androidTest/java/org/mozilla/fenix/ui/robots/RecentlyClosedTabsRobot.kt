/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.net.Uri
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
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

    fun openRecentlyClosedTabsThreeDotMenu() = recentlyClosedTabsThreeDotButton().click()

    fun verifyRecentlyClosedTabsMenuCopy() = assertRecentlyClosedTabsMenuCopy()

    fun verifyRecentlyClosedTabsMenuShare() = assertRecentlyClosedTabsMenuShare()

    fun verifyRecentlyClosedTabsMenuNewTab() = assertRecentlyClosedTabsOverlayNewTab()

    fun verifyRecentlyClosedTabsMenuPrivateTab() = assertRecentlyClosedTabsMenuPrivateTab()

    fun verifyRecentlyClosedTabsMenuDelete() = assertRecentlyClosedTabsMenuDelete()

    fun clickCopyRecentlyClosedTabs() = recentlyClosedTabsCopyButton().click()

    fun clickShareRecentlyClosedTabs() = recentlyClosedTabsShareButton().click()

    fun clickDeleteCopyRecentlyClosedTabs() = recentlyClosedTabsDeleteButton().click()

    fun verifyCopyRecentlyClosedTabsSnackBarText() = assertCopySnackBarText()

    fun verifyShareOverlay() = assertRecentlyClosedShareOverlay()

    fun verifyShareTabFavicon() = assertRecentlyClosedShareFavicon()

    fun verifyShareTabTitle(title: String) = assetRecentlyClosedShareTitle(title)

    fun verifyShareTabUrl(expectedUrl: Uri) = assertRecentlyClosedShareUrl(expectedUrl)

    class Transition {
        fun clickOpenInNewTab(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            recentlyClosedTabsNewTabButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickOpenInPrivateTab(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            recentlyClosedTabsNewPrivateTabButton().click()

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
            matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertEmptyRecentlyClosedTabsList() =
    onView(
        allOf(
            withId(R.id.recently_closed_empty_view),
            withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)
        )
    )
        .check(
            matches(withText("No recently closed tabs here")))

private fun assertPageUrl(expectedUrl: Uri) = onView(
    allOf(
        withId(R.id.url),
        withEffectiveVisibility(
            Visibility.VISIBLE
        )
    )
)
    .check(
        matches(withText(Matchers.containsString(expectedUrl.toString()))))

private fun recentlyClosedTabsPageTitle() = onView(
    allOf(
        withId(R.id.title),
        withText("Test_Page_1")
    )
)

private fun assertRecentlyClosedTabsPageTitle(title: String) {
    recentlyClosedTabsPageTitle()
        .check(
            matches(withEffectiveVisibility(Visibility.VISIBLE)))
        .check(
            matches(withText(title)))
}

private fun recentlyClosedTabsThreeDotButton() =
    onView(
        allOf(
            withId(R.id.overflow_menu),
            withEffectiveVisibility(
            Visibility.VISIBLE
        )
    )
)

private fun assertRecentlyClosedTabsMenuCopy() =
    onView(withText("Copy"))
        .check(
            matches(
                withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertRecentlyClosedTabsMenuShare() =
    onView(withText("Share"))
        .check(
            matches(
                withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertRecentlyClosedTabsOverlayNewTab() =
    onView(withText("Open in new tab"))
        .check(
            matches(
                withEffectiveVisibility(Visibility.VISIBLE))
)

private fun assertRecentlyClosedTabsMenuPrivateTab() =
    onView(withText("Open in private tab"))
        .check(
            matches(
                withEffectiveVisibility(Visibility.VISIBLE)
        )
    )

private fun assertRecentlyClosedTabsMenuDelete() =
    onView(withText("Delete"))
        .check(
            matches(
                withEffectiveVisibility(Visibility.VISIBLE)
    )
)

private fun recentlyClosedTabsCopyButton() = onView(withText("Copy"))

private fun copySnackBarText() = onView(withId(R.id.snackbar_text))

private fun assertCopySnackBarText() = copySnackBarText()
    .check(
        matches
            (withText("URL copied")))

private fun recentlyClosedTabsShareButton() = onView(withText("Share"))

private fun assertRecentlyClosedShareOverlay() =
    onView(withId(R.id.shareWrapper))
        .check(
            matches(ViewMatchers.isDisplayed()))

private fun assetRecentlyClosedShareTitle(title: String) =
    onView(withId(R.id.share_tab_title))
        .check(
            matches(ViewMatchers.isDisplayed()))
        .check(
            matches(withText(title)))

private fun assertRecentlyClosedShareFavicon() =
    onView(withId(R.id.share_tab_favicon))
        .check(
            matches(ViewMatchers.isDisplayed()))

private fun assertRecentlyClosedShareUrl(expectedUrl: Uri) =
    onView(
        allOf(
            withId(R.id.share_tab_url),
            withEffectiveVisibility(Visibility.VISIBLE)
        )
    )
        .check(
            matches(withText(Matchers.containsString(expectedUrl.toString()))))

private fun recentlyClosedTabsNewTabButton() = onView(withText("Open in new tab"))

private fun recentlyClosedTabsNewPrivateTabButton() = onView(withText("Open in private tab"))

private fun recentlyClosedTabsDeleteButton() = onView(withText("Delete"))
