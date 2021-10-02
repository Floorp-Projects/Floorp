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
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import org.hamcrest.CoreMatchers.allOf
import org.mozilla.fenix.helpers.TestHelper.scrollToElementByText
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the settings Tabs sub menu.
 */
class SettingsSubMenuTabsRobot {

    fun verifyTabViewOptions() = assertTabViewOptions()

    fun verifyCloseTabsOptions() = assertCloseTabsOptions()

    fun verifyStartOnHomeOptions() = assertStartOnHomeOptions()

    fun clickAlwaysStartOnHomeToggle() {
        scrollToElementByText("Always")
        alwaysStartOnHomeToggle().click()
    }

    class Transition {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

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
}

private fun assertCloseTabsOptions() {
    closeTabsHeading()
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    afterOneDayToggle()
        .check(ViewAssertions.matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    afterOneWeekToggle()
        .check(ViewAssertions.matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    afterOneMonthToggle()
        .check(ViewAssertions.matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
}

private fun assertStartOnHomeOptions() {
    // Scroll to ensure all the items are visible.
    scrollToElementByText("Never")
    startOnHomeHeading()
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    afterFourHoursToggle()
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    alwaysStartOnHomeToggle()
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
}

private fun tabViewHeading() = onView(withText("Tab view"))

private fun listToggle() = onView(withText("List"))

private fun gridToggle() = onView(withText("Grid"))

private fun searchTermTabGroupsToggle() = onView(withText("Search groups"))

private fun closeTabsHeading() = onView(withText("Close tabs"))

private fun manuallyToggle() = onView(withText("Manually"))

private fun afterOneDayToggle() = onView(withText("After one day"))

private fun afterOneWeekToggle() = onView(withText("After one week"))

private fun afterOneMonthToggle() = onView(withText("After one month"))

private fun startOnHomeHeading() = onView(withText("Start on home"))

private fun afterFourHoursToggle() = onView(withText("After four hours"))

private fun alwaysStartOnHomeToggle() = onView(withText("Always"))

private fun neverStartOnHomeToggle() = onView(withText("Never"))

private fun goBackButton() =
    onView(allOf(ViewMatchers.withContentDescription("Navigate up")))
