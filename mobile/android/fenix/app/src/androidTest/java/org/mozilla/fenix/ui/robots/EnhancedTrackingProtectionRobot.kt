/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.hamcrest.Matchers.containsString
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

    fun verifyTrackingCookiesBlocked() = assertTrackingCookiesBlocked()

    fun verifyFingerprintersBlocked() = assertFingerprintersBlocked()

    fun verifyCryptominersBlocked() = assertCryptominersBlocked()

    fun verifyBasicLevelTrackingContentBlocked() = assertBasicLevelTrackingContentBlocked()

    class Transition {
        fun openEnhancedTrackingProtectionSheet(interact: EnhancedTrackingProtectionRobot.() -> Unit): Transition {
            openEnhancedTrackingProtectionSheet().click()
            EnhancedTrackingProtectionRobot().interact()
            return Transition()
        }

        fun closeEnhancedTrackingProtectionSheet(interact: BrowserRobot.() -> Unit): Transition {
            // Back out of the Enhanced Tracking Protection sheet
            mDevice.pressBack()

            BrowserRobot().interact()
            return Transition()
        }

        fun disableEnhancedTrackingProtectionFromSheet(interact: EnhancedTrackingProtectionRobot.() -> Unit): Transition {
            disableEnhancedTrackingProtection().click()

            EnhancedTrackingProtectionRobot().interact()
            return Transition()
        }

        fun openProtectionSettings(interact: SettingsSubMenuEnhancedTrackingProtectionRobot.() -> Unit): Transition {
            onView(withId(R.id.trackingProtectionDetails)).click()
            openEnhancedTrackingProtectionSettings().click()

            SettingsSubMenuEnhancedTrackingProtectionRobot().interact()
            return Transition()
        }

        fun openDetails(interact: EnhancedTrackingProtectionRobot.() -> Unit): Transition {
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

private fun assertEnhancedTrackingProtectionShield() {
    mDevice.waitNotNull(
        Until.findObjects(By.descContains("Tracking Protection has blocked trackers"))
    )
}

private fun assertEnhancedTrackingProtectionSheetStatus(status: String, state: Boolean) {
    mDevice.waitNotNull(Until.findObjects(By.textContains(status)))
    onView(ViewMatchers.withResourceName("switch_widget")).check(
        ViewAssertions.matches(
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
    onView(withId(R.id.mozac_browser_toolbar_security_indicator))

private fun disableEnhancedTrackingProtection() =
    onView(ViewMatchers.withResourceName("switch_widget"))

private fun openEnhancedTrackingProtectionSettings() =
    onView(ViewMatchers.withId(R.id.protection_settings))

private fun openEnhancedTrackingProtectionDetails() =
    onView(ViewMatchers.withId(R.id.trackingProtectionDetails))

private fun assertTrackingCookiesBlocked() {
    mDevice.findObject(UiSelector().resourceId("$packageName:id/cross_site_tracking"))
        .waitForExists(waitingTime)
    onView(withId(R.id.blocking_header)).check(matches(isDisplayed()))
    onView(withId(R.id.cross_site_tracking)).check(matches(isDisplayed()))
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

private fun assertBasicLevelTrackingContentBlocked() {
    mDevice.findObject(UiSelector().resourceId("$packageName:id/tracking_content"))
        .waitForExists(waitingTime)

    onView(withId(R.id.tracking_content))
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
