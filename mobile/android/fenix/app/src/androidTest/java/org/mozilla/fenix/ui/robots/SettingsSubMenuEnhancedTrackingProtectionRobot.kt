/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

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
import androidx.test.uiautomator.By
import androidx.test.uiautomator.Until
import org.hamcrest.CoreMatchers.allOf
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.appName
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.scrollToElementByText
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.isChecked
import org.mozilla.fenix.helpers.isEnabled

/**
 * Implementation of Robot Pattern for the settings Enhanced Tracking Protection sub menu.
 */
class SettingsSubMenuEnhancedTrackingProtectionRobot {

    fun verifyNavigationToolBarHeader() = assertNavigationToolBarHeader()

    fun verifyEnhancedTrackingProtectionSummary() = assertEnhancedTrackingProtectionSummary()

    fun verifyLearnMoreText() = assertLearnMoreText()

    fun verifyEnhancedTrackingProtectionTextWithSwitchWidget() = assertEnhancedTrackingProtectionTextWithSwitchWidget()

    fun verifyEnhancedTrackingProtectionOptionsEnabled(enabled: Boolean = true) {
        onView(withText("Standard (default)"))
            .check(matches(isEnabled(enabled)))

        onView(withText("Strict"))
            .check(matches(isEnabled(enabled)))

        onView(withText("Custom"))
            .check(matches(isEnabled(enabled)))
    }

    fun verifyTrackingProtectionSwitchEnabled() = assertTrackingProtectionSwitchEnabled()

    fun switchEnhancedTrackingProtectionToggle() = onView(
        allOf(
            withText("Enhanced Tracking Protection"),
            hasSibling(withResourceName("checkbox")),
        ),
    ).click()

    fun verifyStandardOptionDescription() {
        onView(withText(R.string.preference_enhanced_tracking_protection_standard_description_5))
            .check(matches(isDisplayed()))
        onView(withContentDescription(R.string.preference_enhanced_tracking_protection_standard_info_button))
            .check(matches(isDisplayed()))
    }

    fun verifyStrictOptionDescription() {
        onView(withText(R.string.preference_enhanced_tracking_protection_strict_description_4))
            .check(matches(isDisplayed()))
        onView(withContentDescription(R.string.preference_enhanced_tracking_protection_strict_info_button))
            .check(matches(isDisplayed()))
    }

    fun verifyCustomTrackingProtectionSettings() {
        scrollToElementByText("Redirect Trackers")
        onView(withText(R.string.preference_enhanced_tracking_protection_custom_description_2))
            .check(matches(isDisplayed()))
        onView(withContentDescription(R.string.preference_enhanced_tracking_protection_custom_info_button))
            .check(matches(isDisplayed()))
        cookiesCheckbox().check(matches(isDisplayed()))
        cookiesDropDownMenuDefault().check(matches(isDisplayed()))
        trackingContentCheckbox().check(matches(isDisplayed()))
        trackingcontentDropDownDefault().check(matches(isDisplayed()))
        cryptominersCheckbox().check(matches(isDisplayed()))
        fingerprintersCheckbox().check(matches(isDisplayed()))
        redirectTrackersCheckbox().check(matches(isDisplayed()))
    }

    fun verifyWhatsBlockedByStandardETPInfo() {
        onView(withContentDescription(R.string.preference_enhanced_tracking_protection_standard_info_button)).click()
        blockedByStandardETPInfo()
    }

    fun verifyWhatsBlockedByStrictETPInfo() {
        onView(withContentDescription(R.string.preference_enhanced_tracking_protection_strict_info_button)).click()
        // Repeating the info as in the standard option, with one extra point.
        blockedByStandardETPInfo()
        onView(withText("Tracking Content")).check(matches(isDisplayed()))
        onView(withText("Stops outside ads, videos, and other content from loading that contains tracking code. May affect some website functionality.")).check(matches(isDisplayed()))
    }

    fun verifyWhatsBlockedByCustomETPInfo() {
        onView(withContentDescription(R.string.preference_enhanced_tracking_protection_custom_info_button)).click()
        // Repeating the info as in the standard option, with one extra point.
        blockedByStandardETPInfo()
        onView(withText("Tracking Content")).check(matches(isDisplayed()))
        onView(withText("Stops outside ads, videos, and other content from loading that contains tracking code. May affect some website functionality.")).check(matches(isDisplayed()))
    }

    fun selectTrackingProtectionOption(option: String) = onView(withText(option)).click()

    fun verifyEnhancedTrackingProtectionLevelSelected(option: String, checked: Boolean) {
        mDevice.wait(
            Until.findObject(By.text("Enhanced Tracking Protection")),
            waitingTime,
        )
        onView(withText(option))
            .check(
                matches(
                    hasSibling(
                        allOf(
                            withId(R.id.radio_button),
                            isChecked(checked),
                        ),
                    ),
                ),
            )
    }

    class Transition {
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
            interact: SettingsSubMenuEnhancedTrackingProtectionExceptionsRobot.() -> Unit,
        ): SettingsSubMenuEnhancedTrackingProtectionExceptionsRobot.Transition {
            onView(withId(R.id.recycler_view)).perform(
                RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                    hasDescendant(withText("Exceptions")),
                ),
            )

            openExceptions().click()

            SettingsSubMenuEnhancedTrackingProtectionExceptionsRobot().interact()
            return SettingsSubMenuEnhancedTrackingProtectionExceptionsRobot.Transition()
        }
    }
}

private fun assertNavigationToolBarHeader() {
    onView(
        allOf(
            withParent(withId(R.id.navigationToolbar)),
            withText("Enhanced Tracking Protection"),
        ),
    )
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertEnhancedTrackingProtectionSummary() {
    onView(withText("$appName protects you from many of the most common trackers that follow what you do online."))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertLearnMoreText() {
    onView(withText("Learn more"))
        .check(matches(isDisplayed()))
}

private fun assertEnhancedTrackingProtectionTextWithSwitchWidget() {
    onView(
        allOf(
            withParentIndex(1),
            withChild(withText("Enhanced Tracking Protection")),
        ),
    )
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertTrackingProtectionSwitchEnabled() {
    onView(withResourceName("checkbox")).check(
        matches(
            isChecked(
                true,
            ),
        ),
    )
}

fun settingsSubMenuEnhancedTrackingProtection(interact: SettingsSubMenuEnhancedTrackingProtectionRobot.() -> Unit): SettingsSubMenuEnhancedTrackingProtectionRobot.Transition {
    SettingsSubMenuEnhancedTrackingProtectionRobot().interact()
    return SettingsSubMenuEnhancedTrackingProtectionRobot.Transition()
}

private fun goBackButton() =
    onView(allOf(withContentDescription("Navigate up")))

private fun openExceptions() =
    onView(allOf(withText("Exceptions")))

private fun cookiesCheckbox() = onView(withText("Cookies"))

private fun cookiesDropDownMenuDefault() = onView(withText("Isolate cross-site cookies"))

private fun trackingContentCheckbox() = onView(withText("Tracking content"))

private fun trackingcontentDropDownDefault() = onView(withText("In all tabs"))

private fun cryptominersCheckbox() = onView(withText("Cryptominers"))

private fun fingerprintersCheckbox() = onView(withText("Fingerprinters"))

private fun redirectTrackersCheckbox() = onView(withText("Redirect Trackers"))

private fun blockedByStandardETPInfo() {
    onView(withText("Social Media Trackers")).check(matches(isDisplayed()))
    onView(withText("Limits the ability of social networks to track your browsing activity around the web.")).check(matches(isDisplayed()))
    onView(withText("Cross-Site Cookies")).check(matches(isDisplayed()))
    onView(withText("Total Cookie Protection isolates cookies to the site you’re on so trackers like ad networks can’t use them to follow you across sites.")).check(matches(isDisplayed()))
    onView(withText("Cryptominers")).check(matches(isDisplayed()))
    onView(withText("Prevents malicious scripts gaining access to your device to mine digital currency.")).check(matches(isDisplayed()))
    onView(withText("Fingerprinters")).check(matches(isDisplayed()))
    onView(withText("Stops uniquely identifiable data from being collected about your device that can be used for tracking purposes.")).check(matches(isDisplayed()))
    onView(withText("Redirect Trackers")).check(matches(isDisplayed()))
    onView(withText("Clears cookies set by redirects to known tracking websites.")).check(matches(isDisplayed()))
}
