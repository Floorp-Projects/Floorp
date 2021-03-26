/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity.robots

import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import org.junit.Assert.assertTrue
import org.mozilla.focus.helpers.TestHelper.appName
import org.mozilla.focus.helpers.TestHelper.waitingTime

class SettingsRobot {

    fun verifySettingsMenuItems() {
        settingsMenuList.waitForExists(waitingTime)
        assertTrue(generalSettingsMenu.exists())
        assertTrue(searchSettingsMenu.exists())
        assertTrue(privacySettingsMenu.exists())
        assertTrue(advancedSettingsMenu.exists())
        assertTrue(mozillaSettingsMenu.exists())
    }

    class Transition {
        fun openSearchSettingsMenu(interact: SearchSettingsRobot.() -> Unit): SearchSettingsRobot.Transition {
            searchSettingsMenu.waitForExists(waitingTime)
            searchSettingsMenu.click()

            SearchSettingsRobot().interact()
            return SearchSettingsRobot.Transition()
        }

        fun openGeneralSettingsMenu(
            interact: SettingsGeneralMenuRobot.() -> Unit
        ): SettingsGeneralMenuRobot.Transition {
            generalSettingsMenu.waitForExists(waitingTime)
            generalSettingsMenu.click()

            SettingsGeneralMenuRobot().interact()
            return SettingsGeneralMenuRobot.Transition()
        }

        fun openPrivacySettingsMenu(
            interact: SettingsPrivacyMenuRobot.() -> Unit
        ): SettingsPrivacyMenuRobot.Transition {
            privacySettingsMenu.waitForExists(waitingTime)
            privacySettingsMenu.click()

            SettingsPrivacyMenuRobot().interact()
            return SettingsPrivacyMenuRobot.Transition()
        }

        fun openAdvancedSettingsMenu(
            interact: SettingsAdvancedMenuRobot.() -> Unit
        ): SettingsAdvancedMenuRobot.Transition {
            advancedSettingsMenu.waitForExists(waitingTime)
            advancedSettingsMenu.click()

            SettingsAdvancedMenuRobot().interact()
            return SettingsAdvancedMenuRobot.Transition()
        }

        fun openMozillaSettingsMenu(
            interact: SettingsMozillaMenuRobot.() -> Unit
        ): SettingsMozillaMenuRobot.Transition {
            mozillaSettingsMenu.waitForExists(waitingTime)
            mozillaSettingsMenu.click()

            SettingsMozillaMenuRobot().interact()
            return SettingsMozillaMenuRobot.Transition()
        }
    }
}

private val settingsMenuList = UiScrollable(UiSelector().resourceId("$appName:id/recycler_view"))

private val generalSettingsMenu = settingsMenuList.getChild(
    UiSelector()
        .resourceId("android:id/title")
        .text("General")
)

private val searchSettingsMenu = settingsMenuList.getChild(
    UiSelector()
        .resourceId("android:id/title")
        .text("Search")
        .enabled(true)
)

private val privacySettingsMenu = settingsMenuList.getChild(
    UiSelector()
        .resourceId("android:id/title")
        .text("Privacy & Security")
)

private val advancedSettingsMenu = settingsMenuList.getChild(
    UiSelector()
        .text("Advanced")
        .resourceId("android:id/title")
)

private val mozillaSettingsMenu = settingsMenuList.getChild(
    UiSelector()
        .text("Mozilla")
        .resourceId("android:id/title")
)
