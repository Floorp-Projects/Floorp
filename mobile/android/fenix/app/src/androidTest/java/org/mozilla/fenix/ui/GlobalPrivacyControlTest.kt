/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.TestAssetHelper.TestAsset
import org.mozilla.fenix.helpers.TestAssetHelper.getGPCTestAsset
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 * Tests for Global Privacy Control setting.
 */

class GlobalPrivacyControlTest {
    private lateinit var mockWebServer: MockWebServer
    private lateinit var gpcPage: TestAsset

    @get:Rule
    val activityTestRule = HomeActivityIntentTestRule(
        isJumpBackInCFREnabled = false,
        isTCPCFREnabled = false,
        isWallpaperOnboardingEnabled = false,
        skipOnboarding = true,
    )

    @Before
    fun setUp() {
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }

        gpcPage = getGPCTestAsset(mockWebServer)
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2429327
    @Test
    fun testGPCinNormalBrowsing() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(gpcPage.url) {
            verifyPageContent("GPC not enabled.")
        }.openThreeDotMenu {
        }.openSettings {
        }.openEnhancedTrackingProtectionSubMenu {
            scrollToGCPSettings()
            verifyGPCTextWithSwitchWidget()
            verifyGPCSwitchEnabled(false)
            switchGPCToggle()
        }.goBack {
        }.goBackToBrowser {
            verifyPageContent("GPC is enabled.")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2429364
    @Test
    fun testGPCinPrivateBrowsing() {
        homeScreen { }.togglePrivateBrowsingMode()
        navigationToolbar {
        }.enterURLAndEnterToBrowser(gpcPage.url) {
            verifyPageContent("GPC is enabled.")
        }.openThreeDotMenu {
        }.openSettings {
        }.openEnhancedTrackingProtectionSubMenu {
            scrollToGCPSettings()
            verifyGPCTextWithSwitchWidget()
            verifyGPCSwitchEnabled(false)
            switchGPCToggle()
        }.goBack {
        }.goBackToBrowser {
            verifyPageContent("GPC is enabled.")
        }
    }
}
