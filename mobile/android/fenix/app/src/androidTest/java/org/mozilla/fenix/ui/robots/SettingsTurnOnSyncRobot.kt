/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import org.hamcrest.CoreMatchers
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the settings turn on sync option.
 */
class SettingsTurnOnSyncRobot {
    fun verifyUseEmailOption() = assertUseEmailField()

    fun verifyReadyToScanOption() = assertReadyToScan()

    fun tapOnUseEmailToSignIn() = useEmailButton().click()

    class Transition {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

        fun goBack(interact: SettingsSubMenuLoginsAndPasswordRobot.() -> Unit): SettingsSubMenuLoginsAndPasswordRobot.Transition {
            goBackButton().perform(ViewActions.click())

            SettingsSubMenuLoginsAndPasswordRobot().interact()
            return SettingsSubMenuLoginsAndPasswordRobot.Transition()
        }
    }
}

private fun goBackButton() =
        Espresso.onView(CoreMatchers.allOf(ViewMatchers.withContentDescription("Navigate up")))

private fun assertUseEmailField() = Espresso.onView(ViewMatchers.withText("Use email instead"))
        .check(ViewAssertions.matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertReadyToScan() = Espresso.onView(ViewMatchers.withText("Ready to scan"))
        .check(ViewAssertions.matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun useEmailButton() = Espresso.onView(ViewMatchers.withText("Use email instead"))
