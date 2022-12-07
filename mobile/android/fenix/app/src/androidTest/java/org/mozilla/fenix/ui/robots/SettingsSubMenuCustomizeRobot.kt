/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.os.Build
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.ViewInteraction
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.withText
import org.hamcrest.CoreMatchers.allOf
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the settings Theme sub menu.
 */
class SettingsSubMenuCustomizeRobot {

    fun verifyThemes() = assertThemes()

    fun verifyLightThemeApplied(expected: Boolean) =
        assertFalse("Light theme not selected", expected)

    fun verifyDarkThemeApplied(expected: Boolean) = assertTrue("Dark theme not selected", expected)

    fun selectDarkMode() = darkModeToggle().click()

    fun selectLightMode() = lightModeToggle().click()

    fun clickTopToolbarToggle() = topToolbarToggle().click()

    fun clickBottomToolbarToggle() = bottomToolbarToggle().click()

    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            mDevice.waitForIdle()
            goBackButton().perform(ViewActions.click())

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

private fun assertThemes() {
    lightModeToggle()
        .check(ViewAssertions.matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    darkModeToggle()
        .check(ViewAssertions.matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    deviceModeToggle()
        .check(ViewAssertions.matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
}

private fun darkModeToggle() = onView(withText("Dark"))

private fun lightModeToggle() = onView(withText("Light"))

private fun topToolbarToggle() = onView(withText("Top"))

private fun bottomToolbarToggle() = onView(withText("Bottom"))

private fun deviceModeToggle(): ViewInteraction {
    val followDeviceThemeText =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) "Follow device theme" else "Set by Battery Saver"
    return onView(withText(followDeviceThemeText))
}

private fun goBackButton() =
    onView(allOf(ViewMatchers.withContentDescription("Navigate up")))
