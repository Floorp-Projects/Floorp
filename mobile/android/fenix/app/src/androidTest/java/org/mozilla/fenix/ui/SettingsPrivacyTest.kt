/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.ui.robots.homeScreen

/**
 *  Tests for verifying the the privacy and security section of the Settings menu
 *
 */

class SettingsPrivacyTest {
    /* ktlint-disable no-blank-line-before-rbrace */ // This imposes unreadable grouping.

    private lateinit var mDevice: UiDevice
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val activityTestRule = HomeActivityTestRule.withDefaultSettingsOverrides(skipOnboarding = true)

    @Before
    fun setUp() {
        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
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
    fun settingsPrivacyItemsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
            verifySettingsToolbar()
            verifyPrivacyHeading()
            verifyPrivateBrowsingButton()
            verifyHTTPSOnlyModeButton()
            verifySettingsOptionSummary("HTTPS-Only Mode", "Off")
            verifyCookieBannerReductionButton()
            verifySettingsOptionSummary("Cookie banner reduction", "Off")
            verifyEnhancedTrackingProtectionButton()
            verifySettingsOptionSummary("Enhanced Tracking Protection", "Standard")
            verifySitePermissionsButton()
            verifyDeleteBrowsingDataButton()
            verifyDeleteBrowsingDataOnQuitButton()
            verifySettingsOptionSummary("Delete browsing data on quit", "Off")
            verifyNotificationsButton()
            verifySettingsOptionSummary("Notifications", "Allowed")
            verifyDataCollectionButton()
        }
    }

    @Test
    fun verifyDataCollectionTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuDataCollection {
            verifyDataCollectionView(
                true,
                true,
                "On",
            )
        }
    }

    @Test
    fun verifyUsageAndTechnicalDataToggleTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuDataCollection {
            verifyUsageAndTechnicalDataToggle(true)
            clickUsageAndTechnicalDataToggle()
            verifyUsageAndTechnicalDataToggle(false)
        }
    }

    @Test
    fun verifyMarketingDataToggleTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuDataCollection {
            verifyMarketingDataToggle(true)
            clickMarketingDataToggle()
            verifyMarketingDataToggle(false)
        }
    }

    @Test
    fun verifyStudiesToggleTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuDataCollection {
            verifyDataCollectionView(
                true,
                true,
                "On",
            )
            clickStudiesOption()
            verifyStudiesToggle(true)
            clickStudiesToggle()
            verifyStudiesDialog()
            clickStudiesDialogCancelButton()
            verifyStudiesToggle(true)
        }
    }

    @Test
    fun sitePermissionsItemsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuSitePermissions {
            verifySitePermissionsToolbarTitle()
            verifyToolbarGoBackButton()
            verifySitePermissionOption("Autoplay", "Block audio only")
            verifySitePermissionOption("Camera", "Blocked by Android")
            verifySitePermissionOption("Location", "Blocked by Android")
            verifySitePermissionOption("Microphone", "Blocked by Android")
            verifySitePermissionOption("Notification", "Ask to allow")
            verifySitePermissionOption("Persistent Storage", "Ask to allow")
            verifySitePermissionOption("Cross-site cookies", "Ask to allow")
            verifySitePermissionOption("DRM-controlled content", "Ask to allow")
            verifySitePermissionOption("Exceptions")
        }
    }
}
