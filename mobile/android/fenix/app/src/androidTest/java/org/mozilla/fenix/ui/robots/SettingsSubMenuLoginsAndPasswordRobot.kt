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
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.Until
import org.hamcrest.CoreMatchers
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull

/**
 * Implementation of Robot Pattern for the Privacy Settings > logins and passwords sub menu
 */
class SettingsSubMenuLoginsAndPasswordRobot {

    fun verifyDefaultView() {
        mDevice.waitNotNull(Until.findObjects(By.text("Sync logins across devices")), TestAssetHelper.waitingTime)
        assertDefaultView()
    }

    fun verifyDefaultViewBeforeSyncComplete() {
        mDevice.waitNotNull(Until.findObjects(By.text("Off")), TestAssetHelper.waitingTime)
    }

    fun verifyDefaultViewAfterSync() {
        mDevice.waitNotNull(Until.findObjects(By.text("On")), TestAssetHelper.waitingTime)
    }

    fun verifyDefaultValueExceptions() = assertDefaultValueExceptions()

    fun verifyDefaultValueAutofillLogins() = assertDefaultValueAutofillLogins()

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

        fun openLoginExceptions(interact: SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot.() -> Unit): SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot.Transition {
            fun loginExceptionsButton() = onView(ViewMatchers.withText("Exceptions"))
            loginExceptionsButton().click()

            SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot().interact()
            return SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot.Transition()
        }

        fun openSyncLogins(interact: SettingsTurnOnSyncRobot.() -> Unit): SettingsTurnOnSyncRobot.Transition {
            fun syncLoginsButton() = onView(ViewMatchers.withText("Sync logins across devices"))
            syncLoginsButton().click()

            SettingsTurnOnSyncRobot().interact()
            return SettingsTurnOnSyncRobot.Transition()
        }

        fun saveLoginsAndPasswordsOptions(interact: SettingsSubMenuLoginsAndPasswordOptionsToSaveRobot.() -> Unit): SettingsSubMenuLoginsAndPasswordOptionsToSaveRobot.Transition {
            fun saveLoginsAndPasswordButton() = onView(ViewMatchers.withText("Save logins and passwords"))
            saveLoginsAndPasswordButton().click()

            SettingsSubMenuLoginsAndPasswordOptionsToSaveRobot().interact()
            return SettingsSubMenuLoginsAndPasswordOptionsToSaveRobot.Transition()
        }
    }
}

fun settingsSubMenuLoginsAndPassword(interact: SettingsSubMenuLoginsAndPasswordRobot.() -> Unit): SettingsSubMenuLoginsAndPasswordRobot.Transition {
    SettingsSubMenuLoginsAndPasswordRobot().interact()
    return SettingsSubMenuLoginsAndPasswordRobot.Transition()
}

private fun goBackButton() =
        onView(CoreMatchers.allOf(ViewMatchers.withContentDescription("Navigate up")))

private fun assertDefaultView() = onView(ViewMatchers.withText("Sync logins across devices"))
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertDefaultValueAutofillLogins() = onView(ViewMatchers.withText("Autofill"))
    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertDefaultValueExceptions() = onView(ViewMatchers.withText("Exceptions"))
    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertDefaultValueSyncLogins() = onView(ViewMatchers.withText("Sign in to Sync"))
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
