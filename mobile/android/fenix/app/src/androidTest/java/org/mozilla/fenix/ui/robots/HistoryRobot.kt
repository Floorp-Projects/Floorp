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
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull

/**
 * Implementation of Robot Pattern for the history menu.
 */
class HistoryRobot {

    fun verifyHistoryMenuView() = assertHistoryMenuView()

    fun verifyEmptyHistoryView() {
        mDevice.findObject(
            UiSelector().text("No history here")
        ).waitForExists(waitingTime)

        assertEmptyHistoryView()
    }

    fun verifyHistoryListExists() = assertHistoryListExists()

    fun verifyVisitedTimeTitle() {
        mDevice.waitNotNull(
            Until.findObject(
                By.text("Today")
            ),
            waitingTime
        )
        assertVisitedTimeTitle()
    }

    fun verifyHistoryItemExists(shouldExist: Boolean, item: String) = assertHistoryItemExists(shouldExist, item)

    fun verifyFirstTestPageTitle(title: String) = assertTestPageTitle(title)

    fun verifyTestPageUrl(expectedUrl: Uri) = assertPageUrl(expectedUrl)

    fun verifyCopySnackBarText() = assertCopySnackBarText()

    fun verifyDeleteConfirmationMessage() = assertDeleteConfirmationMessage()

    fun verifyHomeScreen() = HomeScreenRobot().verifyHomeScreen()

    fun clickDeleteHistoryButton(item: String) {
        deleteButton(item).click()
    }

    fun clickDeleteAllHistoryButton() = deleteAllButton().click()

    fun confirmDeleteAllHistory() {
        onView(withText("Delete"))
            .inRoot(isDialog())
            .check(matches(isDisplayed()))
            .click()
    }

    fun verifyDeleteSnackbarText(text: String) = assertSnackBarText(text)

    class Transition {
        fun goBackToBrowser(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.pressBack()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

fun historyMenu(interact: HistoryRobot.() -> Unit): HistoryRobot.Transition {
    HistoryRobot().interact()
    return HistoryRobot.Transition()
}

private fun testPageTitle() = onView(allOf(withId(R.id.title), withText("Test_Page_1")))

private fun pageUrl() = onView(withId(R.id.url))

private fun deleteButton(title: String) =
    onView(allOf(withId(R.id.overflow_menu), hasSibling(withText(title))))

private fun deleteAllButton() = onView(withId(R.id.history_delete_all))

private fun snackBarText() = onView(withId(R.id.snackbar_text))

private fun assertHistoryMenuView() {
    onView(
        allOf(withText("History"), withParent(withId(R.id.navigationToolbar)))
    )
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
}

private fun assertEmptyHistoryView() =
    onView(
        allOf(
            withId(R.id.history_empty_view),
            withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)
        )
    )
        .check(matches(withText("No history here")))

private fun assertHistoryListExists() =
    mDevice.findObject(UiSelector().resourceId("R.id.history_list")).waitForExists(waitingTime)

private fun assertHistoryItemExists(shouldExist: Boolean, item: String) {
    if (shouldExist) {
        assertTrue(mDevice.findObject(UiSelector().textContains(item)).waitForExists(waitingTime))
    } else {
        assertFalse(mDevice.findObject(UiSelector().textContains(item)).waitForExists(waitingTime))
    }
}

private fun assertVisitedTimeTitle() =
    onView(withId(R.id.header_title)).check(matches(withText("Today")))

private fun assertTestPageTitle(title: String) = testPageTitle()
    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    .check(matches(withText(title)))

private fun assertPageUrl(expectedUrl: Uri) = pageUrl()
    .check(matches(ViewMatchers.isCompletelyDisplayed()))
    .check(matches(withText(Matchers.containsString(expectedUrl.toString()))))

private fun assertDeleteConfirmationMessage() =
    onView(withText("This will delete all of your browsing data."))
        .inRoot(isDialog())
        .check(matches(isDisplayed()))

private fun assertCopySnackBarText() = snackBarText().check(matches(withText("URL copied")))

private fun assertSnackBarText(text: String) =
    snackBarText().check(matches(withText(Matchers.containsString(text))))
