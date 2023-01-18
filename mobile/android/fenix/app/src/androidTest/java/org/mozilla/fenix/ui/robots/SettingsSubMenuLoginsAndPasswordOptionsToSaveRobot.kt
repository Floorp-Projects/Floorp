/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.isChecked
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import org.hamcrest.CoreMatchers
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.CoreMatchers.not
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.TestHelper.getStringResource

/**
 * Implementation of Robot Pattern for the Privacy Settings > saved logins sub menu
 */

class SettingsSubMenuLoginsAndPasswordOptionsToSaveRobot {
    fun verifySaveLoginsOptionsView() {
        onView(withText("Ask to save"))
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

        onView(withText("Never save"))
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    }

    fun verifyAskToSaveRadioButton(isChecked: Boolean) {
        if (isChecked) {
            onView(
                allOf(
                    withId(R.id.radio_button),
                    hasSibling(withText(R.string.preferences_passwords_save_logins_ask_to_save)),
                ),
            ).check(matches(isChecked()))
        } else {
            onView(
                allOf(
                    withId(R.id.radio_button),
                    hasSibling(withText(R.string.preferences_passwords_save_logins_ask_to_save)),
                ),
            ).check(matches(not(isChecked())))
        }
    }

    fun verifyNeverSaveSaveRadioButton(isChecked: Boolean) {
        if (isChecked) {
            onView(
                allOf(
                    withId(R.id.radio_button),
                    hasSibling(withText(R.string.preferences_passwords_save_logins_never_save)),
                ),
            ).check(matches(isChecked()))
        } else {
            onView(
                allOf(
                    withId(R.id.radio_button),
                    hasSibling(withText(R.string.preferences_passwords_save_logins_never_save)),
                ),
            ).check(matches(not(isChecked())))
        }
    }

    fun clickNeverSaveOption() =
        itemContainingText(getStringResource(R.string.preferences_passwords_save_logins_never_save)).click()

    class Transition {
        fun goBack(interact: SettingsSubMenuLoginsAndPasswordRobot.() -> Unit): SettingsSubMenuLoginsAndPasswordRobot.Transition {
            goBackButton().perform(ViewActions.click())

            SettingsSubMenuLoginsAndPasswordRobot().interact()
            return SettingsSubMenuLoginsAndPasswordRobot.Transition()
        }
    }
}

private fun goBackButton() =
    onView(CoreMatchers.allOf(ViewMatchers.withContentDescription("Navigate up")))
