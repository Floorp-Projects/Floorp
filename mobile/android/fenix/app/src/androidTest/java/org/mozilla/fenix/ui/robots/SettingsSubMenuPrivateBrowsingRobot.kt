/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.os.Build
import android.util.Log
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.By.text
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.junit.Assert.assertTrue
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.checkedItemWithResId
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.appName
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.isEnabled

/**
 * Implementation of Robot Pattern for the settings PrivateBrowsing sub menu.
 */

class SettingsSubMenuPrivateBrowsingRobot {

    fun verifyOpenLinksInPrivateTab() {
        Log.i(TAG, "verifyOpenLinksInPrivateTab: Trying to verify that the \"Open links in a private tab\" option is visible")
        openLinksInPrivateTabSwitch()
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifyOpenLinksInPrivateTab: Verified that the \"Open links in a private tab\" option is visible")
    }

    fun verifyAddPrivateBrowsingShortcutButton() {
        Log.i(TAG, "verifyAddPrivateBrowsingShortcutButton: Waiting for $waitingTime ms until finding the \"Add private browsing shortcut\" button")
        mDevice.wait(
            Until.findObject(text("Add private browsing shortcut")),
            waitingTime,
        )
        Log.i(TAG, "verifyAddPrivateBrowsingShortcutButton: Waited for $waitingTime ms until the \"Add private browsing shortcut\" button was found")
        Log.i(TAG, "verifyAddPrivateBrowsingShortcutButton: Trying to verify that the \"Add private browsing shortcut\" button is visible")
        addPrivateBrowsingShortcutButton()
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifyAddPrivateBrowsingShortcutButton: Verified that the \"Add private browsing shortcut\" button is visible")
    }

    fun verifyOpenLinksInPrivateTabEnabled() {
        Log.i(TAG, "verifyOpenLinksInPrivateTabEnabled: Trying to verify that the \"Open links in a private tab\" toggle is enabled")
        openLinksInPrivateTabSwitch().check(matches(isEnabled(true)))
        Log.i(TAG, "verifyOpenLinksInPrivateTabEnabled: Verified that the \"Open links in a private tab\" toggle is enabled")
    }

    fun verifyOpenLinksInPrivateTabOff() {
        assertUIObjectExists(
            checkedItemWithResId("android:id/switch_widget", isChecked = true),
            exists = false,
        )
        Log.i(TAG, "verifyOpenLinksInPrivateTabOff: Trying to verify that the \"Open links in a private tab\" toggle is visible")
        openLinksInPrivateTabSwitch()
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifyOpenLinksInPrivateTabOff: Verified that the \"Open links in a private tab\" toggle is visible")
    }

    fun verifyPrivateBrowsingShortcutIcon() {
        Log.i(TAG, "verifyPrivateBrowsingShortcutIcon: Waiting for $waitingTime ms until finding the \"Private $appName\" shortcut icon")
        mDevice.wait(Until.findObject(text("Private $appName")), waitingTime)
        Log.i(TAG, "verifyPrivateBrowsingShortcutIcon: Waited for $waitingTime ms until the \"Private $appName\" shortcut icon was found")
        Log.i(TAG, "verifyPrivateBrowsingShortcutIcon: Trying to verify the \"Private $appName\" shortcut icon")
        assertTrue("\"Private $appName\" shortcut icon wasn't verified", mDevice.hasObject(text("Private $appName")))
        Log.i(TAG, "verifyPrivateBrowsingShortcutIcon: Verified the \"Private $appName\" shortcut icon")
    }

    fun clickPrivateModeScreenshotsSwitch() {
        Log.i(TAG, "clickPrivateModeScreenshotsSwitch: Trying to click the \"Allow screenshots in private browsing\" toggle")
        screenshotsInPrivateModeSwitch().click()
        Log.i(TAG, "clickPrivateModeScreenshotsSwitch: Clicked the \"Allow screenshots in private browsing\" toggle")
    }

    fun clickOpenLinksInPrivateTabSwitch() {
        Log.i(TAG, "clickOpenLinksInPrivateTabSwitch: Trying to click the \"Open links in a private tab\" toggle")
        openLinksInPrivateTabSwitch().click()
        Log.i(TAG, "clickOpenLinksInPrivateTabSwitch: Clicked the \"Open links in a private tab\" toggle")
    }

    fun cancelPrivateShortcutAddition() {
        Log.i(TAG, "cancelPrivateShortcutAddition: Waiting for $waitingTime ms until finding the \"Add private browsing shortcut\" button")
        mDevice.wait(
            Until.findObject(text("Add private browsing shortcut")),
            waitingTime,
        )
        Log.i(TAG, "cancelPrivateShortcutAddition: Waited for $waitingTime ms until the \"Add private browsing shortcut\" button was found")
        Log.i(TAG, "cancelPrivateShortcutAddition: Trying to click the \"Add private browsing shortcut\" button")
        addPrivateBrowsingShortcutButton().click()
        Log.i(TAG, "cancelPrivateShortcutAddition: Clicked the \"Add private browsing shortcut\" button")
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Log.i(TAG, "cancelPrivateShortcutAddition: Waiting for $waitingTime ms until finding the \"Cancel\" button")
            mDevice.wait(Until.findObject(By.textContains("CANCEL")), waitingTime)
            Log.i(TAG, "cancelPrivateShortcutAddition: Waited for $waitingTime ms until the \"Cancel\" button was found")
            Log.i(TAG, "cancelPrivateShortcutAddition: Trying to click the \"Cancel\" button")
            cancelShortcutAdditionButton().click()
            Log.i(TAG, "cancelPrivateShortcutAddition: Clicked the \"Cancel\" button")
        }
    }

    fun addPrivateShortcutToHomescreen() {
        Log.i(TAG, "addPrivateShortcutToHomescreen: Waiting for $waitingTime ms until finding the \"Add private browsing shortcut\" button")
        mDevice.wait(
            Until.findObject(text("Add private browsing shortcut")),
            waitingTime,
        )
        Log.i(TAG, "addPrivateShortcutToHomescreen: Waited for $waitingTime ms until the \"Add private browsing shortcut\" button was found")
        Log.i(TAG, "addPrivateShortcutToHomescreen: Trying to click the \"Add private browsing shortcut\" button")
        addPrivateBrowsingShortcutButton().click()
        Log.i(TAG, "addPrivateShortcutToHomescreen: Clicked the \"Add private browsing shortcut\" button")
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Log.i(TAG, "addPrivateShortcutToHomescreen: Waiting for $waitingTime ms until finding the \"Add automatically\" button")
            mDevice.wait(Until.findObject(By.textContains("add automatically")), waitingTime)
            Log.i(TAG, "addPrivateShortcutToHomescreen: Waited for $waitingTime ms until the \"Add automatically\" button was found")
            Log.i(TAG, "addPrivateShortcutToHomescreen: Trying to click the \"Add automatically\" button")
            addAutomaticallyButton().click()
            Log.i(TAG, "addPrivateShortcutToHomescreen: Clicked the \"Add automatically\" button")
        }
    }

    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            Log.i(TAG, "goBack: Trying to click the navigate up button")
            goBackButton().perform(ViewActions.click())
            Log.i(TAG, "goBack: Clicked the navigate up button")

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }

        fun openPrivateBrowsingShortcut(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            Log.i(TAG, "openPrivateBrowsingShortcut: Trying to click the \"Private $appName\" shortcut icon")
            privateBrowsingShortcutIcon().click()
            Log.i(TAG, "openPrivateBrowsingShortcut: Clicked the \"Private $appName\" shortcut icon")

            SearchRobot().interact()
            return SearchRobot.Transition()
        }
    }
}

private fun openLinksInPrivateTabSwitch() =
    onView(withText("Open links in a private tab"))

private fun screenshotsInPrivateModeSwitch() =
    onView(withText("Allow screenshots in private browsing"))

private fun addPrivateBrowsingShortcutButton() = onView(withText("Add private browsing shortcut"))

private fun goBackButton() = onView(withContentDescription("Navigate up"))

private fun addAutomaticallyButton() =
    mDevice.findObject(UiSelector().textStartsWith("add automatically"))

private fun cancelShortcutAdditionButton() =
    mDevice.findObject(UiSelector().textContains("CANCEL"))

private fun privateBrowsingShortcutIcon() = mDevice.findObject(text("Private $appName"))
