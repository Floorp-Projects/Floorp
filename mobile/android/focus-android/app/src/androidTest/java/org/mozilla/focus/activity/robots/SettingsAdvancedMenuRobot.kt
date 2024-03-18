/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.isChecked
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.isNotChecked
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import org.hamcrest.CoreMatchers.allOf
import org.mozilla.focus.R
import org.mozilla.focus.helpers.EspressoHelper.hasCousin
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.packageName
import org.mozilla.focus.helpers.TestHelper.waitingTime

class SettingsAdvancedMenuRobot {
    fun verifyAdvancedSettingsItems() {
        advancedSettingsList.waitForExists(waitingTime)
        developerToolsHeading().check(matches(isDisplayed()))
        remoteDebuggingSwitch().check(matches(isDisplayed()))
        assertRemoteDebuggingSwitchState()
        openLinksInAppsButton().check(matches(isDisplayed()))
        assertOpenLinksInAppsSwitchState()
    }

    fun verifyOpenLinksInAppsSwitchState(enabled: Boolean) = assertOpenLinksInAppsSwitchState(enabled)
    fun clickOpenLinksInAppsSwitch() = openLinksInAppsButton().perform(click())

    class Transition {
        fun goBackToSettings(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            mDevice.pressBack()
            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

private fun openLinksInAppsButton() = onView(withText(R.string.preferences_open_links_in_apps))

private fun assertOpenLinksInAppsSwitchState(enabled: Boolean = false) {
    if (enabled) {
        openLinksInAppsButton()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switchWidget),
                            isChecked(),
                        ),
                    ),
                ),
            )
    } else {
        openLinksInAppsButton()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switchWidget),
                            isNotChecked(),
                        ),
                    ),
                ),
            )
    }
}

private val advancedSettingsList =
    UiScrollable(UiSelector().resourceId("$packageName:id/recycler_view"))

private fun developerToolsHeading() = onView(withText(R.string.preference_advanced_summary))

private fun remoteDebuggingSwitch() = onView(withText("Remote debugging via USB/Wi-Fi"))

private fun assertRemoteDebuggingSwitchState(enabled: Boolean = false) {
    if (enabled) {
        remoteDebuggingSwitch()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switchWidget),
                            isChecked(),
                        ),
                    ),
                ),
            )
    } else {
        remoteDebuggingSwitch()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switchWidget),
                            isNotChecked(),
                        ),
                    ),
                ),
            )
    }
}
