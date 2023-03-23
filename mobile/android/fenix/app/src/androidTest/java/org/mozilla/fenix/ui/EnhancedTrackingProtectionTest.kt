/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.core.net.toUri
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.appContext
import org.mozilla.fenix.helpers.TestHelper.exitMenu
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
    val activityTestRule = HomeActivityTestRule(
        isJumpBackInCFREnabled = false,
        isTCPCFREnabled = false,
        isWallpaperOnboardingEnabled = false,
    )

    @Before
    fun setUp() {
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }
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
            verifySettingsOptionSummary("Enhanced Tracking Protection", "Standard")
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
            exitMenu()
        }

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
        appContext.settings().setStrictETP()
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

    @Test
    fun testStrictVisitDisableExceptionToggle() {
        appContext.settings().setStrictETP()
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
        appContext.settings().setStrictETP()
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
        }.enterURLAndEnterToBrowser(trackingProtectionTest.url) {
            verifyTrackingProtectionWebContent("social blocked")
            verifyTrackingProtectionWebContent("ads blocked")
            verifyTrackingProtectionWebContent("analytics blocked")
            verifyTrackingProtectionWebContent("Fingerprinting blocked")
            verifyTrackingProtectionWebContent("Cryptomining blocked")
        }
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

    @SmokeTest
    @Test
    fun customTrackingProtectionSettingsTest() {
        val genericWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val trackingPage = TestAssetHelper.getEnhancedTrackingProtectionAsset(mockWebServer)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openEnhancedTrackingProtectionSubMenu {
            verifyEnhancedTrackingProtectionOptionsEnabled()
            selectTrackingProtectionOption("Custom")
            verifyCustomTrackingProtectionSettings()
        }.goBackToHomeScreen {}

        navigationToolbar {
            // browsing a basic page to allow GV to load on a fresh run
        }.enterURLAndEnterToBrowser(genericWebPage.url) {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(trackingPage.url) {
            verifyTrackingProtectionWebContent("social blocked")
            verifyTrackingProtectionWebContent("ads blocked")
            verifyTrackingProtectionWebContent("analytics blocked")
            verifyTrackingProtectionWebContent("Fingerprinting blocked")
            verifyTrackingProtectionWebContent("Cryptomining blocked")
        }

        enhancedTrackingProtection {
        }.openEnhancedTrackingProtectionSheet {
        }.openDetails {
            verifyTrackingCookiesBlocked()
            verifyCryptominersBlocked()
            verifyFingerprintersBlocked()
            verifyTrackingContentBlocked()
            viewTrackingContentBlockList()
        }
    }

    @SmokeTest
    @Test
    fun blockCookiesStorageAccessTest() {
        // With Standard TrackingProtection settings
        val page = mockWebServer.url("pages/cross-site-cookies.html").toString().toUri()
        val originSite = "https://mozilla-mobile.github.io"
        val currentSite = "http://localhost:${mockWebServer.port}"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(page) {
        }.clickRequestStorageAccessButton {
            verifyCrossOriginCookiesPermissionPrompt(originSite, currentSite)
        }.clickPagePermissionButton(allow = false) {
            verifyPageContent("access denied")
        }
    }

    @SmokeTest
    @Test
    fun allowCookiesStorageAccessTest() {
        // With Standard TrackingProtection settings
        val page = mockWebServer.url("pages/cross-site-cookies.html").toString().toUri()
        val originSite = "https://mozilla-mobile.github.io"
        val currentSite = "http://localhost:${mockWebServer.port}"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(page) {
        }.clickRequestStorageAccessButton {
            verifyCrossOriginCookiesPermissionPrompt(originSite, currentSite)
        }.clickPagePermissionButton(allow = true) {
            verifyPageContent("access granted")
        }
    }
}
