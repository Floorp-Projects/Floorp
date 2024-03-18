/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.os.Build
import android.util.Log
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.ViewInteraction
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.isChecked
import androidx.test.espresso.matcher.ViewMatchers.isNotChecked
import androidx.test.espresso.matcher.ViewMatchers.withClassName
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.Matchers.endsWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.TestHelper.hasCousin
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the settings Theme sub menu.
 */
class SettingsSubMenuCustomizeRobot {

    fun verifyThemes() {
        Log.i(TAG, "verifyThemes: Trying to verify that the \"Light\" mode option is visible")
        lightModeToggle()
            .check(matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyThemes: Verified that the \"Light\" mode option is visible")
        Log.i(TAG, "verifyThemes: Trying to verify that the \"Dark\" mode option is visible")
        darkModeToggle()
            .check(matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyThemes: Verified that the \"Dark\" mode option is visible")
        Log.i(TAG, "verifyThemes: Trying to verify that the \"Follow device theme\" option is visible")
        deviceModeToggle()
            .check(matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyThemes: Verified that the \"Follow device theme\" option is visible")
    }

    fun selectDarkMode() {
        Log.i(TAG, "selectDarkMode: Trying to click the \"Dark\" mode option")
        darkModeToggle().click()
        Log.i(TAG, "selectDarkMode: Clicked the \"Dark\" mode option")
    }

    fun selectLightMode() {
        Log.i(TAG, "selectLightMode: Trying to click the \"Light\" mode option")
        lightModeToggle().click()
        Log.i(TAG, "selectLightMode: Clicked the \"Light\" mode option")
    }

    fun clickTopToolbarToggle() {
        Log.i(TAG, "clickTopToolbarToggle: Trying to click the \"Top\" toolbar option")
        topToolbarToggle().click()
        Log.i(TAG, "clickTopToolbarToggle: Clicked the \"Top\" toolbar option")
    }

    fun clickBottomToolbarToggle() {
        Log.i(TAG, "clickBottomToolbarToggle: Trying to click the \"Bottom\" toolbar option")
        bottomToolbarToggle().click()
        Log.i(TAG, "clickBottomToolbarToggle: Clicked the \"Bottom\" toolbar option")
    }

    fun verifyToolbarPositionPreference(selectedPosition: String) {
        Log.i(TAG, "verifyToolbarPositionPreference: Trying to verify that the $selectedPosition toolbar option is checked")
        onView(withText(selectedPosition))
            .check(matches(hasSibling(allOf(withId(R.id.radio_button), isChecked()))))
        Log.i(TAG, "verifyToolbarPositionPreference: Verified that the $selectedPosition toolbar option is checked")
    }

    fun clickSwipeToolbarToSwitchTabToggle() {
        Log.i(TAG, "clickSwipeToolbarToSwitchTabToggle: Trying to click the \"Swipe toolbar sideways to switch tabs\" toggle")
        swipeToolbarToggle().click()
        Log.i(TAG, "clickSwipeToolbarToSwitchTabToggle: Clicked the \"Swipe toolbar sideways to switch tabs\" toggle")
    }

    fun clickPullToRefreshToggle() {
        Log.i(TAG, "clickPullToRefreshToggle: Trying to click the \"Pull to refresh\" toggle")
        pullToRefreshToggle().click()
        Log.i(TAG, "clickPullToRefreshToggle: Clicked the \"Pull to refresh\" toggle")
    }

    fun verifySwipeToolbarGesturePrefState(isEnabled: Boolean) {
        Log.i(TAG, "verifySwipeToolbarGesturePrefState: Trying to verify that the \"Swipe toolbar sideways to switch tabs\" toggle is checked: $isEnabled")
        swipeToolbarToggle()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withClassName(endsWith("Switch")),
                            if (isEnabled) {
                                isChecked()
                            } else {
                                isNotChecked()
                            },
                        ),
                    ),
                ),
            )
        Log.i(TAG, "verifySwipeToolbarGesturePrefState: Verified that the \"Swipe toolbar sideways to switch tabs\" toggle is checked: $isEnabled")
    }

    fun verifyPullToRefreshGesturePrefState(isEnabled: Boolean) {
        Log.i(TAG, "verifyPullToRefreshGesturePrefState: Trying to verify that the \"Pull to refresh\" toggle is checked: $isEnabled")
        pullToRefreshToggle()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withClassName(endsWith("Switch")),
                            if (isEnabled) {
                                isChecked()
                            } else {
                                isNotChecked()
                            },
                        ),
                    ),
                ),
            )
        Log.i(TAG, "verifyPullToRefreshGesturePrefState: Verified that the \"Pull to refresh\" toggle is checked: $isEnabled")
    }

    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            Log.i(TAG, "goBack: Waiting for device to be idle")
            mDevice.waitForIdle()
            Log.i(TAG, "goBack: Waited for device to be idle")
            Log.i(TAG, "goBack: Trying to click the navigate up toolbar button")
            goBackButton().perform(click())
            Log.i(TAG, "goBack: Clicked the navigate up toolbar button")

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

private fun darkModeToggle() = onView(withText("Dark"))

private fun lightModeToggle() = onView(withText("Light"))

private fun topToolbarToggle() = onView(withText("Top"))

private fun bottomToolbarToggle() = onView(withText("Bottom"))

private fun deviceModeToggle(): ViewInteraction {
    val followDeviceThemeText =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) "Follow device theme" else "Set by Battery Saver"
    return onView(withText(followDeviceThemeText))
}

private fun swipeToolbarToggle() =
    onView(withText(getStringResource(R.string.preference_gestures_swipe_toolbar_switch_tabs)))

private fun pullToRefreshToggle() =
    onView(withText(getStringResource(R.string.preference_gestures_website_pull_to_refresh)))

private fun goBackButton() =
    onView(allOf(ViewMatchers.withContentDescription("Navigate up")))
