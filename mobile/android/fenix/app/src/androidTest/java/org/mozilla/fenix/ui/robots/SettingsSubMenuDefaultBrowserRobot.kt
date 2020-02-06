/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.intent.Intents.intended
import androidx.test.espresso.intent.matcher.IntentMatchers.hasAction
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.isNotChecked
import androidx.test.espresso.matcher.ViewMatchers.withParent
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.CoreMatchers.not
import org.mozilla.fenix.R
import org.mozilla.fenix.ui.robots.SettingsSubMenuDefaultBrowserRobot.Companion.DEFAULT_APPS_SETTINGS_ACTION

/**
 * Implementation of Robot Pattern for the settings DefaultBrowser sub menu.
 */
class SettingsSubMenuDefaultBrowserRobot {

    companion object {
        const val DEFAULT_APPS_SETTINGS_ACTION = "android.settings.MANAGE_DEFAULT_APPS_SETTINGS"
    }

    fun verifyOpenLinksInPrivateTab() = assertOpenLinksInPrivateTab()
    fun verifyDefaultBrowserIsDisabled() = assertDefaultBrowserIsDisabled()
    fun clickDefaultBrowserSwitch() = toggleDefaultBrowserSwitch()
    fun verifyAndroidDefaultAppsMenuAppears() = assertAndroidDefaultAppsMenuAppears()

    class Transition {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            mDevice.waitForIdle()
            goBackButton().perform(ViewActions.click())

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

fun assertDefaultBrowserIsDisabled() {
    onView(withId(R.id.switch_widget))
        .check(matches(isNotChecked()))
}

fun toggleDefaultBrowserSwitch() {
    onView(
        allOf(
            withParent(not(withId(R.id.navigationToolbar))),
            withText("Set as default browser")
        )
    )
        .perform(click())
}

private fun assertAndroidDefaultAppsMenuAppears() {
    intended(hasAction(DEFAULT_APPS_SETTINGS_ACTION))
}

private fun assertOpenLinksInPrivateTab() {
    onView(withText("Open links in private tab"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun goBackButton() =
    onView(allOf(withContentDescription("Navigate up")))
