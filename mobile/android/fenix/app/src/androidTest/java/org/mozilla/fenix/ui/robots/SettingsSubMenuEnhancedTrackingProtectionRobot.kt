/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.preference.R
import androidx.recyclerview.widget.RecyclerView
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.Espresso.pressBack
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.contrib.RecyclerViewActions
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.hasDescendant
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
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
import org.hamcrest.CoreMatchers.not
import org.mozilla.fenix.helpers.TestHelper.appName
import org.mozilla.fenix.helpers.TestHelper.scrollToElementByText
import org.mozilla.fenix.helpers.assertIsChecked
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.isChecked
import org.mozilla.fenix.helpers.isEnabled

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

    fun verifyEnhancedTrackingProtectionOptionsGrayedOut() = assertEnhancedTrackingProtectionOptionsGrayedOut()

    fun verifyTrackingProtectionSwitchEnabled() = assertTrackingProtectionSwitchEnabled()

    fun switchEnhancedTrackingProtectionToggle() = onView(withResourceName("switch_widget")).click()

    fun verifyRadioButtonDefaults() = assertRadioButtonDefaults()

    fun verifyEnhancedTrackingProtectionProtectionSubMenuItems() {
        verifyEnhancedTrackingProtectionHeader()
        verifyEnhancedTrackingProtectionHeaderDescription()
        verifyLearnMoreText()
        verifyEnhancedTrackingProtectionTextWithSwitchWidget()
        verifyTrackingProtectionSwitchEnabled()
        verifyRadioButtonDefaults()
        verifyEnhancedTrackingProtectionOptions()
    }

    fun verifyCustomTrackingProtectionSettings() = assertCustomTrackingProtectionSettings()

    fun selectTrackingProtectionOption(option: String) = onView(withText(option)).click()

    class Transition {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())!!

        fun goBackToHomeScreen(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            // To settings
            goBackButton().click()
            // To HomeScreen
            pressBack()

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

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
        withText("Keep your data to yourself. $appName protects you from many of the most common trackers that follow what you do online.")))
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
    onView(withText("Standard (default)"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    onView(withText(org.mozilla.fenix.R.string.preference_enhanced_tracking_protection_standard_description_4))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    onView(withText("Strict"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    onView(withText(org.mozilla.fenix.R.string.preference_enhanced_tracking_protection_strict_description_3))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    onView(withText("Custom"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    val customText =
        "Choose which trackers and scripts to block."
    onView(withText(customText))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertEnhancedTrackingProtectionOptionsGrayedOut() {
    onView(withText("Standard (default)"))
        .check(matches(not(isEnabled(true))))

    onView(withText(org.mozilla.fenix.R.string.preference_enhanced_tracking_protection_standard_description_4))
        .check(matches(not(isEnabled(true))))

    onView(withText("Strict"))
        .check(matches(not(isEnabled(true))))

    onView(withText(org.mozilla.fenix.R.string.preference_enhanced_tracking_protection_strict_description_3))
        .check(matches(not(isEnabled(true))))

    onView(withText("Custom"))
        .check(matches(not(isEnabled(true))))

    val customText =
        "Choose which trackers and scripts to block."
    onView(withText(customText))
        .check(matches(not(isEnabled(true))))
}

private fun assertTrackingProtectionSwitchEnabled() {
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
            hasSibling(withText("Standard (default)"))
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

private fun assertCustomTrackingProtectionSettings() {
    scrollToElementByText("Redirect Trackers")
    cookiesCheckbox().check(matches(isDisplayed()))
    cookiesDropDownMenuDefault().check(matches(isDisplayed()))
    trackingContentCheckbox().check(matches(isDisplayed()))
    trackingcontentDropDownDefault().check(matches(isDisplayed()))
    cryptominersCheckbox().check(matches(isDisplayed()))
    fingerprintersCheckbox().check(matches(isDisplayed()))
    redirectTrackersCheckbox().check(matches(isDisplayed()))
}

private fun cookiesCheckbox() = onView(withText("Cookies"))

private fun cookiesDropDownMenuDefault() = onView(withText("All cookies (will cause websites to break)"))

private fun trackingContentCheckbox() = onView(withText("Tracking content"))

private fun trackingcontentDropDownDefault() = onView(withText("In all tabs"))

private fun cryptominersCheckbox() = onView(withText("Cryptominers"))

private fun fingerprintersCheckbox() = onView(withText("Fingerprinters"))

private fun redirectTrackersCheckbox() = onView(withText("Redirect Trackers"))
