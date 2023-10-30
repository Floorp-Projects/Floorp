/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.ViewInteraction
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import org.hamcrest.CoreMatchers.allOf
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.assertIsChecked
import org.mozilla.fenix.helpers.assertIsEnabled
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.isChecked

class SettingsSubMenuHttpsOnlyModeRobot {

    fun verifyHttpsOnlyModeMenuHeader(): ViewInteraction =
        onView(
            allOf(
                withText(getStringResource(R.string.preferences_https_only_title)),
                hasSibling(withContentDescription("Navigate up")),
            ),
        ).check(matches(isDisplayed()))

    fun verifyHttpsOnlyModeSummary() {
        onView(withId(R.id.https_only_title))
            .check(matches(withText("HTTPS-Only Mode")))
        onView(withId(R.id.https_only_summary))
            .check(matches(withText("Automatically attempts to connect to sites using HTTPS encryption protocol for increased security. Learn more")))
    }

    fun verifyHttpsOnlyModeIsEnabled(shouldBeEnabled: Boolean) {
        httpsModeOnlySwitch.check(
            matches(
                if (shouldBeEnabled) {
                    isChecked(true)
                } else {
                    isChecked(false)
                },
            ),
        )
    }

    fun clickHttpsOnlyModeSwitch() = httpsModeOnlySwitch.click()

    fun verifyHttpsOnlyModeOptionsEnabled(shouldBeEnabled: Boolean) {
        allTabsOption.assertIsEnabled(shouldBeEnabled)
        onlyPrivateTabsOption.assertIsEnabled(shouldBeEnabled)
    }

    fun verifyHttpsOnlyOptionSelected(allTabsOptionSelected: Boolean, privateTabsOptionSelected: Boolean) {
        if (allTabsOptionSelected) {
            allTabsOption.assertIsChecked(true)
            onlyPrivateTabsOption.assertIsChecked(false)
        } else if (privateTabsOptionSelected) {
            allTabsOption.assertIsChecked(false)
            onlyPrivateTabsOption.assertIsChecked(true)
        }
    }

    fun selectHttpsOnlyModeOption(allTabsOptionSelected: Boolean, privateTabsOptionSelected: Boolean) {
        if (allTabsOptionSelected) {
            allTabsOption.click()
            allTabsOption.assertIsChecked(true)
        } else if (privateTabsOptionSelected) {
            onlyPrivateTabsOption.click()
            onlyPrivateTabsOption.assertIsChecked(true)
        }
    }

    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            goBackButton.perform(click())

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

private val httpsModeOnlySwitch = onView(withId(R.id.https_only_switch))

private val allTabsOption =
    onView(
        allOf(
            withId(R.id.https_only_all_tabs),
            withText("Enable in all tabs"),
        ),
    )

private val onlyPrivateTabsOption =
    onView(
        allOf(
            withId(R.id.https_only_private_tabs),
            withText("Enable only in private tabs"),
        ),
    )

private val goBackButton = onView(withContentDescription("Navigate up"))
