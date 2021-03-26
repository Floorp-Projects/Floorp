/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import org.mozilla.focus.helpers.TestHelper.appName
import org.mozilla.focus.helpers.TestHelper.waitingTime

class SettingsAdvancedMenuRobot {
    fun verifyAdvancedSettingsItems() {
        advancedSettingsList.waitForExists(waitingTime)
        remoteDebuggingSwitch.check(matches(isDisplayed()))
    }

    class Transition
}

private val advancedSettingsList =
    UiScrollable(UiSelector().resourceId("$appName:id/recycler_view"))

private val remoteDebuggingSwitch = onView(ViewMatchers.withText("Remote debugging via USB/Wi-Fi"))
