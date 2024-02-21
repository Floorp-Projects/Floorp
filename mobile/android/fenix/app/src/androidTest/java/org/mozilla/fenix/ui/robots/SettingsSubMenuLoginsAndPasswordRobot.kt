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
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.isNotChecked
import androidx.test.espresso.matcher.ViewMatchers.withClassName
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.Until
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
        mDevice.waitNotNull(Until.findObjects(By.text("Save logins and passwords")), TestAssetHelper.waitingTime)
        saveLoginsAndPasswordButton().check(matches(isDisplayed()))
        autofillInFirefoxOption().check(matches(isDisplayed()))
        autofillInOtherAppsOption().check(matches(isDisplayed()))
        syncLoginsButton().check(matches(isDisplayed()))
        savedLoginsButton().check(matches(isDisplayed()))
        loginExceptionsButton().check(matches(isDisplayed()))
    }

    fun verifyDefaultViewBeforeSyncComplete() {
        mDevice.waitNotNull(Until.findObjects(By.text("Off")), TestAssetHelper.waitingTime)
    }

    fun verifyDefaultViewAfterSync() {
        mDevice.waitNotNull(Until.findObjects(By.text("On")), TestAssetHelper.waitingTime)
    }

    fun verifyDefaultValueAutofillLogins(context: Context) = assertDefaultValueAutofillLogins(context)

    fun clickAutofillInFirefoxOption() = autofillInFirefoxOption().click()

    fun verifyAutofillInFirefoxToggle(enabled: Boolean) {
        autofillInFirefoxOption()
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
    }
    fun verifyAutofillLoginsInOtherAppsToggle(enabled: Boolean) {
        autofillInOtherAppsOption()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switch_widget),
                            if (enabled) {
                                isChecked()
                            } else {
                                isNotChecked()
                            },
                        ),
                    ),
                ),
            )
    }

    class Transition {

        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            goBackButton().perform(ViewActions.click())

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }

        fun openSavedLogins(interact: SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot.() -> Unit): SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot.Transition {
            savedLoginsButton().click()

            SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot().interact()
            return SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot.Transition()
        }

        fun openLoginExceptions(interact: SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot.() -> Unit): SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot.Transition {
            loginExceptionsButton().click()

            SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot().interact()
            return SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot.Transition()
        }

        fun openSyncLogins(interact: SettingsTurnOnSyncRobot.() -> Unit): SettingsTurnOnSyncRobot.Transition {
            syncLoginsButton().click()

            SettingsTurnOnSyncRobot().interact()
            return SettingsTurnOnSyncRobot.Transition()
        }

        fun openSaveLoginsAndPasswordsOptions(interact: SettingsSubMenuLoginsAndPasswordOptionsToSaveRobot.() -> Unit): SettingsSubMenuLoginsAndPasswordOptionsToSaveRobot.Transition {
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

private fun saveLoginsAndPasswordButton() = onView(withText("Save logins and passwords"))

private fun savedLoginsButton() = onView(withText("Saved logins"))

private fun syncLoginsButton() = onView(withText("Sync logins across devices"))

private fun loginExceptionsButton() = onView(withText("Exceptions"))

private fun goBackButton() =
    onView(allOf(ViewMatchers.withContentDescription("Navigate up")))

private fun assertDefaultValueAutofillLogins(context: Context) = onView(
    ViewMatchers.withText(
        context.getString(
            R.string.preferences_passwords_autofill2,
            context.getString(R.string.app_name),
        ),
    ),
)
    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun autofillInFirefoxOption() = onView(withText("Autofill in $appName"))

private fun autofillInOtherAppsOption() = onView(withText("Autofill in other apps"))
