/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withParent
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import org.hamcrest.CoreMatchers.allOf
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.TestHelper.appName
import org.mozilla.fenix.helpers.assertIsEnabled
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the settings Data Collection sub menu.
 */
class SettingsSubMenuDataCollectionRobot {

    fun verifyNavigationToolBarHeader() = assertNavigationToolBarHeader()

    fun verifyDataCollectionOptions() = assertDataCollectionOptions()

    fun verifyUsageAndTechnicalDataSwitchDefault() = assertUsageAndTechnicalDataSwitchDefault()

    fun verifyMarketingDataSwitchDefault() = assertMarketingDataValueSwitchDefault()

    fun verifyExperimentsSwitchDefault() = assertExperimentsSwitchDefault()

    fun verifyDataCollectionSubMenuItems() {
        verifyDataCollectionOptions()
        verifyUsageAndTechnicalDataSwitchDefault()
        verifyMarketingDataSwitchDefault()
        // Temporarily disabled until https://github.com/mozilla-mobile/fenix/issues/17086 and
        // https://github.com/mozilla-mobile/fenix/issues/17143 are resolved:
        // verifyExperimentsSwitchDefault()
    }

    class Transition {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())!!
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            goBackButton().click()

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

private fun goBackButton() =
    onView(withContentDescription("Navigate up"))

private fun assertNavigationToolBarHeader() = onView(
    allOf(withParent(withId(R.id.navigationToolbar)),
        withText(R.string.preferences_data_collection)))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertDataCollectionOptions() {

    onView(withText(R.string.preference_usage_data))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    val usageAndTechnicalDataText =
        "Shares performance, usage, hardware and customization data about your browser with Mozilla to help us make $appName better"

    onView(withText(usageAndTechnicalDataText))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    onView(withText(R.string.preferences_marketing_data))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    val marketingDataText =
        "Shares basic usage data with Adjust, our mobile marketing vendor"

    onView(withText(marketingDataText))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    // Temporarily disabled until https://github.com/mozilla-mobile/fenix/issues/17086 and
    // https://github.com/mozilla-mobile/fenix/issues/17143 are resolved:
    // onView(withText(R.string.preference_experiments_2)).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    // onView(withText(R.string.preference_experiments_summary_2)).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun usageAndTechnicalDataButton() = onView(withText(R.string.preference_usage_data))

private fun assertUsageAndTechnicalDataSwitchDefault() = usageAndTechnicalDataButton()
    .assertIsEnabled(isEnabled = true)

private fun marketingDataButton() = onView(withText(R.string.preferences_marketing_data))

private fun assertMarketingDataValueSwitchDefault() = marketingDataButton()
    .assertIsEnabled(isEnabled = true)

private fun experimentsButton() = onView(withText(R.string.preference_experiments_2))

private fun assertExperimentsSwitchDefault() = experimentsButton()
    .assertIsEnabled(isEnabled = true)
