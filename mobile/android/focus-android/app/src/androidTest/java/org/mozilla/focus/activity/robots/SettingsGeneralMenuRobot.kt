/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.ViewInteraction
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.intent.Intents.intended
import androidx.test.espresso.intent.matcher.IntentMatchers
import androidx.test.espresso.matcher.ViewMatchers.isChecked
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.isNotChecked
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import junit.framework.TestCase.assertTrue
import org.mozilla.focus.R
import org.mozilla.focus.helpers.TestHelper.appName
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.waitingTime

class SettingsGeneralMenuRobot {
    companion object {
        const val DEFAULT_APPS_SETTINGS_ACTION = "android.settings.MANAGE_DEFAULT_APPS_SETTINGS"
    }

    fun verifyGeneralSettingsItems() {
        defaultBrowserSwitch.check(matches(isDisplayed()))
        switchToNewTabToggleButton.check(matches(isDisplayed()))
    }

    fun clickSetDefaultBrowser() {
        defaultBrowserSwitch
                .check(matches(isDisplayed()))
                .perform(click())
    }

    fun verifyAndroidDefaultAppsMenuAppears() {
        intended(IntentMatchers.hasAction(DEFAULT_APPS_SETTINGS_ACTION))
    }

    fun verifyOpenWithDialog() {
        openWithDialogTitle.waitForExists(waitingTime)
        assertTrue(openWithDialogTitle.exists())
        assertTrue(openWithList.exists())
    }

    fun selectAlwaysOpenWithFocus() {
        mDevice.findObject(UiSelector().text(appName)).click()
        mDevice.findObject(
                UiSelector()
                        .resourceId("android:id/button_always")
                        .enabled(true)
        ).click()
    }

    fun verifySwitchIsToggled(checked: Boolean) {
        onView(withId(R.id.switch_widget)).check(matches(
                if (checked)
                    isChecked()
                else
                    isNotChecked()
        ))
    }

    fun openLanguageSelectionMenu(localizedText: String = "Language"): ViewInteraction =
        languageMenuButton(localizedText).perform(click())

    fun verifyLanguageSelected(value: String) {
        assertTrue(languageMenu.getChild(UiSelector().text(value)).isChecked)
    }

    fun selectLanguage(language: String) {
        languageMenu.scrollTextIntoView(language)
        onView(withText(language)).perform(click())
    }

    class Transition {
        // add here transitions to other robot classes
    }
}

private val defaultBrowserSwitch = onView(withText("Make Firefox Focus default browser"))

private val switchToNewTabToggleButton = onView(withText("Switch to link in new tab immediately"))

private val openWithDialogTitle = mDevice.findObject(
        UiSelector()
                .text("Open with")
)

private val openWithList = mDevice.findObject(
        UiSelector()
                .resourceId("android:id/resolver_list")
)

private fun languageMenuButton(localizedText: String) = onView(withText(localizedText))

private val languageMenu = UiScrollable(UiSelector().scrollable(true))
