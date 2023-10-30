/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.RootMatchers
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import junit.framework.TestCase.assertTrue
import org.hamcrest.Matchers.allOf
import org.hamcrest.Matchers.containsString
import org.hamcrest.Matchers.not
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.MatcherHelper.assertItemWithResIdExists
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull
import org.mozilla.fenix.helpers.isChecked

/**
 * Implementation of Robot Pattern for Enhanced Tracking Protection UI.
 */
class EnhancedTrackingProtectionRobot {
    fun verifyEnhancedTrackingProtectionSheetStatus(status: String, state: Boolean) =
        assertEnhancedTrackingProtectionSheetStatus(status, state)

    fun verifyETPSwitchVisibility(visible: Boolean) = assertETPSwitchVisibility(visible)

    fun verifyCrossSiteCookiesBlocked(isBlocked: Boolean) {
        assertTrue(
            mDevice.findObject(UiSelector().resourceId("$packageName:id/cross_site_tracking"))
                .waitForExists(waitingTime),
        )
        crossSiteCookiesBlockListButton.click()
        // Verifies the trackers block/allow list
        onView(withId(R.id.details_blocking_header))
            .check(
                matches(
                    withText(
                        if (isBlocked) {
                            ("Blocked")
                        } else {
                            ("Allowed")
                        },
                    ),
                ),
            )
    }

    fun verifySocialMediaTrackersBlocked(isBlocked: Boolean) {
        assertTrue(
            mDevice.findObject(UiSelector().resourceId("$packageName:id/social_media_trackers"))
                .waitForExists(waitingTime),
        )
        socialTrackersBlockListButton.click()
        // Verifies the trackers block/allow list
        onView(withId(R.id.details_blocking_header))
            .check(
                matches(
                    withText(
                        if (isBlocked) {
                            ("Blocked")
                        } else {
                            ("Allowed")
                        },
                    ),
                ),
            )
        onView(withId(R.id.blocking_text_list)).check(matches(isDisplayed()))
    }

    fun verifyFingerprintersBlocked(isBlocked: Boolean) {
        assertTrue(
            mDevice.findObject(UiSelector().resourceId("$packageName:id/fingerprinters"))
                .waitForExists(waitingTime),
        )
        fingerprintersBlockListButton.click()
        // Verifies the trackers block/allow list
        onView(withId(R.id.details_blocking_header))
            .check(
                matches(
                    withText(
                        if (isBlocked) {
                            ("Blocked")
                        } else {
                            ("Allowed")
                        },
                    ),
                ),
            )
        onView(withId(R.id.blocking_text_list)).check(matches(isDisplayed()))
    }

    fun verifyCryptominersBlocked(isBlocked: Boolean) {
        assertTrue(
            mDevice.findObject(UiSelector().resourceId("$packageName:id/cryptominers"))
                .waitForExists(waitingTime),
        )
        cryptominersBlockListButton.click()
        // Verifies the trackers block/allow list
        onView(withId(R.id.details_blocking_header))
            .check(
                matches(
                    withText(
                        if (isBlocked) {
                            ("Blocked")
                        } else {
                            ("Allowed")
                        },
                    ),
                ),
            )
        onView(withId(R.id.blocking_text_list)).check(matches(isDisplayed()))
    }

    fun verifyTrackingContentBlocked(isBlocked: Boolean) {
        assertTrue(
            mDevice.findObject(UiSelector().text("Tracking Content"))
                .waitForExists(waitingTime),
        )
        trackingContentBlockListButton.click()
        // Verifies the trackers block/allow list
        onView(withId(R.id.details_blocking_header))
            .check(
                matches(
                    withText(
                        if (isBlocked) {
                            ("Blocked")
                        } else {
                            ("Allowed")
                        },
                    ),
                ),
            )
        onView(withId(R.id.blocking_text_list)).check(matches(isDisplayed()))
    }

    fun viewTrackingContentBlockList() {
        onView(withId(R.id.blocking_text_list))
            .check(
                matches(
                    withText(
                        containsString(
                            "social-track-digest256.dummytracker.org\n" +
                                "ads-track-digest256.dummytracker.org\n" +
                                "analytics-track-digest256.dummytracker.org",
                        ),
                    ),
                ),
            )
    }

    fun verifyETPSectionIsDisplayedInQuickSettingsSheet(isDisplayed: Boolean) =
        assertItemWithResIdExists(
            itemWithResId("$packageName:id/trackingProtectionLayout"),
            exists = isDisplayed,
        )

    fun navigateBackToDetails() {
        onView(withId(R.id.details_back)).click()
    }

    class Transition {
        fun openEnhancedTrackingProtectionSheet(interact: EnhancedTrackingProtectionRobot.() -> Unit): Transition {
            pageSecurityIndicator().waitForExists(waitingTime)
            pageSecurityIndicator().click()
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

        fun toggleEnhancedTrackingProtectionFromSheet(interact: EnhancedTrackingProtectionRobot.() -> Unit): Transition {
            enhancedTrackingProtectionSwitch().click()

            EnhancedTrackingProtectionRobot().interact()
            return Transition()
        }

        fun openProtectionSettings(interact: SettingsSubMenuEnhancedTrackingProtectionRobot.() -> Unit): SettingsSubMenuEnhancedTrackingProtectionRobot.Transition {
            openEnhancedTrackingProtectionDetails().waitForExists(waitingTime)
            openEnhancedTrackingProtectionDetails().click()
            trackingProtectionSettingsButton().click()

            SettingsSubMenuEnhancedTrackingProtectionRobot().interact()
            return SettingsSubMenuEnhancedTrackingProtectionRobot.Transition()
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
    mDevice.waitNotNull(Until.findObjects(By.text("Protections are $status for this site")))
    onView(ViewMatchers.withResourceName("switch_widget")).check(
        matches(
            isChecked(
                state,
            ),
        ),
    )
}

private fun pageSecurityIndicator() =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/mozac_browser_toolbar_security_indicator"))

private fun enhancedTrackingProtectionSwitch() =
    onView(ViewMatchers.withResourceName("switch_widget"))

private fun trackingProtectionSettingsButton() =
    onView(withId(R.id.protection_settings)).inRoot(RootMatchers.isDialog()).check(
        matches(
            isDisplayed(),
        ),
    )

private fun openEnhancedTrackingProtectionDetails() =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/trackingProtectionDetails"))

private val trackingContentBlockListButton =
    onView(
        allOf(
            withText("Tracking Content"),
            withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE),
        ),
    )

private val socialTrackersBlockListButton =
    onView(
        allOf(
            withId(R.id.social_media_trackers),
            withText("Social Media Trackers"),
        ),
    )

private val crossSiteCookiesBlockListButton =
    onView(
        allOf(
            withId(R.id.cross_site_tracking),
            withText("Cross-Site Cookies"),
        ),
    )

private val cryptominersBlockListButton =
    onView(
        allOf(
            withId(R.id.cryptominers),
            withText("Cryptominers"),
        ),
    )

private val fingerprintersBlockListButton =
    onView(
        allOf(
            withId(R.id.fingerprinters),
            withText("Fingerprinters"),
        ),
    )

private fun assertSecuritySheetIsCompletelyDisplayed() {
    mDevice.findObject(UiSelector().description(getStringResource(R.string.quick_settings_sheet)))
        .waitForExists(waitingTime)
    assertTrue(
        mDevice.findObject(
            UiSelector().resourceId("$packageName:id/quick_action_sheet"),
        ).waitForExists(waitingTime),
    )
}
