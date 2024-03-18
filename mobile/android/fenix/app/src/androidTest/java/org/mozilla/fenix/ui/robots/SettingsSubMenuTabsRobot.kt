/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.util.Log
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import org.hamcrest.CoreMatchers.allOf
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.isChecked

/**
 * Implementation of Robot Pattern for the settings Tabs sub menu.
 */
class SettingsSubMenuTabsRobot {

    fun verifyTabViewOptions() {
        Log.i(TAG, "verifyTabViewOptions: Trying to verify that the \"Tab view\" title is visible")
        tabViewHeading()
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyTabViewOptions: Verified that the \"Tab view\" title is visible")
        Log.i(TAG, "verifyTabViewOptions: Trying to verify that the \"List\" option is visible")
        listToggle()
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyTabViewOptions: Verified that the \"List\" option is visible")
        Log.i(TAG, "verifyTabViewOptions: Trying to verify that the \"Grid\" option is visible")
        gridToggle()
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyTabViewOptions: Verified that the \"Grid\" option is visible")
    }

    fun verifyCloseTabsOptions() {
        Log.i(TAG, "verifyCloseTabsOptions: Trying to verify that the \"Close tabs\" title is visible")
        closeTabsHeading()
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyCloseTabsOptions: Verified that the \"Close tabs\" title is visible")
        Log.i(TAG, "verifyCloseTabsOptions: Trying to verify that the \"Never\" option is visible")
        neverOption()
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyCloseTabsOptions: Verified that the \"Never\" option is visible")
        Log.i(TAG, "verifyCloseTabsOptions: Trying to verify that the \"After one day\" option is visible")
        afterOneDayOption()
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyCloseTabsOptions: Verified that the \"After one day\" option is visible")
        Log.i(TAG, "verifyCloseTabsOptions: Trying to verify that the \"After one week\" option is visible")
        afterOneWeekOption()
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyCloseTabsOptions: Verified that the \"After one week\" option is visible")
        Log.i(TAG, "verifyCloseTabsOptions: Trying to verify that the \"After one month\" option is visible")
        afterOneMonthOption()
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyCloseTabsOptions: Verified that the \"After one month\" option is visible")
    }

    fun verifyMoveOldTabsToInactiveOptions() {
        Log.i(TAG, "verifyMoveOldTabsToInactiveOptions: Trying to verify that the \"Move old tabs to inactive\" title is visible")
        moveOldTabsToInactiveHeading()
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyMoveOldTabsToInactiveOptions: Verified that the \"Move old tabs to inactive\" title is visible")
        Log.i(TAG, "verifyMoveOldTabsToInactiveOptions: Trying to verify that the \"Move old tabs to inactive\" toggle is visible")
        moveOldTabsToInactiveToggle()
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyMoveOldTabsToInactiveOptions: Verified that the \"Move old tabs to inactive\" toggle is visible")
    }

    fun verifySelectedCloseTabsOption(closedTabsOption: String) {
        Log.i(TAG, "verifySelectedCloseTabsOption: Trying to verify that the $closedTabsOption radio button is checked")
        onView(
            allOf(
                withId(R.id.radio_button),
                hasSibling(withText(closedTabsOption)),
            ),
        ).check(matches(isChecked(true)))
        Log.i(TAG, "verifySelectedCloseTabsOption: Verified that the $closedTabsOption radio button is checked")
    }

    fun clickClosedTabsOption(closedTabsOption: String) {
        Log.i(TAG, "clickClosedTabsOption: Trying to click the $closedTabsOption option")
        when (closedTabsOption) {
            "Never" -> neverOption().click()
            "After one day" -> afterOneDayOption().click()
            "After one week" -> afterOneWeekOption().click()
            "After one month" -> afterOneMonthOption().click()
        }
        Log.i(TAG, "clickClosedTabsOption: Clicked the $closedTabsOption option")
    }

    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            Log.i(TAG, "goBack: Waiting for device to be idle")
            mDevice.waitForIdle()
            Log.i(TAG, "goBack: Device was idle")
            Log.i(TAG, "goBack: Trying to click the navigate up button")
            goBackButton().perform(ViewActions.click())
            Log.i(TAG, "goBack: Clicked the navigate up button")

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

private fun tabViewHeading() = onView(withText("Tab view"))

private fun listToggle() = onView(withText("List"))

private fun gridToggle() = onView(withText("Grid"))

private fun closeTabsHeading() = onView(withText("Close tabs"))

private fun manuallyToggle() = onView(withText("Manually"))

private fun neverOption() = onView(withText("Never"))

private fun afterOneDayOption() = onView(withText("After one day"))

private fun afterOneWeekOption() = onView(withText("After one week"))

private fun afterOneMonthOption() = onView(withText("After one month"))

private fun moveOldTabsToInactiveHeading() = onView(withText("Move old tabs to inactive"))

private fun moveOldTabsToInactiveToggle() =
    onView(withText(R.string.preferences_inactive_tabs_title))

private fun goBackButton() =
    onView(allOf(ViewMatchers.withContentDescription("Navigate up")))
