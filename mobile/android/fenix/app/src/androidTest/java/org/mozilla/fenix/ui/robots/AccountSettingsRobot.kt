/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso
import androidx.test.espresso.assertion.ViewAssertions
import androidx.test.espresso.matcher.ViewMatchers
import org.hamcrest.CoreMatchers
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the URL toolbar.
 */
class AccountSettingsRobot {
    fun verifyBookmarksCheckbox() = assertBookmarksCheckbox()

    fun verifyHistoryCheckbox() = assertHistoryCheckbox()

    fun verifySignOutButton() = assertSignOutButton()
    fun verifyDeviceName() = assertDeviceName()

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

private fun assertBookmarksCheckbox() = bookmarksCheckbox().check(
    ViewAssertions.matches(
        ViewMatchers.withEffectiveVisibility(
            ViewMatchers.Visibility.VISIBLE
        )
    )
)

private fun assertHistoryCheckbox() = historyCheckbox().check(
    ViewAssertions.matches(
        ViewMatchers.withEffectiveVisibility(
            ViewMatchers.Visibility.VISIBLE
        )
    )
)

private fun assertSignOutButton() = signOutButton().check(ViewAssertions.matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
private fun assertDeviceName() = deviceName().check(ViewAssertions.matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
