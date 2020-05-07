/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import org.hamcrest.CoreMatchers.allOf
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.R

/**
 * Implementation of Robot Pattern for the settings Enhanced Tracking Protection Exceptions sub menu.
 */
class SettingsSubMenuEnhancedTrackingProtectionExceptionsRobot {

    fun verifyNavigationToolBarHeader() = assertNavigationToolBarHeader()

    fun verifyDefault() = assertExceptionDefault()!!

    fun verifyExceptionLearnMoreText() = assertExceptionLearnMoreText()!!

    fun verifyListedURL(url: String) = assertExceptionURL(url)!!

    fun verifyEnhancedTrackingProtectionProtectionExceptionsSubMenuItems() {
        verifyDefault()
        verifyExceptionLearnMoreText()
    }

    class Transition {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())!!

        fun goBack(interact: SettingsSubMenuEnhancedTrackingProtectionRobot.() -> Unit): SettingsSubMenuEnhancedTrackingProtectionRobot.Transition {
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
    onView(allOf(withContentDescription("Navigate up")))

private fun assertNavigationToolBarHeader() {
    onView(withText(R.string.preferences_tracking_protection_exceptions))
        .check((matches(withEffectiveVisibility(Visibility.VISIBLE))))
}

private fun assertExceptionDefault() =
    onView(allOf(withText(R.string.exceptions_empty_message_description)))

private fun assertExceptionLearnMoreText() =
    onView(allOf(withText(R.string.exceptions_empty_message_learn_more_link)))

private fun assertExceptionURL(url: String) =
    onView(allOf(withText(url)))

private fun disableExceptionsButton() =
    onView(allOf(withId(R.id.removeAllExceptions)))
