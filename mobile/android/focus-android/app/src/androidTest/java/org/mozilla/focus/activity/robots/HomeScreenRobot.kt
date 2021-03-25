/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.uiautomator.UiSelector
import org.mozilla.focus.R
import org.mozilla.focus.helpers.TestHelper.appName
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.waitingTime

class HomeScreenRobot {

    class Transition {
        fun openMainMenu(interact: ThreeDotMainMenuRobot.() -> Unit): ThreeDotMainMenuRobot.Transition {
            searchBar.waitForExists(waitingTime)
            mainMenu
                .check(matches(isDisplayed()))
                .perform(click())

            ThreeDotMainMenuRobot().interact()
            return ThreeDotMainMenuRobot.Transition()
        }
    }
}

fun homeScreen(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
    HomeScreenRobot().interact()
    return HomeScreenRobot.Transition()
}

private val searchBar = mDevice.findObject(UiSelector().resourceId("$appName:id/urlView"))

private val mainMenu = onView(withId(R.id.menuView))
