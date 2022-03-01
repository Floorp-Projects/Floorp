/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import org.junit.Assert.assertTrue
import org.mozilla.focus.R
import org.mozilla.focus.helpers.TestHelper.getStringResource
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.packageName
import org.mozilla.focus.helpers.TestHelper.waitingTime

class SettingsRobot {

    fun verifySettingsMenuItems() {
        settingsMenuList.waitForExists(waitingTime)
        whatsNewButton.check(matches(isDisplayed()))
        assertTrue(generalSettingsMenu().exists())
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
            localizedText: String = getStringResource(R.string.preference_category_general),
            interact: SettingsGeneralMenuRobot.() -> Unit
        ): SettingsGeneralMenuRobot.Transition {
            generalSettingsMenu(localizedText).waitForExists(waitingTime)
            generalSettingsMenu(localizedText).click()

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

        fun clickWhatsNewLink(
            interact: BrowserRobot.() -> Unit
        ): BrowserRobot.Transition {
            whatsNewButton.perform(click())

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun goBackToHomeScreen(
            interact: SearchRobot.() -> Unit
        ): SearchRobot.Transition {
            mDevice.pressBack()

            SearchRobot().interact()
            return SearchRobot.Transition()
        }
    }
}

private val settingsMenuList =
    UiScrollable(UiSelector().resourceId("$packageName:id/recycler_view"))

private fun generalSettingsMenu(localizedText: String = getStringResource(R.string.preference_category_general)) =
    settingsMenuList.getChild(
        UiSelector()
            .text(localizedText)
    )

private val searchSettingsMenu = settingsMenuList.getChild(
    UiSelector()
        .text(getStringResource(R.string.preference_category_search))
)

private val privacySettingsMenu = settingsMenuList.getChild(
    UiSelector()
        .text(getStringResource(R.string.preference_privacy_and_security_header))
)

private val advancedSettingsMenu = settingsMenuList.getChild(
    UiSelector()
        .text(getStringResource(R.string.preference_category_advanced))
)

private val mozillaSettingsMenu = settingsMenuList.getChild(
    UiSelector()
        .text(getStringResource(R.string.preference_mozilla_summary))
)

private val whatsNewButton = onView(withId(R.id.menu_whats_new))
