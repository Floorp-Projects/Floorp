/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.net.Uri
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.RootMatchers.isDialog
import androidx.test.espresso.matcher.ViewMatchers
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
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
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

    fun verifyVisitedTimeTitle() {
        mDevice.waitNotNull(
            Until.findObject(
                By.text("Last 24 hours")
            ),
            waitingTime
        )
        assertVisitedTimeTitle()
    }

    fun verifyFirstTestPageTitle(title: String) = assertTestPageTitle(title)

    fun verifyTestPageUrl(expectedUrl: Uri) = assertPageUrl(expectedUrl)

    fun verifyCopySnackBarText() = assertCopySnackBarText()

    fun verifyDeleteConfirmationMessage() = assertDeleteConfirmationMessage()

    fun verifyHomeScreen() = HomeScreenRobot().verifyHomeScreen()

    fun openOverflowMenu() {
        mDevice.waitNotNull(
            Until.findObject(
                By.res("org.mozilla.fenix.debug:id/overflow_menu")
            ),
            waitingTime
        )
        threeDotMenu().click()
    }

    fun clickDeleteHistoryButton() {
        mDevice.waitNotNull(Until.findObject(By.text("Delete history")), waitingTime)
        deleteAllHistoryButton().click()
    }

    fun confirmDeleteAllHistory() {
        onView(withText("Delete"))
            .inRoot(isDialog())
            .check(matches(isDisplayed()))
            .click()
    }

    fun verifyDeleteSnackbarText(text: String) = assertSnackBarText(text)

    class Transition {
        fun closeMenu(interact: HistoryRobot.() -> Unit): Transition {
            closeButton().click()

            HistoryRobot().interact()
            return Transition()
        }

        fun goBackToBrowser(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            closeButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openThreeDotMenu(interact: ThreeDotMenuHistoryItemRobot.() -> Unit):
            ThreeDotMenuHistoryItemRobot.Transition {

            threeDotMenu().click()

            ThreeDotMenuHistoryItemRobot().interact()
            return ThreeDotMenuHistoryItemRobot.Transition()
        }
    }
}

fun historyMenu(interact: HistoryRobot.() -> Unit): HistoryRobot.Transition {
    HistoryRobot().interact()
    return HistoryRobot.Transition()
}

private fun closeButton() = onView(withId(R.id.close_history))

private fun testPageTitle() = onView(allOf(withId(R.id.title), withText("Test_Page_1")))

private fun pageUrl() = onView(withId(R.id.url))

private fun threeDotMenu() = onView(withId(R.id.overflow_menu))

private fun snackBarText() = onView(withId(R.id.snackbar_text))

private fun deleteAllHistoryButton() = onView(withId(R.id.delete_button))

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

private fun assertVisitedTimeTitle() =
    onView(withId(R.id.header_title)).check(matches(withText("Last 24 hours")))

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
