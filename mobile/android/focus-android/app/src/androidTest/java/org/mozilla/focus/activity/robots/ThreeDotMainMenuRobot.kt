/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.uiautomator.UiSelector
import org.mozilla.focus.R
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.waitingTime

class ThreeDotMainMenuRobot {

    class Transition {
        fun openSettings(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            mDevice.findObject(UiSelector().text("Settings")).waitForExists(waitingTime)
            settingsMenuButton
                .check(matches(isDisplayed()))
                .perform(click())

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

private val settingsMenuButton = onView(ViewMatchers.withId(R.id.settings))
