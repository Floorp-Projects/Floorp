/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import org.hamcrest.CoreMatchers.allOf
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the settings Theme sub menu.
 */
class SettingsSubMenuThemeRobot {

    fun verifyThemes() = assertThemes()

    fun verifyLightThemeApplied(expected: Boolean) = assertFalse("Light theme not selected", expected)

    fun verifyDarkThemeApplied(expected: Boolean) = assertTrue("Dark theme not selected", expected)

    fun selectDarkMode() = darkModeToggle().click()

    fun selectLightMode() = lightModeToggle().click()

    class Transition {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            mDevice.waitForIdle()
            goBackButton().perform(ViewActions.click())

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

private fun assertThemes() {
    onView(withText("Light"))
        .check(ViewAssertions.matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    onView(withText("Dark"))
        .check(ViewAssertions.matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    // Conditionally unavailable on API 25
    // onView(ViewMatchers.withText("Follow device theme"))
    //    .check(ViewAssertions.matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
}

private fun goBackButton() =
    onView(allOf(ViewMatchers.withContentDescription("Navigate up")))

private fun darkModeToggle() = onView(withText("Dark"))

private fun lightModeToggle() = onView(withText("Light"))
