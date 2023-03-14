/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.os.Build
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.junit4.ComposeTestRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.junit.Assert.assertTrue
import org.mozilla.fenix.helpers.MatcherHelper.assertItemContainingTextExists
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import java.util.regex.Pattern

/**
 * Implementation of Robot Pattern for the Add to homescreen feature.
 */
class AddToHomeScreenRobot {

    fun verifyAddPrivateBrowsingShortcutButton(composeTestRule: ComposeTestRule) = assertAddPrivateBrowsingShortcutButton(composeTestRule)

    fun verifyNoThanksPrivateBrowsingShortcutButton(composeTestRule: ComposeTestRule) = assertNoThanksPrivateBrowsingShortcutButton(composeTestRule)

    fun clickAddPrivateBrowsingShortcutButton(composeTestRule: ComposeTestRule) = composeTestRule.onNodeWithTag("private.add").performClick()

    fun addShortcutName(title: String) = shortcutTextField.setText(title)

    fun verifyShortcutTextFieldTitle(title: String) = assertItemContainingTextExists(shortcutTitle(title))

    fun clickAddShortcutButton() =
        confirmAddToHomeScreenButton.clickAndWaitForNewWindow(waitingTime)

    fun clickCancelShortcutButton() =
        cancelAddToHomeScreenButton.click()

    fun clickAddAutomaticallyButton() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            mDevice.wait(
                Until.findObject(
                    By.text(
                        Pattern.compile("Add Automatically", Pattern.CASE_INSENSITIVE),
                    ),
                ),
                waitingTime,
            )
            addAutomaticallyButton().click()
        }
    }

    fun verifyShortcutAdded(shortcutTitle: String) {
        assertTrue(
            mDevice.findObject(UiSelector().text(shortcutTitle))
                .waitForExists(waitingTime),
        )
    }

    class Transition {
        fun openHomeScreenShortcut(title: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.wait(
                Until.findObject(By.text(title)),
                waitingTime,
            )
            mDevice.findObject((UiSelector().text(title))).clickAndWaitForNewWindow(waitingTime)

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun searchAndOpenHomeScreenShortcut(title: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.pressHome()

            fun homeScreenView() = UiScrollable(UiSelector().scrollable(true))
            homeScreenView().waitForExists(waitingTime)

            fun shortcut() =
                homeScreenView()
                    .setAsHorizontalList()
                    .getChildByText(UiSelector().textContains(title), title, true)
            shortcut().clickAndWaitForNewWindow()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

fun addToHomeScreen(interact: AddToHomeScreenRobot.() -> Unit): AddToHomeScreenRobot.Transition {
    AddToHomeScreenRobot().interact()
    return AddToHomeScreenRobot.Transition()
}

private fun addAutomaticallyButton() =
    mDevice.findObject(UiSelector().textContains("add automatically"))

private fun assertAddPrivateBrowsingShortcutButton(composeTestRule: ComposeTestRule) =
    composeTestRule.onNodeWithTag("private.add").assertIsDisplayed()

private fun assertNoThanksPrivateBrowsingShortcutButton(composeTestRule: ComposeTestRule) =
    composeTestRule.onNodeWithTag("private.cancel").assertIsDisplayed()

private val cancelAddToHomeScreenButton =
    itemWithResId("$packageName:id/cancel_button")
private val confirmAddToHomeScreenButton =
    itemWithResId("$packageName:id/add_button")
private val shortcutTextField =
    itemWithResId("$packageName:id/shortcut_text")
private fun shortcutTitle(title: String) =
    itemWithResIdAndText("$packageName:id/shortcut_text", title)
