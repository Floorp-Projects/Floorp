/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity.robots

import android.os.Build
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.ViewInteraction
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.intent.Intents.intended
import androidx.test.espresso.intent.matcher.IntentMatchers
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.isChecked
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.isNotChecked
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import junit.framework.TestCase.assertTrue
import org.hamcrest.Matchers.allOf
import org.junit.Assert.assertFalse
import org.mozilla.focus.R
import org.mozilla.focus.helpers.EspressoHelper.hasCousin
import org.mozilla.focus.helpers.TestHelper.appName
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.waitingTime

class SettingsGeneralMenuRobot {
    companion object {
        const val ACTION_REQUEST_ROLE = "android.app.role.action.REQUEST_ROLE"
    }

    fun verifyGeneralSettingsItems(defaultBrowserSwitchState: Boolean = false) {
        verifyThemesList()
        languageMenuButton().check(matches(isDisplayed()))
        defaultBrowserSwitch().check(matches(isDisplayed()))
        assertDefaultBrowserSwitchState(defaultBrowserSwitchState)
    }

    fun clickSetDefaultBrowser() {
        defaultBrowserSwitch()
            .check(matches(isDisplayed()))
            .perform(click())
    }

    fun verifyAndroidDefaultAppsMenuAppears() {
        // method used to assert the default apps menu on API 24 and above
        when (Build.VERSION.SDK_INT) {
            in Build.VERSION_CODES.N..Build.VERSION_CODES.P ->
                assertTrue(
                    mDevice.findObject(UiSelector().resourceId("com.android.settings:id/list"))
                        .waitForExists(waitingTime)
                )
            in Build.VERSION_CODES.Q..Build.VERSION_CODES.R ->
                intended(IntentMatchers.hasAction(ACTION_REQUEST_ROLE))
        }
    }

    fun selectFocusDefaultBrowser() {
        // method used to set default browser on API 30 and above
        mDevice.findObject(UiSelector().text(appName)).click()
        mDevice.findObject(UiSelector().textContains("Set as default")).click()
    }

    fun verifySwitchIsToggled(checked: Boolean) {
        onView(withId(R.id.switch_widget)).check(
            matches(
                if (checked)
                    isChecked()
                else
                    isNotChecked()
            )
        )
    }

    fun openLanguageSelectionMenu(localizedText: String = "Language"): ViewInteraction =
        languageMenuButton(localizedText).perform(click())

    fun verifySystemLocaleSelected(localizedText: String = "System default") {
        assertTrue(
            languageMenu.getChild(
                UiSelector()
                    .text(localizedText)
                    .index(1)
            ).getFromParent(
                UiSelector().index(0)
            ).isChecked
        )
    }

    fun selectLanguage(language: String) {
        // Due to scrolling issues on Compose LazyList, avoid setting a language that is under the fold.
        // see https://github.com/mozilla-mobile/focus-android/issues/7282
        languageMenu
            .getChildByText(UiSelector().text(language), language, true)
            .click()
    }

    fun verifyThemesList() {
        darkThemeToggle.check(matches(isDisplayed()))
        lightThemeToggle.check(matches(isDisplayed()))
        deviceThemeToggle
            .check(matches(isDisplayed()))
            .check(matches(isChecked()))
    }

    fun verifyThemeApplied(isDarkTheme: Boolean = false, isLightTheme: Boolean = false, getThemeState: Boolean) {
        when {
            // getUiTheme() returns true if dark is applied
            isDarkTheme -> assertTrue("Dark theme not applied", getThemeState)
            // getUiTheme() returns false if light is applied
            isLightTheme -> assertFalse("Light theme not applied", getThemeState)
        }
    }

    fun selectDarkTheme() = darkThemeToggle.perform(click())

    fun selectLightTheme() = lightThemeToggle.perform(click())

    fun selectDeviceTheme() = deviceThemeToggle.perform(click())

    class Transition {
        // add here transitions to other robot classes
    }
}

private fun defaultBrowserSwitch() = onView(withText("Make $appName default browser"))

private fun assertDefaultBrowserSwitchState(enabled: Boolean) {
    if (enabled) {
        defaultBrowserSwitch()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switch_widget),
                            isChecked()
                        )
                    )
                )
            )
    } else {
        defaultBrowserSwitch()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switch_widget),
                            isNotChecked()
                        )
                    )
                )
            )
    }
}

private val openWithDialogTitle = mDevice.findObject(
    UiSelector()
        .text("Open with")
)

private val openWithList = mDevice.findObject(
    UiSelector()
        .resourceId("android:id/resolver_list")
)

private fun languageMenuButton(localizedText: String = "Language") = onView(withText(localizedText))

private val languageMenu = UiScrollable(UiSelector().scrollable(true))

private val darkThemeToggle =
    onView(
        allOf(
            withId(R.id.radio_button),
            hasSibling(withText("Dark"))
        )
    )

private val lightThemeToggle =
    onView(
        allOf(
            withId(R.id.radio_button),
            hasSibling(withText("Light"))
        )
    )

private val deviceThemeToggle =
    onView(
        allOf(
            withId(R.id.radio_button),
            hasSibling(withText("Follow device theme"))
        )
    )
