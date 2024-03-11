/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import org.hamcrest.CoreMatchers
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the URL toolbar.
 */
class AccountSettingsRobot {
    fun verifyBookmarksCheckbox() =
        bookmarksCheckbox().check(
            matches(
                withEffectiveVisibility(
                    ViewMatchers.Visibility.VISIBLE,
                ),
            ),
        )

    fun verifyHistoryCheckbox() =
        historyCheckbox().check(
            matches(
                withEffectiveVisibility(
                    ViewMatchers.Visibility.VISIBLE,
                ),
            ),
        )

    fun verifySignOutButton() =
        signOutButton().check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    fun verifyDeviceName() = deviceName().check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

    class Transition {

        fun disconnectAccount(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            signOutButton().click()
            disconnectButton().click()

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

fun accountSettings(interact: AccountSettingsRobot.() -> Unit): AccountSettingsRobot.Transition {
    AccountSettingsRobot().interact()
    return AccountSettingsRobot.Transition()
}

private fun bookmarksCheckbox() = Espresso.onView(CoreMatchers.allOf(ViewMatchers.withText("Bookmarks")))
private fun historyCheckbox() = Espresso.onView(CoreMatchers.allOf(ViewMatchers.withText("History")))

private fun signOutButton() = Espresso.onView(CoreMatchers.allOf(ViewMatchers.withText("Sign out")))
private fun deviceName() = Espresso.onView(CoreMatchers.allOf(ViewMatchers.withText("Device name")))

private fun disconnectButton() = Espresso.onView(CoreMatchers.allOf(ViewMatchers.withId(R.id.signOutDisconnect)))
