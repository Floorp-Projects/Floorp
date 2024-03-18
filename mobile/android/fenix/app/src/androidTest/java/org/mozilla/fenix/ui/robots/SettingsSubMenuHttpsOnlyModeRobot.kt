/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.util.Log
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import org.hamcrest.CoreMatchers.allOf
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.assertIsChecked
import org.mozilla.fenix.helpers.assertIsEnabled
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.isChecked

class SettingsSubMenuHttpsOnlyModeRobot {

    fun verifyHttpsOnlyModeMenuHeader() {
        Log.i(TAG, "verifyHttpsOnlyModeMenuHeader: Trying to verify that the \"HTTPS-Only Mode\" toolbar items are visible")
        onView(
            allOf(
                withText(getStringResource(R.string.preferences_https_only_title)),
                hasSibling(withContentDescription("Navigate up")),
            ),
        ).check(matches(isDisplayed()))
        Log.i(TAG, "verifyHttpsOnlyModeMenuHeader: Verified that the \"HTTPS-Only Mode\" toolbar items are visible")
    }

    fun verifyHttpsOnlyModeSummary() {
        Log.i(TAG, "verifyHttpsOnlyModeSummary: Trying to verify that the \"HTTPS-Only Mode\" title is visible")
        onView(withId(R.id.https_only_title))
            .check(matches(withText("HTTPS-Only Mode")))
        Log.i(TAG, "verifyHttpsOnlyModeSummary: Verified that the \"HTTPS-Only Mode\" title is visible")
        Log.i(TAG, "verifyHttpsOnlyModeSummary: Trying to verify that the \"HTTPS-Only Mode\" summary is visible")
        onView(withId(R.id.https_only_summary))
            .check(matches(withText("Automatically attempts to connect to sites using HTTPS encryption protocol for increased security. Learn more")))
        Log.i(TAG, "verifyHttpsOnlyModeSummary: Verified that the \"HTTPS-Only Mode\" summary is visible")
    }

    fun verifyHttpsOnlyModeIsEnabled(shouldBeEnabled: Boolean) {
        Log.i(TAG, "verifyHttpsOnlyModeIsEnabled: Trying to verify that the \"HTTPS-Only Mode\" toggle is checked: $shouldBeEnabled")
        httpsModeOnlySwitch().check(
            matches(
                if (shouldBeEnabled) {
                    isChecked(true)
                } else {
                    isChecked(false)
                },
            ),
        )
        Log.i(TAG, "verifyHttpsOnlyModeIsEnabled: Verified that the \"HTTPS-Only Mode\" toggle is checked: $shouldBeEnabled")
    }

    fun clickHttpsOnlyModeSwitch() {
        Log.i(TAG, "clickHttpsOnlyModeSwitch: Trying to click the \"HTTPS-Only Mode\" toggle")
        httpsModeOnlySwitch().click()
        Log.i(TAG, "clickHttpsOnlyModeSwitch: Clicked the \"HTTPS-Only Mode\" toggle")
    }

    fun verifyHttpsOnlyModeOptionsEnabled(shouldBeEnabled: Boolean) {
        Log.i(TAG, "verifyHttpsOnlyModeOptionsEnabled: Trying to verify that the \"Enable in all tabs\" option is enabled: $shouldBeEnabled")
        allTabsOption().assertIsEnabled(shouldBeEnabled)
        Log.i(TAG, "verifyHttpsOnlyModeOptionsEnabled: Verified that the \"Enable in all tabs\" option is enabled: $shouldBeEnabled")
        Log.i(TAG, "verifyHttpsOnlyModeOptionsEnabled: Trying to verify that the \"Enable only in private tabs\" option is enabled: $shouldBeEnabled")
        onlyPrivateTabsOption().assertIsEnabled(shouldBeEnabled)
        Log.i(TAG, "verifyHttpsOnlyModeOptionsEnabled: Verified that the \"Enable only in private tabs\" option is enabled: $shouldBeEnabled")
    }

    fun verifyHttpsOnlyOptionSelected(allTabsOptionSelected: Boolean, privateTabsOptionSelected: Boolean) {
        if (allTabsOptionSelected) {
            Log.i(TAG, "verifyHttpsOnlyOptionSelected: Trying to verify that the \"Enable in all tabs\" option is checked: $allTabsOptionSelected")
            allTabsOption().assertIsChecked(true)
            Log.i(TAG, "verifyHttpsOnlyOptionSelected: Verified that the \"Enable in all tabs\" option is checked: $allTabsOptionSelected")
            Log.i(TAG, "verifyHttpsOnlyOptionSelected: Trying to verify that the \"Enable only in private tabs\" option is checked: $privateTabsOptionSelected")
            onlyPrivateTabsOption().assertIsChecked(false)
            Log.i(TAG, "verifyHttpsOnlyOptionSelected: Verified that the \"Enable only in private tabs\" option is checked: $privateTabsOptionSelected")
        } else if (privateTabsOptionSelected) {
            Log.i(TAG, "verifyHttpsOnlyOptionSelected: Trying to verify that the \"Enable in all tabs\" option is checked: $allTabsOptionSelected")
            allTabsOption().assertIsChecked(false)
            Log.i(TAG, "verifyHttpsOnlyOptionSelected: Verified that the \"Enable in all tabs\" option is checked: $allTabsOptionSelected")
            Log.i(TAG, "verifyHttpsOnlyOptionSelected: Trying to verify that the \"Enable only in private tabs\" option is checked: $privateTabsOptionSelected")
            onlyPrivateTabsOption().assertIsChecked(true)
            Log.i(TAG, "verifyHttpsOnlyOptionSelected: Verified that the \"Enable only in private tabs\" option is checked: $privateTabsOptionSelected")
        }
    }

    fun selectHttpsOnlyModeOption(allTabsOptionSelected: Boolean, privateTabsOptionSelected: Boolean) {
        if (allTabsOptionSelected) {
            Log.i(TAG, "selectHttpsOnlyModeOption: Trying to click the \"Enable in all tabs\" option")
            allTabsOption().click()
            Log.i(TAG, "selectHttpsOnlyModeOption: Clicked the \"Enable in all tabs\" option")
            Log.i(TAG, "selectHttpsOnlyModeOption: Trying to verify that the \"Enable in all tabs\" option is checked: $allTabsOptionSelected")
            allTabsOption().assertIsChecked(true)
            Log.i(TAG, "selectHttpsOnlyModeOption: Verified that the \"Enable in all tabs\" option is checked: $allTabsOptionSelected")
        } else if (privateTabsOptionSelected) {
            Log.i(TAG, "selectHttpsOnlyModeOption: Trying to click the \"Enable only in private tabs\" option")
            onlyPrivateTabsOption().click()
            Log.i(TAG, "selectHttpsOnlyModeOption: Clicked the \"Enable only in private tabs\" option")
            Log.i(TAG, "selectHttpsOnlyModeOption: Trying to verify that the \"Enable only in private tabs\" option is checked: $privateTabsOptionSelected")
            onlyPrivateTabsOption().assertIsChecked(true)
            Log.i(TAG, "selectHttpsOnlyModeOption: Verified that the \"Enable only in private tabs\" option is checked: $privateTabsOptionSelected")
        }
    }

    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            Log.i(TAG, "goBack: Trying to click the navigate up toolbar button")
            goBackButton().perform(click())
            Log.i(TAG, "goBack: Clicked the navigate up toolbar button")

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

private fun httpsModeOnlySwitch() = onView(withId(R.id.https_only_switch))

private fun allTabsOption() =
    onView(
        allOf(
            withId(R.id.https_only_all_tabs),
            withText("Enable in all tabs"),
        ),
    )

private fun onlyPrivateTabsOption() =
    onView(
        allOf(
            withId(R.id.https_only_private_tabs),
            withText("Enable only in private tabs"),
        ),
    )

private fun goBackButton() = onView(withContentDescription("Navigate up"))
