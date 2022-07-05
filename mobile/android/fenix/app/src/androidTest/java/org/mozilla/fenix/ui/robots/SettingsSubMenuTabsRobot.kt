/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withText
import org.hamcrest.CoreMatchers.allOf
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.TestHelper.mDevice

/**
 * Implementation of Robot Pattern for the settings Tabs sub menu.
 */
class SettingsSubMenuTabsRobot {

    fun verifyTabViewOptions() = assertTabViewOptions()

    fun verifyCloseTabsOptions() = assertCloseTabsOptions()

    fun verifyMoveOldTabsToInactiveOptions() = assertMoveOldTabsToInactiveOptions()

    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            mDevice.waitForIdle()
            goBackButton().perform(ViewActions.click())

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

private fun assertTabViewOptions() {
    tabViewHeading()
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    listToggle()
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    gridToggle()
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    searchTermTabGroupsToggle()
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    searchGroupsDescription()
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
}

private fun assertCloseTabsOptions() {
    closeTabsHeading()
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    neverToggle()
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    afterOneDayToggle()
        .check(ViewAssertions.matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    afterOneWeekToggle()
        .check(ViewAssertions.matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    afterOneMonthToggle()
        .check(ViewAssertions.matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
}

private fun assertMoveOldTabsToInactiveOptions() {
    moveOldTabsToInactiveHeading()
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    moveOldTabsToInactiveToggle()
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
}

private fun tabViewHeading() = onView(withText("Tab view"))

private fun listToggle() = onView(withText("List"))

private fun gridToggle() = onView(withText("Grid"))

private fun searchTermTabGroupsToggle() = onView(withText("Search groups"))

private fun searchGroupsDescription() = onView(withText("Group related sites together"))

private fun closeTabsHeading() = onView(withText("Close tabs"))

private fun manuallyToggle() = onView(withText("Manually"))

private fun neverToggle() = onView(withText("Never"))

private fun afterOneDayToggle() = onView(withText("After one day"))

private fun afterOneWeekToggle() = onView(withText("After one week"))

private fun afterOneMonthToggle() = onView(withText("After one month"))

private fun moveOldTabsToInactiveHeading() = onView(withText("Move old tabs to inactive"))

private fun moveOldTabsToInactiveToggle() =
    onView(withText(R.string.preferences_inactive_tabs_title))

private fun goBackButton() =
    onView(allOf(ViewMatchers.withContentDescription("Navigate up")))
