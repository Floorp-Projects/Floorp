/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.RootMatchers
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import junit.framework.TestCase.assertTrue
import org.hamcrest.Matchers.containsString
import org.hamcrest.Matchers.not
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull
import org.mozilla.fenix.helpers.isChecked

/**
 * Implementation of Robot Pattern for Enhanced Tracking Protection UI.
 */
class EnhancedTrackingProtectionRobot {

    val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())!!

    fun verifyEnhancedTrackingProtectionSheetStatus(status: String, state: Boolean) =
        assertEnhancedTrackingProtectionSheetStatus(status, state)

    fun verifyEnhancedTrackingProtectionDetailsStatus(status: String) =
        assertEnhancedTrackingProtectionDetailsStatus(status)

    fun verifyETPSwitchVisibility(visible: Boolean) = assertETPSwitchVisibility(visible)

    fun verifyTrackingCookiesBlocked() = assertTrackingCookiesBlocked()

    fun verifyFingerprintersBlocked() = assertFingerprintersBlocked()

    fun verifyCryptominersBlocked() = assertCryptominersBlocked()

    fun verifyTrackingContentBlocked() = assertTrackingContentBlocked()

    fun viewTrackingContentBlockList() {
        trackingContentBlockListButton()
            .check(matches(isDisplayed()))
            .click()
        onView(withId(R.id.blocking_text_list))
            .check(
                matches(
                    withText(
                        containsString(
                            "social-track-digest256.dummytracker.org\n" +
                                "ads-track-digest256.dummytracker.org\n" +
                                "analytics-track-digest256.dummytracker.org"
                        )
                    )
                )
            )
    }

    class Transition {
        fun openEnhancedTrackingProtectionSheet(interact: EnhancedTrackingProtectionRobot.() -> Unit): Transition {
            openEnhancedTrackingProtectionSheet().waitForExists(waitingTime)
            openEnhancedTrackingProtectionSheet().click()
            assertSecuritySheetIsCompletelyDisplayed()

            EnhancedTrackingProtectionRobot().interact()
            return Transition()
        }

        fun closeEnhancedTrackingProtectionSheet(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            // Back out of the Enhanced Tracking Protection sheet
            mDevice.pressBack()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun disableEnhancedTrackingProtectionFromSheet(interact: EnhancedTrackingProtectionRobot.() -> Unit): Transition {
            enhancedTrackingProtectionSwitch().click()

            EnhancedTrackingProtectionRobot().interact()
            return Transition()
        }

        fun openProtectionSettings(interact: SettingsSubMenuEnhancedTrackingProtectionRobot.() -> Unit): Transition {
            openEnhancedTrackingProtectionDetails().waitForExists(waitingTime)
            openEnhancedTrackingProtectionDetails().click()
            trackingProtectionSettingsButton().click()

            SettingsSubMenuEnhancedTrackingProtectionRobot().interact()
            return Transition()
        }

        fun openDetails(interact: EnhancedTrackingProtectionRobot.() -> Unit): Transition {
            openEnhancedTrackingProtectionDetails().waitForExists(waitingTime)
            openEnhancedTrackingProtectionDetails().click()

            EnhancedTrackingProtectionRobot().interact()
            return Transition()
        }
    }
}

fun enhancedTrackingProtection(interact: EnhancedTrackingProtectionRobot.() -> Unit): EnhancedTrackingProtectionRobot.Transition {
    EnhancedTrackingProtectionRobot().interact()
    return EnhancedTrackingProtectionRobot.Transition()
}

private fun assertETPSwitchVisibility(visible: Boolean) {
    if (visible) {
        enhancedTrackingProtectionSwitch()
            .check(matches(isDisplayed()))
    } else {
        enhancedTrackingProtectionSwitch()
            .check(matches(not(isDisplayed())))
    }
}

private fun assertEnhancedTrackingProtectionSheetStatus(status: String, state: Boolean) {
    mDevice.waitNotNull(Until.findObjects(By.textContains(status)))
    onView(ViewMatchers.withResourceName("switch_widget")).check(
        matches(
            isChecked(
                state
            )
        )
    )
}

private fun assertEnhancedTrackingProtectionDetailsStatus(status: String) {
    mDevice.waitNotNull(Until.findObjects(By.textContains(status)))
}

private fun openEnhancedTrackingProtectionSheet() =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/mozac_browser_toolbar_security_indicator"))

private fun enhancedTrackingProtectionSwitch() =
    onView(ViewMatchers.withResourceName("switch_widget"))

private fun trackingProtectionSettingsButton() =
    onView(withId(R.id.protection_settings)).inRoot(RootMatchers.isDialog()).check(
        matches(
            isDisplayed()
        )
    )

private fun openEnhancedTrackingProtectionDetails() =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/trackingProtectionDetails"))

private fun assertTrackingCookiesBlocked() {
    mDevice.findObject(UiSelector().resourceId("$packageName:id/cross_site_tracking"))
        .waitForExists(waitingTime)
    onView(withId(R.id.blocking_header)).check(matches(isDisplayed()))
    onView(withId(R.id.tracking_content)).check(matches(isDisplayed()))
}

private fun assertFingerprintersBlocked() {
    mDevice.findObject(UiSelector().resourceId("$packageName:id/fingerprinters"))
        .waitForExists(waitingTime)
    onView(withId(R.id.blocking_header)).check(matches(isDisplayed()))
    onView(withId(R.id.fingerprinters)).check(matches(isDisplayed()))
}

private fun assertCryptominersBlocked() {
    mDevice.findObject(UiSelector().resourceId("$packageName:id/cryptominers"))
        .waitForExists(waitingTime)
    onView(withId(R.id.blocking_header)).check(matches(isDisplayed()))
    onView(withId(R.id.cryptominers)).check(matches(isDisplayed()))
}

private fun assertTrackingContentBlocked() {
    assertTrue(
        mDevice.findObject(UiSelector().resourceId("$packageName:id/tracking_content"))
            .waitForExists(waitingTime)
    )
}

private fun trackingContentBlockListButton() = onView(withId(R.id.tracking_content))

private fun assertSecuritySheetIsCompletelyDisplayed() {
    mDevice.findObject(UiSelector().description("Quick settings sheet"))
        .waitForExists(waitingTime)
    onView(withContentDescription("Quick settings sheet"))
        .check(matches(isCompletelyDisplayed()))
}
