/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.content.Context
import android.util.Log
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
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
import org.mozilla.fenix.helpers.Constants.TAG
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
        mDevice.waitNotNull(Until.findObjects(By.text("Save passwords")), TestAssetHelper.waitingTime)
        Log.i(TAG, "verifyDefaultView: Trying to verify that the \"Save logins and passwords\" button is displayed")
        saveLoginsAndPasswordButton().check(matches(isDisplayed()))
        Log.i(TAG, "verifyDefaultView: Verified that the \"Save passwords\" button is displayed")
        Log.i(TAG, "verifyDefaultView: Trying to verify that the Autofill in Firefox option is displayed")
        autofillInFirefoxOption().check(matches(isDisplayed()))
        Log.i(TAG, "verifyDefaultView: Verified that the Autofill in Firefox option is displayed")
        Log.i(TAG, "verifyDefaultView: Trying to verify that the \"Autofill in other apps\" option is displayed")
        autofillInOtherAppsOption().check(matches(isDisplayed()))
        Log.i(TAG, "verifyDefaultView: Verified that the \"Autofill in other apps\" option is displayed")
        Log.i(TAG, "verifyDefaultView: Trying to verify that the \"Sync logins across devices\" button is displayed")
        syncLoginsButton().check(matches(isDisplayed()))
        Log.i(TAG, "verifyDefaultView: Verified that the \"Sync logins across devices\" button is displayed")
        Log.i(TAG, "verifyDefaultView: Trying to verify that the \"Saved logins\" button is displayed")
        savedLoginsButton().check(matches(isDisplayed()))
        Log.i(TAG, "verifyDefaultView: Verified that the \"Saved logins\" button is displayed")
        Log.i(TAG, "verifyDefaultView: Trying to verify that the \"Exceptions\" button is displayed")
        loginExceptionsButton().check(matches(isDisplayed()))
        Log.i(TAG, "verifyDefaultView: Verified that the \"Exceptions\" button is displayed")
    }

    fun verifyDefaultViewBeforeSyncComplete() {
        mDevice.waitNotNull(Until.findObjects(By.text("Off")), TestAssetHelper.waitingTime)
    }

    fun verifyDefaultViewAfterSync() {
        mDevice.waitNotNull(Until.findObjects(By.text("On")), TestAssetHelper.waitingTime)
    }

    fun verifyDefaultValueAutofillLogins(context: Context) {
        Log.i(TAG, "verifyDefaultValueAutofillLogins: Trying to verify that the Autofill in Firefox option is displayed")
        onView(
            withText(
                context.getString(
                    R.string.preferences_passwords_autofill2,
                    context.getString(R.string.app_name),
                ),
            ),
        ).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyDefaultValueAutofillLogins: Verified that the Autofill in Firefox option is displayed")
    }

    fun clickAutofillInFirefoxOption() {
        Log.i(TAG, "clickAutofillInFirefoxOption: Trying to click the Autofill in Firefox option")
        autofillInFirefoxOption().click()
        Log.i(TAG, "clickAutofillInFirefoxOption: Clicked the Autofill in Firefox option")
    }

    fun verifyAutofillInFirefoxToggle(enabled: Boolean) {
        Log.i(TAG, "verifyAutofillInFirefoxToggle: Trying to verify that the Autofill in Firefox toggle is enabled: $enabled")
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
        Log.i(TAG, "verifyAutofillInFirefoxToggle: Verified that the Autofill in Firefox toggle is enabled: $enabled")
    }
    fun verifyAutofillLoginsInOtherAppsToggle(enabled: Boolean) {
        Log.i(TAG, "verifyAutofillLoginsInOtherAppsToggle: Trying to verify that the \"Autofill in other apps\" toggle is enabled: $enabled")
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
        Log.i(TAG, "verifyAutofillLoginsInOtherAppsToggle: Verified that the \"Autofill in other apps\" toggle is enabled: $enabled")
    }

    class Transition {

        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            Log.i(TAG, "goBack: Trying to click the navigate up button")
            goBackButton().perform(click())
            Log.i(TAG, "goBack: Clicked the navigate up button")

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }

        fun openSavedLogins(interact: SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot.() -> Unit): SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot.Transition {
            Log.i(TAG, "openSavedLogins: Trying to click the \"Saved logins\" button")
            savedLoginsButton().click()
            Log.i(TAG, "openSavedLogins: Clicked the \"Saved logins\" button")

            SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot().interact()
            return SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot.Transition()
        }

        fun openLoginExceptions(interact: SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot.() -> Unit): SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot.Transition {
            Log.i(TAG, "openLoginExceptions: Trying to click the \"Exceptions\" button")
            loginExceptionsButton().click()
            Log.i(TAG, "openLoginExceptions: Clicked the \"Exceptions\" button")

            SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot().interact()
            return SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot.Transition()
        }

        fun openSyncLogins(interact: SettingsTurnOnSyncRobot.() -> Unit): SettingsTurnOnSyncRobot.Transition {
            Log.i(TAG, "openSyncLogins: Trying to click the \"Sync logins across devices\" button")
            syncLoginsButton().click()
            Log.i(TAG, "openSyncLogins: Clicked the \"Sync logins across devices\" button")

            SettingsTurnOnSyncRobot().interact()
            return SettingsTurnOnSyncRobot.Transition()
        }

        fun openSaveLoginsAndPasswordsOptions(interact: SettingsSubMenuLoginsAndPasswordOptionsToSaveRobot.() -> Unit): SettingsSubMenuLoginsAndPasswordOptionsToSaveRobot.Transition {
            Log.i(TAG, "openSaveLoginsAndPasswordsOptions: Trying to click the \"Save logins and passwords\" button")
            saveLoginsAndPasswordButton().click()
            Log.i(TAG, "openSaveLoginsAndPasswordsOptions: Clicked the \"Save logins and passwords\" button")

            SettingsSubMenuLoginsAndPasswordOptionsToSaveRobot().interact()
            return SettingsSubMenuLoginsAndPasswordOptionsToSaveRobot.Transition()
        }
    }
}

fun settingsSubMenuLoginsAndPassword(interact: SettingsSubMenuLoginsAndPasswordRobot.() -> Unit): SettingsSubMenuLoginsAndPasswordRobot.Transition {
    SettingsSubMenuLoginsAndPasswordRobot().interact()
    return SettingsSubMenuLoginsAndPasswordRobot.Transition()
}

private fun saveLoginsAndPasswordButton() = onView(withText("Save passwords"))

private fun savedLoginsButton() = onView(withText("Saved passwords"))

private fun syncLoginsButton() = onView(withText("Sync passwords across devices"))

private fun loginExceptionsButton() = onView(withText("Exceptions"))

private fun goBackButton() =
    onView(allOf(ViewMatchers.withContentDescription("Navigate up")))

private fun autofillInFirefoxOption() = onView(withText("Autofill in $appName"))

private fun autofillInOtherAppsOption() = onView(withText("Autofill in other apps"))
