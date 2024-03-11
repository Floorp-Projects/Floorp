/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.util.Log
import androidx.test.espresso.Espresso
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import org.hamcrest.CoreMatchers
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the URL toolbar.
 */
class AccountSettingsRobot {
    fun verifyBookmarksCheckbox() {
        Log.i(TAG, "verifyBookmarksCheckbox: Trying to verify that the bookmarks check box is visible")
        bookmarksCheckbox().check(
            matches(
                withEffectiveVisibility(
                    Visibility.VISIBLE,
                ),
            ),
        )
        Log.i(TAG, "verifyBookmarksCheckbox: Verified that the bookmarks check box is visible")
    }

    fun verifyHistoryCheckbox() {
        Log.i(TAG, "verifyHistoryCheckbox: Trying to verify that the history check box is visible")
        historyCheckbox().check(
            matches(
                withEffectiveVisibility(
                    Visibility.VISIBLE,
                ),
            ),
        )
        Log.i(TAG, "verifyHistoryCheckbox: Verified that the history check box is visible")
    }

    fun verifySignOutButton() {
        Log.i(TAG, "verifySignOutButton: Trying to verify that the \"Sign out\" button is visible")
        signOutButton().check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifySignOutButton: Verified that the \"Sign out\" button is visible")
    }

    fun verifyDeviceName() {
        Log.i(TAG, "verifyDeviceName: Trying to verify that the \"Device name\" option is visible")
        deviceName().check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifyDeviceName: Verified that the \"Device name\" option is visible")
    }

    class Transition {

        fun disconnectAccount(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            Log.i(TAG, "disconnectAccount: Trying to click the \"Sign out\" button")
            signOutButton().click()
            Log.i(TAG, "disconnectAccount: Clicked the \"Sign out\" button")
            Log.i(TAG, "disconnectAccount: Trying to click the \"Disconnect\" button")
            disconnectButton().click()
            Log.i(TAG, "disconnectAccount: Clicked the \"Disconnect\" button")

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
