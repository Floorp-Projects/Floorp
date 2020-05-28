/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.preference.R
import androidx.recyclerview.widget.RecyclerView
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.contrib.RecyclerViewActions
import androidx.test.espresso.matcher.ViewMatchers.hasDescendant
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.withChild
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withParent
import androidx.test.espresso.matcher.ViewMatchers.withParentIndex
import androidx.test.espresso.matcher.ViewMatchers.withResourceName
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import org.hamcrest.CoreMatchers.allOf
import org.mozilla.fenix.helpers.assertIsChecked
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.isChecked

/**
 * Implementation of Robot Pattern for the settings Enhanced Tracking Protection sub menu.
 */
class SettingsSubMenuEnhancedTrackingProtectionRobot {

    fun verifyNavigationToolBarHeader() = assertNavigationToolBarHeader()

    fun verifyEnhancedTrackingProtectionHeader() = assertEnhancedTrackingProtectionHeader()

    fun verifyEnhancedTrackingProtectionHeaderDescription() = assertEnhancedTrackingProtectionHeaderDescription()

    fun verifyLearnMoreText() = assertLearnMoreText()

    fun verifyEnhancedTrackingProtectionTextWithSwitchWidget() = assertEnhancedTrackingProtectionTextWithSwitchWidget()

    fun verifyEnhancedTrackingProtectionOptions() = assertEnhancedTrackingProtectionOptions()

    fun verifyEnhancedTrackingProtectionDefaults() = assertEnhancedTrackingProtectionDefaults()

    fun verifyRadioButtonDefaults() = assertRadioButtonDefaults()

    fun verifyEnhancedTrackingProtectionProtectionSubMenuItems() {
        verifyEnhancedTrackingProtectionHeader()
        verifyEnhancedTrackingProtectionHeaderDescription()
        verifyLearnMoreText()
        verifyEnhancedTrackingProtectionTextWithSwitchWidget()
        verifyEnhancedTrackingProtectionDefaults()
        verifyRadioButtonDefaults()
        verifyEnhancedTrackingProtectionOptions()
    }

    class Transition {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())!!

        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            goBackButton().click()

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }

        fun openExceptions(
            interact: SettingsSubMenuEnhancedTrackingProtectionExceptionsRobot.() -> Unit
        ): SettingsSubMenuEnhancedTrackingProtectionExceptionsRobot.Transition {

            onView(withId(R.id.recycler_view)).perform(
                RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                    hasDescendant(withText("Exceptions"))
                )
            )

            openExceptions().click()

            SettingsSubMenuEnhancedTrackingProtectionExceptionsRobot().interact()
            return SettingsSubMenuEnhancedTrackingProtectionExceptionsRobot.Transition()
        }
    }
}

private fun assertNavigationToolBarHeader() {
    onView(allOf(withParent(withId(org.mozilla.fenix.R.id.navigationToolbar)),
        withText("Enhanced Tracking Protection")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertEnhancedTrackingProtectionHeader() {
    onView(withText("Browse without being followed"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertEnhancedTrackingProtectionHeaderDescription() {
    onView(allOf(withParent(withParentIndex(0)),
        withText("Keep your data to yourself. Firefox Preview protects you from many of the most common trackers that follow what you do online.")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertLearnMoreText() {
    onView(allOf(withParent(withParentIndex(0)),
        withText("Learn more")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertEnhancedTrackingProtectionTextWithSwitchWidget() {
    onView(allOf(
            withParentIndex(1),
            withChild(withText("Enhanced Tracking Protection"))))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertEnhancedTrackingProtectionOptions() {
    onView(withText("Standard"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    val stdText = "Pages will load normally, but block fewer trackers."
    onView(withText(stdText))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    onView(withText("Strict"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    val strictText =
        "Stronger tracking protection and faster performance, but some sites may not work properly."
    onView(withText(strictText))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    onView(withText("Custom"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    val customText =
        "Choose which trackers and scripts to block"
    onView(withText(customText))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertEnhancedTrackingProtectionDefaults() {
    onView(withResourceName("switch_widget")).check(
        matches(
            isChecked(
                true
            )
        )
    )
}

private fun assertRadioButtonDefaults() {
    onView(withText("Strict")
    ).assertIsChecked(false)

    onView(
        allOf(
            withId(org.mozilla.fenix.R.id.radio_button),
            hasSibling(withText("Standard"))
        )
    ).assertIsChecked(true)

    onView(withText("Custom")
    ).assertIsChecked(false)
}

fun settingsSubMenuEnhancedTrackingProtection(interact: SettingsSubMenuEnhancedTrackingProtectionRobot.() -> Unit): SettingsSubMenuEnhancedTrackingProtectionRobot.Transition {
    SettingsSubMenuEnhancedTrackingProtectionRobot().interact()
    return SettingsSubMenuEnhancedTrackingProtectionRobot.Transition()
}

private fun goBackButton() =
    onView(allOf(withContentDescription("Navigate up")))

private fun openExceptions() =
    onView(allOf(withText("Exceptions")))
