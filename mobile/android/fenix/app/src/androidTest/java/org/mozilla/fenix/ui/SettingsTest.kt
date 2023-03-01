/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.ui.robots.homeScreen

/**
 *  Tests for verifying the main three dot menu options
 *
 */

class SettingsTest {
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

    // Walks through settings privacy menu and sub-menus to ensure all items are present
    @Test
    fun settingsPrivacyItemsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
            // PRIVACY
            verifyPrivacyHeading()

            // PRIVATE BROWSING
            verifyPrivateBrowsingButton()
        }.openPrivateBrowsingSubMenu {
            verifyNavigationToolBarHeader()
        }.goBack {
            // HTTPS-Only Mode
            verifyHTTPSOnlyModeButton()
            verifyHTTPSOnlyModeState("Off")

            // ENHANCED TRACKING PROTECTION
            verifyEnhancedTrackingProtectionButton()
            verifyEnhancedTrackingProtectionState("Standard")
        }.openEnhancedTrackingProtectionSubMenu {
            verifyNavigationToolBarHeader()
            verifyEnhancedTrackingProtectionProtectionSubMenuItems()

            // ENHANCED TRACKING PROTECTION EXCEPTION
        }.openExceptions {
            verifyNavigationToolBarHeader()
            verifyEnhancedTrackingProtectionProtectionExceptionsSubMenuItems()
        }.goBack {
        }.goBack {
            // SITE PERMISSIONS
            verifySitePermissionsButton()
        }.openSettingsSubMenuSitePermissions {
            verifyNavigationToolBarHeader()
            verifySitePermissionsSubMenuItems()

            // SITE PERMISSIONS AUTOPLAY
        }.openAutoPlay {
            verifyNavigationToolBarHeader("Autoplay")
            verifySitePermissionsAutoPlaySubMenuItems()
        }.goBack {
            // SITE PERMISSIONS CAMERA
        }.openCamera {
            verifyNavigationToolBarHeader("Camera")
            verifySitePermissionsCommonSubMenuItems()
            verifyToggleNameToON("3. Toggle Camera to ON")
        }.goBack {
            // SITE PERMISSIONS LOCATION
        }.openLocation {
            verifyNavigationToolBarHeader("Location")
            verifySitePermissionsCommonSubMenuItems()
            verifyToggleNameToON("3. Toggle Location to ON")
        }.goBack {
            // SITE PERMISSIONS MICROPHONE
        }.openMicrophone {
            verifyNavigationToolBarHeader("Microphone")
            verifySitePermissionsCommonSubMenuItems()
            verifyToggleNameToON("3. Toggle Microphone to ON")
        }.goBack {
            // SITE PERMISSIONS NOTIFICATION
        }.openNotification {
            verifyNavigationToolBarHeader("Notification")
            verifySitePermissionsNotificationSubMenuItems()
        }.goBack {
            // SITE PERMISSIONS PERSISTENT STORAGE
        }.openPersistentStorage {
            verifyNavigationToolBarHeader("Persistent Storage")
            verifySitePermissionsPersistentStorageSubMenuItems()
        }.goBack {
            // SITE PERMISSIONS EXCEPTIONS
        }.openExceptions {
            verifyNavigationToolBarHeader()
            verifySitePermissionsExceptionSubMenuItems()
        }.goBack {
        }.goBack {
            // DELETE BROWSING DATA
            verifyDeleteBrowsingDataButton()
        }.openSettingsSubMenuDeleteBrowsingData {
            verifyNavigationToolBarHeader()
            verifyDeleteBrowsingDataSubMenuItems()
        }.goBack {
            // DELETE BROWSING DATA ON QUIT
            verifyDeleteBrowsingDataOnQuitButton()
            verifyDeleteBrowsingDataOnQuitState("Off")
        }.openSettingsSubMenuDeleteBrowsingDataOnQuit {
            verifyNavigationToolBarHeader()
            verifyDeleteBrowsingDataOnQuitSubMenuItems()
        }.goBack {
            // NOTIFICATIONS
            verifyNotificationsButton()
        }.openSettingsSubMenuNotifications {
            verifySystemNotificationsView()
        }.goBack {
            // DATA COLLECTION
            verifyDataCollectionButton()
        }.openSettingsSubMenuDataCollection {
            verifyNavigationToolBarHeader()
            verifyDataCollectionSubMenuItems()
        }.goBack {
        }.goBack {
            verifyHomeComponent()
        }
    }

    // Walks through settings menu and sub-menus to ensure all items are present
    @Ignore("This is a stub test, ignore for now")
    @Test
    fun settingsMenusItemsTest() {
        // SYNC

        // see: SettingsSyncTest

        // BASICS

        // see: SettingsBasicsTest

        // PRIVACY

        // see: SettingsPrivacyTest

        // DEVELOPER TOOLS

        // Verify header: "Developer Tools"
        // Verify item: "Remote debugging via USB" and default toggle value: "Off"

        // ABOUT

        // Verify header: "About"
        // Verify item: "Help"
        // Verify item: "Rate on Google Play"
        // Verify item: "About Firefox Preview"
        //
    }

    // SYNC
    // see: SettingsSyncTest

    // BASICS
    // see: SettingsBasicsTest
    //
    // PRIVACY
    // see: SettingsPrivacyTest

    // DEVELOPER TOOLS
    @Ignore("This is a stub test, ignore for now")
    @Test
    fun turnOnRemoteDebuggingViaUsb() {
        // Open terminal
        // Verify USB debugging is off
        // Open 3dot (main) menu
        // Select settings
        // Toggle Remote debugging via USB to 'on'
        // Open terminal
        // Verify USB debugging is on
    }

    // ABOUT
    @Ignore("This is a stub test, ignore for now")
    @Test
    fun verifyHelpRedirect() {
        // Open 3dot (main) menu
        // Select settings
        // Click on "Help"
        // Verify redirect to: https://support.mozilla.org/
    }

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun verifyRateOnGooglePlayRedirect() {
        // Open 3dot (main) menu
        // Select settings
        // Click on "Rate on Google Play"
        // Verify Android "Open with Google Play Store" sub menu
    }

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun verifyAboutFirefoxPreview() {
        // Open 3dot (main) menu
        // Select settings
        // Click on "Verify About Firefox Preview"
        // Verify about page contains....
        // Build #
        // Version #
        // "Firefox Preview is produced by Mozilla"
        // Day, Date, timestamp
        // "Open source libraries we use"
    }
}
