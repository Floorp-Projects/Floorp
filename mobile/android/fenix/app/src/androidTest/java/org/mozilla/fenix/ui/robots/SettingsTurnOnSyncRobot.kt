/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withParent
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import org.hamcrest.CoreMatchers
import org.hamcrest.Matchers.allOf
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the settings turn on sync option.
 */
class SettingsTurnOnSyncRobot {
    fun verifyUseEmailOption() = assertUseEmailField()

    fun verifyReadyToScanOption() = assertReadyToScan()

    fun tapOnUseEmailToSignIn() = useEmailButton().click()

    fun verifyTurnOnSyncToolbarTitle() = assertTurnOnSyncToolbarTitle()

    class Transition {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

        fun goBack(interact: SettingsSubMenuLoginsAndPasswordRobot.() -> Unit): SettingsRobot.Transition {
            goBackButton().perform(ViewActions.click())

            SettingsSubMenuLoginsAndPasswordRobot().interact()
            return SettingsRobot.Transition()
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

private fun assertTurnOnSyncToolbarTitle() =
    onView(
        allOf(
            withParent(withId(R.id.navigationToolbar)),
            withText("Turn on Sync")
        )
    ).check(matches(isDisplayed()))
