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
        Log.i(TAG, "verifyAddPrivateBrowsingShortcutButton: Trying to verify \"Add to Home screen\" private browsing shortcut dialog button is displayed")
        composeTestRule.onNodeWithTag("private.add").assertIsDisplayed()
        Log.i(TAG, "verifyAddPrivateBrowsingShortcutButton: Verified \"Add to Home screen\" private browsing shortcut dialog button is displayed")
    }

    fun verifyNoThanksPrivateBrowsingShortcutButton(composeTestRule: ComposeTestRule) {
        Log.i(TAG, "verifyNoThanksPrivateBrowsingShortcutButton: Trying to verify \"No thanks\" private browsing shortcut dialog button is displayed")
        composeTestRule.onNodeWithTag("private.cancel").assertIsDisplayed()
        Log.i(TAG, "verifyNoThanksPrivateBrowsingShortcutButton: Verified \"No thanks\" private browsing shortcut dialog button is displayed")
    }

    fun clickAddPrivateBrowsingShortcutButton(composeTestRule: ComposeTestRule) {
        Log.i(TAG, "clickAddPrivateBrowsingShortcutButton: Trying to click \"Add to Home screen\" private browsing shortcut dialog button")
        composeTestRule.onNodeWithTag("private.add").performClick()
        Log.i(TAG, "clickAddPrivateBrowsingShortcutButton: Clicked \"Add to Home screen\" private browsing shortcut dialog button")
    }

    fun addShortcutName(title: String) {
        Log.i(TAG, "addShortcutName: Trying to set shortcut name to: $title")
        shortcutTextField().setText(title)
        Log.i(TAG, "addShortcutName: Set shortcut name to: $title")
    }

    fun verifyShortcutTextFieldTitle(title: String) = assertUIObjectExists(shortcutTitle(title))

    fun clickAddShortcutButton() {
        Log.i(TAG, "clickAddShortcutButton: Trying to click \"Add\" button from \"Add to home screen\" dialog and wait for $waitingTime ms for a new window")
        confirmAddToHomeScreenButton().clickAndWaitForNewWindow(waitingTime)
        Log.i(TAG, "clickAddShortcutButton: Clicked \"Add\" button from \"Add to home screen\" dialog and waited for $waitingTime ms for a new window")
    }

    fun clickCancelShortcutButton() {
        Log.i(TAG, "clickCancelShortcutButton: Trying to click \"Cancel\" button from \"Add to home screen\" dialog")
        cancelAddToHomeScreenButton().click()
        Log.i(TAG, "clickCancelShortcutButton: Clicked \"Cancel\" button from \"Add to home screen\" dialog")
    }

    fun clickAddAutomaticallyButton() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Log.i(TAG, "clickAddAutomaticallyButton: Waiting for $waitingTime ms until finding \"Add automatically\" system dialog button")
            mDevice.wait(
                Until.findObject(
                    By.text(
                        Pattern.compile("Add Automatically", Pattern.CASE_INSENSITIVE),
                    ),
                ),
                waitingTime,
            )
            Log.i(TAG, "clickAddAutomaticallyButton: Waited for $waitingTime ms until \"Add automatically\" system dialog button was found")
            Log.i(TAG, "clickAddAutomaticallyButton: Trying to click \"Add automatically\" system dialog button")
            addAutomaticallyButton().click()
            Log.i(TAG, "clickAddAutomaticallyButton: Clicked \"Add automatically\" system dialog button")
        }
    }

    fun verifyShortcutAdded(shortcutTitle: String) =
        assertUIObjectExists(itemContainingText(shortcutTitle))

    class Transition {
        fun openHomeScreenShortcut(title: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "openHomeScreenShortcut: Waiting for $waitingTime ms until finding $title home screen shortcut")
            mDevice.wait(
                Until.findObject(By.text(title)),
                waitingTime,
            )
            Log.i(TAG, "openHomeScreenShortcut: Waited for $waitingTime ms until $title home screen shortcut was found")
            Log.i(TAG, "openHomeScreenShortcut: Trying to click $title home screen shortcut and wait for $waitingTime ms for a new window")
            mDevice.findObject((UiSelector().text(title))).clickAndWaitForNewWindow(waitingTime)
            Log.i(TAG, "openHomeScreenShortcut: Clicked $title home screen shortcut and waited for $waitingTime ms for a new window")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun searchAndOpenHomeScreenShortcut(title: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "searchAndOpenHomeScreenShortcut: Trying to press device home button")
            mDevice.pressHome()
            Log.i(TAG, "searchAndOpenHomeScreenShortcut: Pressed device home button")

            fun homeScreenView() = UiScrollable(UiSelector().scrollable(true))
            Log.i(TAG, "searchAndOpenHomeScreenShortcut: Waiting for $waitingTime ms for home screen view to exist")
            homeScreenView().waitForExists(waitingTime)
            Log.i(TAG, "searchAndOpenHomeScreenShortcut: Waited for $waitingTime ms for home screen view to exist")

            fun shortcut() =
                homeScreenView()
                    .setAsHorizontalList()
                    .getChildByText(UiSelector().textContains(title), title, true)
            Log.i(TAG, "searchAndOpenHomeScreenShortcut: Trying to click home screen shortcut: $title and wait for a new window")
            shortcut().clickAndWaitForNewWindow()
            Log.i(TAG, "searchAndOpenHomeScreenShortcut: Clicked home screen shortcut: $title and waited for a new window")

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
