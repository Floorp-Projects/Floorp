/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.util.Log
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.isChecked
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.CoreMatchers.not
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText

/**
 * Implementation of Robot Pattern for the Privacy Settings > saved logins sub menu
 */

class SettingsSubMenuLoginsAndPasswordOptionsToSaveRobot {
    fun verifySaveLoginsOptionsView() {
        Log.i(TAG, "verifySaveLoginsOptionsView: Trying to verify that the \"Ask to save\" option is visible")
        onView(withText("Ask to save"))
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifySaveLoginsOptionsView: Verified that the \"Ask to save\" option is visible")
        Log.i(TAG, "verifySaveLoginsOptionsView: Trying to verify that the \"Never save\" option is visible")
        onView(withText("Never save"))
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifySaveLoginsOptionsView: Verified that the \"Never save\" option is visible")
    }

    fun verifyAskToSaveRadioButton(isChecked: Boolean) {
        if (isChecked) {
            Log.i(TAG, "verifyAskToSaveRadioButton: Trying to verify that the \"Ask to save\" option is checked")
            onView(
                allOf(
                    withId(R.id.radio_button),
                    hasSibling(withText(R.string.preferences_passwords_save_logins_ask_to_save)),
                ),
            ).check(matches(isChecked()))
            Log.i(TAG, "verifyAskToSaveRadioButton: Verified that the \"Ask to save\" option is checked")
        } else {
            Log.i(TAG, "verifyAskToSaveRadioButton: Trying to verify that the \"Ask to save\" option is not checked")
            onView(
                allOf(
                    withId(R.id.radio_button),
                    hasSibling(withText(R.string.preferences_passwords_save_logins_ask_to_save)),
                ),
            ).check(matches(not(isChecked())))
            Log.i(TAG, "verifyAskToSaveRadioButton: Verified that the \"Ask to save\" option is not checked")
        }
    }

    fun verifyNeverSaveSaveRadioButton(isChecked: Boolean) {
        if (isChecked) {
            Log.i(TAG, "verifyNeverSaveSaveRadioButton: Trying to verify that the \"Never save\" option is checked")
            onView(
                allOf(
                    withId(R.id.radio_button),
                    hasSibling(withText(R.string.preferences_passwords_save_logins_never_save)),
                ),
            ).check(matches(isChecked()))
            Log.i(TAG, "verifyNeverSaveSaveRadioButton: Verified that the \"Never save\" option is checked")
        } else {
            Log.i(TAG, "verifyNeverSaveSaveRadioButton: Trying to verify that the \"Never save\" option is not checked")
            onView(
                allOf(
                    withId(R.id.radio_button),
                    hasSibling(withText(R.string.preferences_passwords_save_logins_never_save)),
                ),
            ).check(matches(not(isChecked())))
            Log.i(TAG, "verifyNeverSaveSaveRadioButton: Verified that the \"Never save\" option is not checked")
        }
    }

    fun clickNeverSaveOption() {
        Log.i(TAG, "clickNeverSaveOption: Trying to click the \"Never save\" option")
        itemContainingText(getStringResource(R.string.preferences_passwords_save_logins_never_save)).click()
        Log.i(TAG, "clickNeverSaveOption: Clicked the \"Never save\" option")
    }

    class Transition {
        fun goBack(interact: SettingsSubMenuLoginsAndPasswordRobot.() -> Unit): SettingsSubMenuLoginsAndPasswordRobot.Transition {
            Log.i(TAG, "goBack: Trying to click the navigate up button")
            goBackButton().perform(ViewActions.click())
            Log.i(TAG, "goBack: Clicked the navigate up button")

            SettingsSubMenuLoginsAndPasswordRobot().interact()
            return SettingsSubMenuLoginsAndPasswordRobot.Transition()
        }
    }
}

private fun goBackButton() =
    onView(allOf(ViewMatchers.withContentDescription("Navigate up")))
