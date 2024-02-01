/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.net.Uri
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.RootMatchers.isDialog
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withParent
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.hamcrest.Matchers.allOf
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdContainingText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull

/**
 * Implementation of Robot Pattern for the history menu.
 */
class HistoryRobot {

    fun verifyHistoryMenuView() {
        onView(
            allOf(withText("History"), withParent(withId(R.id.navigationToolbar))),
        ).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    }

    fun verifyEmptyHistoryView() {
        mDevice.findObject(
            UiSelector().text("No history here"),
        ).waitForExists(waitingTime)

        onView(
            allOf(
                withId(R.id.history_empty_view),
                withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE),
            ),
        ).check(matches(withText("No history here")))
    }

    fun verifyHistoryListExists() = assertUIObjectExists(itemWithResId("$packageName:id/history_list"))

    fun verifyVisitedTimeTitle() {
        mDevice.waitNotNull(
            Until.findObject(
                By.text("Today"),
            ),
            waitingTime,
        )
        onView(withId(R.id.header_title)).check(matches(withText("Today")))
    }

    fun verifyHistoryItemExists(shouldExist: Boolean, item: String) =
        assertUIObjectExists(itemContainingText(item), exists = shouldExist)

    fun verifyFirstTestPageTitle(title: String) =
        testPageTitle()
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
            .check(matches(withText(title)))

    fun verifyTestPageUrl(expectedUrl: Uri) = pageUrl(expectedUrl.toString()).check(matches(isDisplayed()))

    fun verifyDeleteConfirmationMessage() =
        assertUIObjectExists(
            itemWithResIdContainingText("$packageName:id/title", getStringResource(R.string.delete_history_prompt_title)),
            itemWithResIdContainingText("$packageName:id/body", getStringResource(R.string.delete_history_prompt_body_2)),
        )

    fun clickDeleteHistoryButton(item: String) {
        deleteButton(item).click()
    }

    fun verifyDeleteHistoryItemButton(historyItemTitle: String) =
        deleteButton(historyItemTitle).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

    fun clickDeleteAllHistoryButton() = deleteButton().click()

    fun selectEverythingOption() = deleteHistoryEverythingOption().click()

    fun confirmDeleteAllHistory() {
        onView(withText("Delete"))
            .inRoot(isDialog())
            .check(matches(isDisplayed()))
            .click()
    }

    fun cancelDeleteHistory() =
        mDevice
            .findObject(
                UiSelector()
                    .textContains(getStringResource(R.string.delete_browsing_data_prompt_cancel)),
            ).click()

    fun verifyUndoDeleteSnackBarButton() = snackBarUndoButton().check(matches(withText("UNDO")))

    fun clickUndoDeleteButton() {
        snackBarUndoButton().click()
    }

    fun verifySearchGroupDisplayed(shouldBeDisplayed: Boolean, searchTerm: String, groupSize: Int) =
        // checks if the search group exists in the Recently visited section
        assertUIObjectExists(
            itemContainingText(searchTerm)
                .getFromParent(
                    UiSelector().text("$groupSize sites"),
                ),
            exists = shouldBeDisplayed,
        )

    fun openSearchGroup(searchTerm: String) {
        mDevice.findObject(UiSelector().text(searchTerm)).waitForExists(waitingTime)
        mDevice.findObject(UiSelector().text(searchTerm)).click()
    }

    class Transition {
        fun goBack(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            onView(withContentDescription("Navigate up")).click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openWebsite(url: Uri, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            assertUIObjectExists(itemWithResId("$packageName:id/history_list"))
            onView(withText(url.toString())).click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openRecentlyClosedTabs(interact: RecentlyClosedTabsRobot.() -> Unit): RecentlyClosedTabsRobot.Transition {
            recentlyClosedTabsListButton().waitForExists(waitingTime)
            recentlyClosedTabsListButton().click()

            RecentlyClosedTabsRobot().interact()
            return RecentlyClosedTabsRobot.Transition()
        }

        fun clickSearchButton(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            itemWithResId("$packageName:id/history_search").click()

            SearchRobot().interact()
            return SearchRobot.Transition()
        }
    }
}

fun historyMenu(interact: HistoryRobot.() -> Unit): HistoryRobot.Transition {
    HistoryRobot().interact()
    return HistoryRobot.Transition()
}

private fun testPageTitle() = onView(allOf(withId(R.id.title), withText("Test_Page_1")))

private fun pageUrl(url: String) = onView(allOf(withId(R.id.url), withText(url)))

private fun deleteButton(title: String) =
    onView(allOf(withContentDescription("Delete"), hasSibling(withText(title))))

private fun deleteButton() = onView(withId(R.id.history_delete))

private fun snackBarUndoButton() = onView(withId(R.id.snackbar_btn))

private fun deleteHistoryEverythingOption() =
    mDevice
        .findObject(
            UiSelector()
                .textContains(getStringResource(R.string.delete_history_prompt_button_everything))
                .resourceId("$packageName:id/everything_button"),
        )

private fun recentlyClosedTabsListButton() =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/recently_closed_tabs_header"))
