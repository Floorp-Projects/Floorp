/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity.robots

import androidx.test.uiautomator.UiObject
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import org.mozilla.focus.helpers.TestHelper.appName
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.waitingTime

class SearchSettingsRobot {

    fun openSearchEngineSubMenu() {
        searchEngineSubMenu.waitForExists(waitingTime)
        searchEngineSubMenu.click()
    }

    fun selectSearchEngine(engineName: String) {
        searchEngineList.waitForExists(waitingTime)
        searchEngineList
            .getChild(UiSelector().text(engineName))
            .click()
    }

    fun clickSearchSuggestionsSwitch() {
        searchSuggestionsSwitch.waitForExists(waitingTime)
        searchSuggestionsSwitch.click()
    }

    class Transition
}

private val searchEngineSubMenu =
    UiScrollable(UiSelector().resourceId("$appName:id/recycler_view"))
        .getChild(UiSelector().text("Search engine"))

private val searchEngineList = UiScrollable(
    UiSelector()
        .resourceId("$appName:id/search_engine_group").enabled(true)
)

private val searchSuggestionsSwitch: UiObject =
    mDevice.findObject(
        UiSelector()
            .resourceId("$appName:id/switchWidget")
    )
