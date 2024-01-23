/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.os.Build
import android.util.Log
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.junit4.ComposeTestRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
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

    fun verifyAddPrivateBrowsingShortcutButton(composeTestRule: ComposeTestRule) {
        composeTestRule.onNodeWithTag("private.add").assertIsDisplayed()
        Log.i(TAG, "verifyAddPrivateBrowsingShortcutButton: Verified \"Add to Home screen\" private browsing shortcut dialog button is displayed")
    }

    fun verifyNoThanksPrivateBrowsingShortcutButton(composeTestRule: ComposeTestRule) {
        composeTestRule.onNodeWithTag("private.cancel").assertIsDisplayed()
        Log.i(TAG, "verifyNoThanksPrivateBrowsingShortcutButton: Verified \"No thanks\" private browsing shortcut dialog button is displayed")
    }

    fun clickAddPrivateBrowsingShortcutButton(composeTestRule: ComposeTestRule) {
        composeTestRule.onNodeWithTag("private.add").performClick()
        Log.i(TAG, "clickAddPrivateBrowsingShortcutButton: Clicked \"Add to Home screen\" private browsing shortcut dialog button")
    }

    fun addShortcutName(title: String) {
        shortcutTextField().setText(title)
        Log.i(TAG, "addShortcutName: Set shortcut name to: $title")
    }

    fun verifyShortcutTextFieldTitle(title: String) = assertUIObjectExists(shortcutTitle(title))

    fun clickAddShortcutButton() {
        confirmAddToHomeScreenButton().clickAndWaitForNewWindow(waitingTime)
        Log.i(TAG, "clickAddShortcutButton: Clicked \"Add\" button from \"Add to home screen\" dialog")
    }

    fun clickCancelShortcutButton() {
        cancelAddToHomeScreenButton().click()
        Log.i(TAG, "clickCancelShortcutButton: Clicked \"Cancel\" button from \"Add to home screen\" dialog")
    }

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
            Log.i(TAG, "clickAddAutomaticallyButton: Waited for \"Add automatically\" system dialog button")
            addAutomaticallyButton().click()
            Log.i(TAG, "clickAddAutomaticallyButton: Clicked \"Add automatically\" system dialog button")
        }
    }

    fun verifyShortcutAdded(shortcutTitle: String) =
        assertUIObjectExists(itemContainingText(shortcutTitle))

    class Transition {
        fun openHomeScreenShortcut(title: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.wait(
                Until.findObject(By.text(title)),
                waitingTime,
            )
            Log.i(TAG, "openHomeScreenShortcut: Waited for $title home screen shortcut")
            mDevice.findObject((UiSelector().text(title))).clickAndWaitForNewWindow(waitingTime)
            Log.i(TAG, "openHomeScreenShortcut: Clicked $title home screen shortcut")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun searchAndOpenHomeScreenShortcut(title: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.pressHome()
            Log.i(TAG, "searchAndOpenHomeScreenShortcut: Pressed device home button")

            fun homeScreenView() = UiScrollable(UiSelector().scrollable(true))
            homeScreenView().waitForExists(waitingTime)
            Log.i(TAG, "searchAndOpenHomeScreenShortcut: Waiting for home screen view")

            fun shortcut() =
                homeScreenView()
                    .setAsHorizontalList()
                    .getChildByText(UiSelector().textContains(title), title, true)
            shortcut().clickAndWaitForNewWindow()
            Log.i(TAG, "searchAndOpenHomeScreenShortcut: Clicked home screen shortcut: $title")

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

private fun cancelAddToHomeScreenButton() =
    itemWithResId("$packageName:id/cancel_button")
private fun confirmAddToHomeScreenButton() =
    itemWithResId("$packageName:id/add_button")
private fun shortcutTextField() =
    itemWithResId("$packageName:id/shortcut_text")
private fun shortcutTitle(title: String) =
    itemWithResIdAndText("$packageName:id/shortcut_text", title)
