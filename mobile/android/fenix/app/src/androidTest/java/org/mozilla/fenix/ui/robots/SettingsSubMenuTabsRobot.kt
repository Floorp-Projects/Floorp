/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

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
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.isChecked

/**
 * Implementation of Robot Pattern for the settings Tabs sub menu.
 */
class SettingsSubMenuTabsRobot {

    fun verifyTabViewOptions() {
        tabViewHeading()
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        listToggle()
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        gridToggle()
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    }

    fun verifyCloseTabsOptions() {
        closeTabsHeading()
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        neverOption()
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        afterOneDayOption()
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        afterOneWeekOption()
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        afterOneMonthOption()
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    }

    fun verifyMoveOldTabsToInactiveOptions() {
        moveOldTabsToInactiveHeading()
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        moveOldTabsToInactiveToggle()
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    }

    fun verifySelectedCloseTabsOption(closedTabsOption: String) =
        onView(
            allOf(
                withId(R.id.radio_button),
                hasSibling(withText(closedTabsOption)),
            ),
        ).check(matches(isChecked(true)))

    fun clickClosedTabsOption(closedTabsOption: String) {
        when (closedTabsOption) {
            "Never" -> neverOption().click()
            "After one day" -> afterOneDayOption().click()
            "After one week" -> afterOneWeekOption().click()
            "After one month" -> afterOneMonthOption().click()
        }
    }

    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            mDevice.waitForIdle()
            goBackButton().perform(ViewActions.click())

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
