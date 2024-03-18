/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.net.Uri
import android.util.Log
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
import org.mozilla.fenix.helpers.Constants.TAG
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
        Log.i(TAG, "verifyHistoryMenuView: Trying to verify that history menu view is visible")
        onView(
            allOf(withText("History"), withParent(withId(R.id.navigationToolbar))),
        ).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyHistoryMenuView: Verified that history menu view is visible")
    }

    fun verifyEmptyHistoryView() {
        Log.i(TAG, "verifyEmptyHistoryView: Waiting for $waitingTime ms for empty history list view to exist")
        mDevice.findObject(
            UiSelector().text("No history here"),
        ).waitForExists(waitingTime)
        Log.i(TAG, "verifyEmptyHistoryView: Waited for $waitingTime ms for empty history list view to exist")

        Log.i(TAG, "verifyEmptyHistoryView: Trying to verify empty history list view")
        onView(
            allOf(
                withId(R.id.history_empty_view),
                withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE),
            ),
        ).check(matches(withText("No history here")))
        Log.i(TAG, "verifyEmptyHistoryView: Verified empty history list view")
    }

    fun verifyHistoryListExists() = assertUIObjectExists(itemWithResId("$packageName:id/history_list"))

    fun verifyVisitedTimeTitle() {
        mDevice.waitNotNull(
            Until.findObject(
                By.text("Today"),
            ),
            waitingTime,
        )
        Log.i(TAG, "verifyVisitedTimeTitle: Trying to verify \"Today\" chronological timeline title")
        onView(withId(R.id.header_title)).check(matches(withText("Today")))
        Log.i(TAG, "verifyVisitedTimeTitle: Verified \"Today\" chronological timeline title")
    }

    fun verifyHistoryItemExists(shouldExist: Boolean, item: String) =
        assertUIObjectExists(itemContainingText(item), exists = shouldExist)

    fun verifyFirstTestPageTitle(title: String) {
        Log.i(TAG, "verifyFirstTestPageTitle: Trying to verify $title page title is visible")
        testPageTitle()
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
            .check(matches(withText(title)))
        Log.i(TAG, "verifyFirstTestPageTitle: Verified $title page title is visible")
    }

    fun verifyTestPageUrl(expectedUrl: Uri) {
        Log.i(TAG, "verifyTestPageUrl: Trying to verify page url: $expectedUrl is displayed")
        pageUrl(expectedUrl.toString()).check(matches(isDisplayed()))
        Log.i(TAG, "verifyTestPageUrl: Verified page url: $expectedUrl is displayed")
    }

    fun verifyDeleteConfirmationMessage() =
        assertUIObjectExists(
            itemWithResIdContainingText("$packageName:id/title", getStringResource(R.string.delete_history_prompt_title)),
            itemWithResIdContainingText("$packageName:id/body", getStringResource(R.string.delete_history_prompt_body_2)),
        )

    fun clickDeleteHistoryButton(item: String) {
        Log.i(TAG, "clickDeleteHistoryButton: Trying to click delete history button for item: $item")
        deleteButton(item).click()
        Log.i(TAG, "clickDeleteHistoryButton: Clicked delete history button for item: $item")
    }

    fun verifyDeleteHistoryItemButton(historyItemTitle: String) {
        Log.i(TAG, "verifyDeleteHistoryItemButton: Trying to verify delete history button for item: $historyItemTitle is visible")
        deleteButton(historyItemTitle).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyDeleteHistoryItemButton: Verified delete history button for item: $historyItemTitle is visible")
    }

    fun clickDeleteAllHistoryButton() {
        Log.i(TAG, "clickDeleteAllHistoryButton: Trying to click delete all history button")
        deleteButton().click()
        Log.i(TAG, "clickDeleteAllHistoryButton: Clicked delete all history button")
    }

    fun selectEverythingOption() {
        Log.i(TAG, "selectEverythingOption: Trying to click \"Everything\" dialog option")
        deleteHistoryEverythingOption().click()
        Log.i(TAG, "selectEverythingOption: Clicked \"Everything\" dialog option")
    }

    fun confirmDeleteAllHistory() {
        Log.i(TAG, "confirmDeleteAllHistory: Trying to click \"Delete\" dialog button")
        onView(withText("Delete"))
            .inRoot(isDialog())
            .check(matches(isDisplayed()))
            .click()
        Log.i(TAG, "confirmDeleteAllHistory: Clicked \"Delete\" dialog button")
    }

    fun cancelDeleteHistory() {
        Log.i(TAG, "cancelDeleteHistory: Trying to click \"Cancel\" dialog button")
        mDevice
            .findObject(
                UiSelector()
                    .textContains(getStringResource(R.string.delete_browsing_data_prompt_cancel)),
            ).click()
        Log.i(TAG, "cancelDeleteHistory: Clicked \"Cancel\" dialog button")
    }

    fun verifyUndoDeleteSnackBarButton() {
        Log.i(TAG, "verifyUndoDeleteSnackBarButton: Trying to verify \"Undo\" snackbar button")
        snackBarUndoButton().check(matches(withText("UNDO")))
        Log.i(TAG, "verifyUndoDeleteSnackBarButton: Verified \"Undo\" snackbar button")
    }

    fun clickUndoDeleteButton() {
        Log.i(TAG, "verifyUndoDeleteSnackBarButton: Trying to click \"Undo\" snackbar button")
        snackBarUndoButton().click()
        Log.i(TAG, "verifyUndoDeleteSnackBarButton: Clicked \"Undo\" snackbar button")
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
        Log.i(TAG, "openSearchGroup: Waiting for $waitingTime ms for search group: $searchTerm to exist")
        mDevice.findObject(UiSelector().text(searchTerm)).waitForExists(waitingTime)
        Log.i(TAG, "openSearchGroup: Waited for $waitingTime ms for search group: $searchTerm to exist")
        Log.i(TAG, "openSearchGroup: Trying to click search group: $searchTerm")
        mDevice.findObject(UiSelector().text(searchTerm)).click()
        Log.i(TAG, "openSearchGroup: Clicked search group: $searchTerm")
    }

    class Transition {
        fun goBack(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "goBack: Trying to click go back menu button")
            onView(withContentDescription("Navigate up")).click()
            Log.i(TAG, "goBack: Clicked go back menu button")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openWebsite(url: Uri, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            assertUIObjectExists(itemWithResId("$packageName:id/history_list"))
            Log.i(TAG, "openWebsite: Trying to click history item with url: $url")
            onView(withText(url.toString())).click()
            Log.i(TAG, "openWebsite: Clicked history item with url: $url")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openRecentlyClosedTabs(interact: RecentlyClosedTabsRobot.() -> Unit): RecentlyClosedTabsRobot.Transition {
            Log.i(TAG, "openRecentlyClosedTabs: Waiting for $waitingTime ms for \"Recently closed tabs\" button to exist")
            recentlyClosedTabsListButton().waitForExists(waitingTime)
            Log.i(TAG, "openRecentlyClosedTabs: Waited for $waitingTime ms for \"Recently closed tabs\" button to exist")
            Log.i(TAG, "openRecentlyClosedTabs: Trying to click \"Recently closed tabs\" button")
            recentlyClosedTabsListButton().click()
            Log.i(TAG, "openRecentlyClosedTabs: Clicked \"Recently closed tabs\" button")

            RecentlyClosedTabsRobot().interact()
            return RecentlyClosedTabsRobot.Transition()
        }

        fun clickSearchButton(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            Log.i(TAG, "clickSearchButton: Trying to click search history button")
            itemWithResId("$packageName:id/history_search").click()
            Log.i(TAG, "clickSearchButton: Clicked search history button")

            SearchRobot().interact()
            return SearchRobot.Transition()
        }
    }
}

fun historyMenu(interact: HistoryRobot.() -> Unit): HistoryRobot.Transition {
    HistoryRobot().interact()
    return HistoryRobot.Transition()
}

private fun testPageTitle() = onView(withId(R.id.title))

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
