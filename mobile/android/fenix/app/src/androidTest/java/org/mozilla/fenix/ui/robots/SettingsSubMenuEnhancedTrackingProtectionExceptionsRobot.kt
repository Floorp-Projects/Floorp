/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import org.hamcrest.CoreMatchers
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.R

/**
 * Implementation of Robot Pattern for the settings Enhanced Tracking Protection Exceptions sub menu.
 */
class SettingsSubMenuEnhancedTrackingProtectionExceptionsRobot {

    fun verifyDefault() = assertExceptionDefault()!!

    fun verifyListedURL(url: String) = assertExceptionURL(url)!!

    class Transition {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())!!

        fun goBack(interact: SettingsSubMenuEnhancedTrackingProtectionRobot.() -> Unit): SettingsSubMenuEnhancedTrackingProtectionRobot.Transition {
            mDevice.waitForIdle()
            goBackButton().click()

            SettingsSubMenuEnhancedTrackingProtectionRobot().interact()
            return SettingsSubMenuEnhancedTrackingProtectionRobot.Transition()
        }

        fun disableExceptions(interact: SettingsSubMenuEnhancedTrackingProtectionExceptionsRobot.() -> Unit): Transition {
            disableExceptionsButton().click()

            SettingsSubMenuEnhancedTrackingProtectionExceptionsRobot().interact()
            return Transition()
        }
    }
}

private fun goBackButton() =
    Espresso.onView(CoreMatchers.allOf(ViewMatchers.withContentDescription("Navigate up")))

private fun assertExceptionDefault() =
    Espresso.onView(CoreMatchers.allOf(ViewMatchers.withText("Exceptions let you disable tracking protection for selected sites.")))

private fun assertExceptionURL(url: String) =
    Espresso.onView(CoreMatchers.allOf(ViewMatchers.withText(url)))

private fun disableExceptionsButton() =
    Espresso.onView(CoreMatchers.allOf(ViewMatchers.withId(R.id.removeAllExceptions)))
