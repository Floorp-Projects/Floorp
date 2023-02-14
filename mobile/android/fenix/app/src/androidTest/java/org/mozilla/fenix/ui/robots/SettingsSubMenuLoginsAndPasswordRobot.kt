/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.content.Context
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.isChecked
import androidx.test.espresso.matcher.ViewMatchers.isNotChecked
import androidx.test.espresso.matcher.ViewMatchers.withClassName
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.Until
import org.hamcrest.CoreMatchers
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.CoreMatchers.endsWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.appName
import org.mozilla.fenix.helpers.TestHelper.hasCousin
import org.mozilla.fenix.helpers.TestHelper.mDevice
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

    fun verifyDefaultValueAutofillLogins(context: Context) = assertDefaultValueAutofillLogins(context)

    fun clickAutofillOption() = autofillOption.click()

    fun verifyAutofillToggle(enabled: Boolean) =
        autofillOption
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withClassName(endsWith("Switch")),
                            if (enabled) {
                                isChecked()
                            } else {
                                isNotChecked()
                            },
                        ),
                    ),
                ),
            )

    class Transition {

        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            goBackButton().perform(ViewActions.click())

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }

        fun openSavedLogins(interact: SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot.() -> Unit): SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot.Transition {
            fun savedLoginsButton() = onView(withText("Saved logins"))
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

        fun openSaveLoginsAndPasswordsOptions(interact: SettingsSubMenuLoginsAndPasswordOptionsToSaveRobot.() -> Unit): SettingsSubMenuLoginsAndPasswordOptionsToSaveRobot.Transition {
            fun saveLoginsAndPasswordButton() = onView(withText("Save logins and passwords"))
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

private fun assertDefaultValueAutofillLogins(context: Context) = onView(
    ViewMatchers.withText(
        context.getString(
            R.string.preferences_passwords_autofill2,
            context.getString(R.string.app_name),
        ),
    ),
)
    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertDefaultValueExceptions() = onView(ViewMatchers.withText("Exceptions"))
    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertDefaultValueSyncLogins() = onView(ViewMatchers.withText("Sync and save data"))
    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private val autofillOption = onView(withText("Autofill in $appName"))
