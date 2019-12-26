/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import org.hamcrest.CoreMatchers
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the Privacy Settings > logins and passwords sub menu
 */
class SettingsSubMenuLoginsAndPasswordRobot {

    fun verifyDefaultView() = assertDefaultView()
    fun verifyDefaultValueSyncLogins() = asserDefaultValueSyncLogins()

    class Transition {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            goBackButton().perform(ViewActions.click())

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }

        fun openSavedLogins(interact: SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot.() -> Unit): SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot.Transition {
            fun savedLoginsButton() = onView(ViewMatchers.withText("Saved logins"))
            savedLoginsButton().click()

            SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot().interact()
            return SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot.Transition()
        }

        fun openSyncLogins(interact: SettingsTurnOnSyncRobot.() -> Unit): SettingsTurnOnSyncRobot.Transition {
            fun syncLoginsButton() = onView(ViewMatchers.withText("Sync logins"))
            syncLoginsButton().click()

            SettingsTurnOnSyncRobot().interact()
            return SettingsTurnOnSyncRobot.Transition()
        }
    }
}

private fun goBackButton() =
        onView(CoreMatchers.allOf(ViewMatchers.withContentDescription("Navigate up")))

private fun assertDefaultView() = onView(ViewMatchers.withText("Sync logins"))
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun asserDefaultValueSyncLogins() = onView(ViewMatchers.withText("Sign in to Sync"))
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
