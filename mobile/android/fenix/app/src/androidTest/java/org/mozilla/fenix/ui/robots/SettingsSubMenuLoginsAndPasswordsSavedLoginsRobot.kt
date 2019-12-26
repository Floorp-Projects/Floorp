/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import org.hamcrest.CoreMatchers

/**
 * Implementation of Robot Pattern for the Privacy Settings > saved logins sub menu
 */

class SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot {
    fun verifySavedLoginsView() = assertSavedLoginsView()

    fun tapSetupLater() = onView(ViewMatchers.withText("Later")).perform(ViewActions.click())

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

private fun assertSavedLoginsView() = onView(ViewMatchers.withText("Secure your logins and passwords"))
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
