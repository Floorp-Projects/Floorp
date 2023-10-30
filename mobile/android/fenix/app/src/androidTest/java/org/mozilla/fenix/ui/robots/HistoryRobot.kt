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
import org.hamcrest.Matchers
import org.hamcrest.Matchers.allOf
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull

/**
 * Implementation of Robot Pattern for the history menu.
 */
class HistoryRobot {

    fun verifyHistoryMenuView() = assertHistoryMenuView()

    fun verifyEmptyHistoryView() {
        mDevice.findObject(
            UiSelector().text("No history here"),
        ).waitForExists(waitingTime)

        assertEmptyHistoryView()
    }

    fun verifyHistoryListExists() = assertHistoryListExists()

    fun verifyVisitedTimeTitle() {
        mDevice.waitNotNull(
            Until.findObject(
                By.text("Today"),
            ),
            waitingTime,
        )
        assertVisitedTimeTitle()
    }

    fun verifyHistoryItemExists(shouldExist: Boolean, item: String) {
        if (shouldExist) {
            assertTrue(mDevice.findObject(UiSelector().textContains(item)).waitForExists(waitingTime))
        } else {
            assertFalse(mDevice.findObject(UiSelector().textContains(item)).waitForExists(waitingTimeShort))
        }
    }

    fun verifyFirstTestPageTitle(title: String) = assertTestPageTitle(title)

    fun verifyTestPageUrl(expectedUrl: Uri) = pageUrl(expectedUrl.toString()).check(matches(isDisplayed()))

    fun verifyCopySnackBarText() = assertCopySnackBarText()

    fun verifyDeleteConfirmationMessage() = assertDeleteConfirmationMessage()

    fun verifyHomeScreen() = HomeScreenRobot().verifyHomeScreen()

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

    fun verifyDeleteSnackbarText(text: String) = assertSnackBarText(text)

    fun verifyUndoDeleteSnackBarButton() = assertUndoDeleteSnackBarButton()

    fun clickUndoDeleteButton() {
        snackBarUndoButton().click()
    }

    fun verifySearchGroupDisplayed(shouldBeDisplayed: Boolean, searchTerm: String, groupSize: Int) {
        // checks if the search group exists in the Recently visited section
        if (shouldBeDisplayed) {
            assertTrue(
                mDevice.findObject(UiSelector().text(searchTerm))
                    .getFromParent(UiSelector().text("$groupSize sites"))
                    .waitForExists(waitingTimeShort),
            )
        } else {
            assertFalse(
                mDevice.findObject(UiSelector().text(searchTerm))
                    .getFromParent(UiSelector().text("$groupSize sites"))
                    .waitForExists(waitingTimeShort),
            )
        }
    }

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
            assertHistoryListExists()
            onView(withText(url.toString())).click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openRecentlyClosedTabs(interact: RecentlyClosedTabsRobot.() -> Unit): RecentlyClosedTabsRobot.Transition {
            recentlyClosedTabsListButton.waitForExists(waitingTime)
            recentlyClosedTabsListButton.click()

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

private fun snackBarText() = onView(withId(R.id.snackbar_text))

private fun assertHistoryMenuView() {
    onView(
        allOf(withText("History"), withParent(withId(R.id.navigationToolbar))),
    )
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
}

private fun assertEmptyHistoryView() =
    onView(
        allOf(
            withId(R.id.history_empty_view),
            withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE),
        ),
    )
        .check(matches(withText("No history here")))

private fun assertHistoryListExists() =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/history_list")).waitForExists(waitingTime)

private fun assertVisitedTimeTitle() =
    onView(withId(R.id.header_title)).check(matches(withText("Today")))

private fun assertTestPageTitle(title: String) = testPageTitle()
    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    .check(matches(withText(title)))

private fun assertDeleteConfirmationMessage() {
    assertTrue(deleteHistoryPromptTitle().waitForExists(waitingTime))
    assertTrue(deleteHistoryPromptSummary().waitForExists(waitingTime))
}

private fun assertCopySnackBarText() = snackBarText().check(matches(withText("URL copied")))

private fun assertSnackBarText(text: String) =
    snackBarText().check(matches(withText(Matchers.containsString(text))))

private fun snackBarUndoButton() = onView(withId(R.id.snackbar_btn))

private fun assertUndoDeleteSnackBarButton() =
    snackBarUndoButton().check(matches(withText("UNDO")))

private fun deleteHistoryPromptTitle() =
    mDevice
        .findObject(
            UiSelector()
                .textContains(getStringResource(R.string.delete_history_prompt_title))
                .resourceId("$packageName:id/title"),
        )

private fun deleteHistoryPromptSummary() =
    mDevice
        .findObject(
            UiSelector()
                .textContains(getStringResource(R.string.delete_history_prompt_body_2))
                .resourceId("$packageName:id/body"),
        )

private fun deleteHistoryEverythingOption() =
    mDevice
        .findObject(
            UiSelector()
                .textContains(getStringResource(R.string.delete_history_prompt_button_everything))
                .resourceId("$packageName:id/everything_button"),
        )

private val recentlyClosedTabsListButton =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/recently_closed_tabs_header"))
