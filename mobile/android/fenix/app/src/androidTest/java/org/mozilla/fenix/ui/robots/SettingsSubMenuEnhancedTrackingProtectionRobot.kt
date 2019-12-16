/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.withResourceName
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import org.hamcrest.CoreMatchers
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.isChecked

/**
 * Implementation of Robot Pattern for the settings Enhanced Tracking Protection sub menu.
 */
class SettingsSubMenuEnhancedTrackingProtectionRobot {

    fun verifyEnhancedTrackingProtectionHeader() = assertEnhancedTrackingProtectionHeader()

    fun verifyEnhancedTrackingProtectionOptions() = assertEnhancedTrackingProtectionOptions()

    fun verifyEnhancedTrackingProtectionDefaults() = assertEnhancedTrackingProtectionDefaults()

    class Transition {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())!!

        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            mDevice.waitForIdle()
            goBackButton().click()

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }

        fun openExceptions(interact: SettingsSubMenuEnhancedTrackingProtectionExceptionsRobot.() -> Unit): SettingsSubMenuEnhancedTrackingProtectionExceptionsRobot.Transition {
            openExceptions().click()

            SettingsSubMenuEnhancedTrackingProtectionExceptionsRobot().interact()
            return SettingsSubMenuEnhancedTrackingProtectionExceptionsRobot.Transition()
        }
    }
}

private fun assertEnhancedTrackingProtectionHeader() {
    onView(ViewMatchers.withText("Browse without being followed"))
        .check(matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
}

private fun assertEnhancedTrackingProtectionOptions() {
    onView(ViewMatchers.withText("Standard"))
        .check(matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    val stdText = "Pages will load normally, but block fewer trackers."
    onView(ViewMatchers.withText(stdText))
        .check(matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

    onView(ViewMatchers.withText("Strict (Default)"))
        .check(matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    val summaryText =
        "Stronger tracking protection and faster performance, but some sites may not work properly."
    onView(ViewMatchers.withText(summaryText))
        .check(matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
}

private fun assertEnhancedTrackingProtectionDefaults() {
    onView(withResourceName("switch_widget")).check(
        matches(
            isChecked(
                true
            )
        )
    )
}

fun settingsSubMenuEnhancedTrackingProtection(interact: SettingsSubMenuEnhancedTrackingProtectionRobot.() -> Unit): SettingsSubMenuEnhancedTrackingProtectionRobot.Transition {
    SettingsSubMenuEnhancedTrackingProtectionRobot().interact()
    return SettingsSubMenuEnhancedTrackingProtectionRobot.Transition()
}

private fun goBackButton() =
    onView(CoreMatchers.allOf(ViewMatchers.withContentDescription("Navigate up")))

private fun openExceptions() =
    onView(CoreMatchers.allOf(ViewMatchers.withText("Exceptions")))
