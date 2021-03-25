/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity.robots

import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import org.mozilla.focus.helpers.TestHelper.appName
import org.mozilla.focus.helpers.TestHelper.waitingTime

class SettingsRobot {

    class Transition {
        fun openSearchSettingsMenu(interact: SearchSettingsRobot.() -> Unit): SearchSettingsRobot.Transition {
            searchSettingsMenu.waitForExists(waitingTime)
            searchSettingsMenu.click()

            SearchSettingsRobot().interact()
            return SearchSettingsRobot.Transition()
        }
    }
}

private val settingsMenuList = UiScrollable(UiSelector().resourceId("$appName:id/recycler_view"))

private val searchSettingsMenu = settingsMenuList.getChild(
    UiSelector()
        .resourceId("android:id/title")
        .text("Search")
        .enabled(true)
)
