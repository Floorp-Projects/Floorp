/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.FeatureSettingsHelper
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
    private val featureSettingsHelper = FeatureSettingsHelper()

    @get:Rule
    val activityTestRule = HomeActivityTestRule()

    @Before
    fun setUp() {
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }
        featureSettingsHelper.setStrictETPEnabled()
        featureSettingsHelper.setJumpBackCFREnabled(false)
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
        featureSettingsHelper.resetAllFeatureFlags()
    }

    @Test
    fun testSettingsDefaults() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
            verifyEnhancedTrackingProtectionButton()
            verifyEnhancedTrackingProtectionState("On")
        }.openEnhancedTrackingProtectionSubMenu {
            verifyEnhancedTrackingProtectionHeader()
            verifyEnhancedTrackingProtectionOptionsEnabled()
            verifyTrackingProtectionSwitchEnabled()
        }.openExceptions {
            verifyDefault()
        }
    }

    @SmokeTest
    @Test
    fun testETPOffGlobally() {
        val genericPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openEnhancedTrackingProtectionSubMenu {
            switchEnhancedTrackingProtectionToggle()
            verifyEnhancedTrackingProtectionOptionsEnabled(false)
        }.goBack {
        }.goBack { }

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericPage.url) { }
        enhancedTrackingProtection {
        }.openEnhancedTrackingProtectionSheet {
            verifyETPSwitchVisibility(false)
        }.closeEnhancedTrackingProtectionSheet {
        }.openThreeDotMenu {
        }.openSettings {
        }.openEnhancedTrackingProtectionSubMenu {
            switchEnhancedTrackingProtectionToggle()
            verifyEnhancedTrackingProtectionOptionsEnabled(true)
        }.goBack {
        }.goBackToBrowser { }
        enhancedTrackingProtection {
        }.openEnhancedTrackingProtectionSheet {
            verifyETPSwitchVisibility(true)
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
        }.openTabDrawer {
            closeTab()
        }

        navigationToolbar {
        }.enterURLAndEnterToBrowser(trackingProtectionTest.url) {}
        enhancedTrackingProtection {
        }.openEnhancedTrackingProtectionSheet {
            verifyEnhancedTrackingProtectionSheetStatus("ON", true)
        }
    }

    @Ignore("Failing with frequent ANR: https://bugzilla.mozilla.org/show_bug.cgi?id=1764605")
    @Test
    fun testStrictVisitDisableExceptionToggle() {
        val genericPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val trackingProtectionTest =
            TestAssetHelper.getEnhancedTrackingProtectionAsset(mockWebServer)

        // browsing a generic page to allow GV to load on a fresh run
        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericPage.url) {
        }.openTabDrawer {
            closeTab()
        }

        navigationToolbar {
        }.enterURLAndEnterToBrowser(trackingProtectionTest.url) {}
        enhancedTrackingProtection {
        }.openEnhancedTrackingProtectionSheet {
            verifyEnhancedTrackingProtectionSheetStatus("ON", true)
        }.disableEnhancedTrackingProtectionFromSheet {
            verifyEnhancedTrackingProtectionSheetStatus("OFF", false)
        }.openProtectionSettings {
            verifyEnhancedTrackingProtectionHeader()
            verifyEnhancedTrackingProtectionOptionsEnabled()
            verifyTrackingProtectionSwitchEnabled()
        }

        settingsSubMenuEnhancedTrackingProtection {
        }.openExceptions {
            verifyListedURL(trackingProtectionTest.url.host.toString())
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
        }.openTabDrawer {
            closeTab()
        }

        navigationToolbar {
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
