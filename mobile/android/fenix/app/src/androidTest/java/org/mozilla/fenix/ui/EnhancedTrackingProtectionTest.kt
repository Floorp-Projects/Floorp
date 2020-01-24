/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.test.platform.app.InstrumentationRegistry
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.ui.robots.enhancedTrackingProtection
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.robots.settingsSubMenuEnhancedTrackingProtection

/**
 *  Tests for verifying basic UI functionality of Enhanced Tracking Protection
 *
 *  Including
 *  - Verifying default states
 *  - Verifying Enhanced Tracking Protection notification bubble
 *  - Verifying Enhanced Tracking Protection notification shield
 *  - Verifying Enhanced Tracking Protection content sheet
 *  - Verifying Enhanced Tracking Protection content sheet details
 *  - Verifying Enhanced Tracking Protection toggle
 *  - Verifying Enhanced Tracking Protection site exceptions
 */

class EnhancedTrackingProtectionTest {
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val activityTestRule = HomeActivityTestRule()

    @Before
    fun setUp() {
        mockWebServer = MockWebServer().apply {
            setDispatcher(AndroidAssetDispatcher())
            start()
        }

        // Reset on-boarding notification for each test
        TestHelper.setPreference(
            InstrumentationRegistry.getInstrumentation().context,
            "pref_key_tracking_protection_onboarding", 0
        )
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    @Test
    fun testSettingsDefaults() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
            verifyEnhancedTrackingProtectionButton()
            verifyEnhancedTrackingProtectionValue("On")
        }.openEnhancedTrackingProtectionSubMenu {
            verifyEnhancedTrackingProtectionHeader()
            verifyEnhancedTrackingProtectionOptions()
            verifyEnhancedTrackingProtectionDefaults()
        }.openExceptions {
            verifyDefault()
        }
    }

    @Ignore("Disabled for investigation into failure - https://github.com/mozilla-mobile/fenix/issues/7907")
    @Test
    fun testStrictVisitContentNotification() {
        val trackingProtectionTest =
            TestAssetHelper.getEnhancedTrackingProtectionAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(trackingProtectionTest.url) {}

        enhancedTrackingProtection {
            verifyEnhancedTrackingProtectionNotice()
        }.closeNotificationPopup {}
    }

    @Ignore("Disabled for investigation into failure - https://github.com/mozilla-mobile/fenix/issues/7907")
    @Test
    fun testStrictVisitContentShield() {
        val trackingProtectionTest =
            TestAssetHelper.getEnhancedTrackingProtectionAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(trackingProtectionTest.url) {}

        enhancedTrackingProtection {
            verifyEnhancedTrackingProtectionNotice()
        }.closeNotificationPopup {}

        enhancedTrackingProtection {
            verifyEnhancedTrackingProtectionShield()
        }
    }

    @Ignore("Disabled for investigation into failure - https://github.com/mozilla-mobile/fenix/issues/7907")
    @Test
    fun testStrictVisitProtectionSheet() {
        val trackingProtectionTest =
            TestAssetHelper.getEnhancedTrackingProtectionAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(trackingProtectionTest.url) {}

        enhancedTrackingProtection {
            verifyEnhancedTrackingProtectionNotice()
        }.closeNotificationPopup {}

        enhancedTrackingProtection {
            verifyEnhancedTrackingProtectionShield()
        }.openEnhancedTrackingProtectionSheet {
            verifyEnhancedTrackingProtectionSheetStatus("ON", true)
        }
    }

    @Ignore("Disabled for investigation into failure - https://github.com/mozilla-mobile/fenix/issues/7907")
    @Test
    fun testStrictVisitDisable() {
        val trackingProtectionTest =
            TestAssetHelper.getEnhancedTrackingProtectionAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(trackingProtectionTest.url) {}

        enhancedTrackingProtection {
            verifyEnhancedTrackingProtectionNotice()
        }.closeNotificationPopup {}

        enhancedTrackingProtection {
            verifyEnhancedTrackingProtectionShield()
        }.openEnhancedTrackingProtectionSheet {
            verifyEnhancedTrackingProtectionSheetStatus("ON", true)
        }.disableEnhancedTrackingProtectionFromSheet {
            verifyEnhancedTrackingProtectionSheetStatus("OFF", false)
        }.closeEnhancedTrackingProtectionSheet {}

        // Verify that Enhanced Tracking Protection remains globally enabled
        navigationToolbar {
        }.openThreeDotMenu {
        }.openSettings {
            verifyEnhancedTrackingProtectionValue("On")
        }
    }

    @Ignore("Disabled for investigation into failure - https://github.com/mozilla-mobile/fenix/issues/7907")
    @Test
    fun testStrictVisitDisableExceptionToggle() {
        val trackingProtectionTest =
            TestAssetHelper.getEnhancedTrackingProtectionAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(trackingProtectionTest.url) {}

        enhancedTrackingProtection {
            verifyEnhancedTrackingProtectionNotice()
        }.closeNotificationPopup {}

        enhancedTrackingProtection {
            verifyEnhancedTrackingProtectionShield()
        }.openEnhancedTrackingProtectionSheet {
            verifyEnhancedTrackingProtectionSheetStatus("ON", true)
        }.disableEnhancedTrackingProtectionFromSheet {
            verifyEnhancedTrackingProtectionSheetStatus("OFF", false)
        }.openProtectionSettings {
            verifyEnhancedTrackingProtectionHeader()
            verifyEnhancedTrackingProtectionOptions()
            verifyEnhancedTrackingProtectionDefaults()
        }

        settingsSubMenuEnhancedTrackingProtection {
        }.openExceptions {
            verifyListedURL(trackingProtectionTest.url.toString())
        }.disableExceptions {
            verifyDefault()
        }
    }

    @Ignore("Disabled for investigation into failure - https://github.com/mozilla-mobile/fenix/issues/7907")
    @Test
    fun testStrictVisitSheetDetails() {
        val trackingProtectionTest =
            TestAssetHelper.getEnhancedTrackingProtectionAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(trackingProtectionTest.url) {}

        enhancedTrackingProtection {
            verifyEnhancedTrackingProtectionNotice()
        }.closeNotificationPopup {}

        enhancedTrackingProtection {
            verifyEnhancedTrackingProtectionShield()
        }.openEnhancedTrackingProtectionSheet {
            verifyEnhancedTrackingProtectionSheetStatus("ON", true)
        }.openDetails {
            verifyEnhancedTrackingProtectionDetailsStatus("Blocked")
        }
    }
}
