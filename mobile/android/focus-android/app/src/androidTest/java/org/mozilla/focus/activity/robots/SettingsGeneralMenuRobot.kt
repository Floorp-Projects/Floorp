/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withText
import org.mozilla.focus.helpers.TestHelper.pressBackKey

class SettingsGeneralMenuRobot {

    fun verifyGeneralSettingsItems() {
        defaultBrowserSwitch.check(matches(isDisplayed()))
        switchToLinkToggleButton.check(matches(isDisplayed()))
    }

    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            pressBackKey()

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

private val defaultBrowserSwitch = onView(withText("Make Firefox Focus default browser"))

private val switchToLinkToggleButton = onView(withText("Switch to link in new tab immediately"))
