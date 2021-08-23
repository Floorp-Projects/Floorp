/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
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

class StrictEnhancedTrackingProtectionTest {
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val activityTestRule = HomeActivityTestRule()

    @Before
    fun setUp() {
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }

        activityTestRule.activity.settings().setStrictETP()
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
            verifyTrackingProtectionSwitchEnabled()
        }.openExceptions {
            verifyDefault()
        }
    }

    @Test
    fun testStrictVisitProtectionSheet() {
        val genericPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val trackingProtectionTest =
            TestAssetHelper.getEnhancedTrackingProtectionAsset(mockWebServer)

        // browsing a generic page to allow GV to load on a fresh run
        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericPage.url) {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(trackingProtectionTest.url) {}

        enhancedTrackingProtection {
        }.openEnhancedTrackingProtectionSheet {
            verifyEnhancedTrackingProtectionSheetStatus("ON", true)
        }
    }

    @Test
    fun testStrictVisitDisableExceptionToggle() {
        val genericPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val trackingProtectionTest =
            TestAssetHelper.getEnhancedTrackingProtectionAsset(mockWebServer)

        // browsing a generic page to allow GV to load on a fresh run
        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericPage.url) {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(trackingProtectionTest.url) {}

        enhancedTrackingProtection {
        }.openEnhancedTrackingProtectionSheet {
            verifyEnhancedTrackingProtectionSheetStatus("ON", true)
        }.disableEnhancedTrackingProtectionFromSheet {
            verifyEnhancedTrackingProtectionSheetStatus("OFF", false)
        }.openProtectionSettings {
            verifyEnhancedTrackingProtectionHeader()
            verifyEnhancedTrackingProtectionOptions()
            verifyTrackingProtectionSwitchEnabled()
        }

        settingsSubMenuEnhancedTrackingProtection {
        }.openExceptions {
            verifyListedURL(trackingProtectionTest.url.toString())
        }.disableExceptions {
            verifyDefault()
        }
    }

    @Test
    fun testStrictVisitSheetDetails() {
        val genericPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val trackingProtectionTest =
            TestAssetHelper.getEnhancedTrackingProtectionAsset(mockWebServer)

        // browsing a generic page to allow GV to load on a fresh run
        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericPage.url) {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(trackingProtectionTest.url) {}

        enhancedTrackingProtection {
        }.openEnhancedTrackingProtectionSheet {
            verifyEnhancedTrackingProtectionSheetStatus("ON", true)
        }.openDetails {
            verifyEnhancedTrackingProtectionDetailsStatus("Blocked")
            verifyTrackingCookiesBlocked()
            verifyCryptominersBlocked()
            verifyFingerprintersBlocked()
            verifyTrackingContentBlocked()
            viewTrackingContentBlockList()
        }
    }
}
