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
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.helpers.TestHelper.openAppFromExternalLink
import org.mozilla.fenix.helpers.TestHelper.restartApp
import org.mozilla.fenix.ui.robots.addToHomeScreen
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying the main three dot menu options
 *
 */

class SettingsPrivacyTest {
    /* ktlint-disable no-blank-line-before-rbrace */ // This imposes unreadable grouping.

    private val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
    private lateinit var mockWebServer: MockWebServer
    private val pageShortcutName = "TestShortcut"

    @get:Rule
    val activityTestRule = HomeActivityTestRule()

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
    // Walks through settings privacy menu and sub-menus to ensure all items are present
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

            // ENHANCED TRACKING PROTECTION
            verifyEnhancedTrackingProtectionButton()
            verifyEnhancedTrackingProtectionValue("On")
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
            verifyDeleteBrowsingDataOnQuitValue("Off")
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

    // Tests only for initial state without signing in.
    // For tests after singing in, see SyncIntegration test suite

    @Test
    fun loginsAndPasswordsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
            // Necessary to scroll a little bit for all screen sizes
            TestHelper.scrollToElementByText("Logins and passwords")
        }.openLoginsAndPasswordSubMenu {
            verifyDefaultView()
            verifyDefaultValueAutofillLogins()
            verifyDefaultValueExceptions()
        }.openSavedLogins {
            verifySecurityPromptForLogins()
            tapSetupLater()
            // Verify that logins list is empty
            // Issue #7272 nothing is shown
        }.goBack {
        }.openSyncLogins {
            verifyReadyToScanOption()
            verifyUseEmailOption()
        }
    }

    @Test
    fun saveLoginFromPromptTest() {
        val saveLoginTest =
            TestAssetHelper.getSaveLoginAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(saveLoginTest.url) {
            verifySaveLoginPromptIsShown()
            // Click save to save the login
            saveLoginFromPrompt("Save")
        }.openTabDrawer {
        }.openNewTab {
        }.dismissSearchBar {
        }.openThreeDotMenu {
        }.openSettings {
            TestHelper.scrollToElementByText("Logins and passwords")
        }.openLoginsAndPasswordSubMenu {
            verifyDefaultView()
        }.openSavedLogins {
            verifySecurityPromptForLogins()
            tapSetupLater()
            // Verify that the login appears correctly
            verifySavedLoginFromPrompt()
        }
    }

    @Test
    fun neverSaveLoginFromPromptTest() {
        val saveLoginTest = TestAssetHelper.getSaveLoginAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(saveLoginTest.url) {
            verifySaveLoginPromptIsShown()
            // Don't save the login, add to exceptions
            saveLoginFromPrompt("Never save")
        }.openTabDrawer {
        }.openNewTab {
        }.dismissSearchBar {
        }.openThreeDotMenu {
        }.openSettings {
        }.openLoginsAndPasswordSubMenu {
            verifyDefaultView()
        }.openSavedLogins {
            verifySecurityPromptForLogins()
            tapSetupLater()
            // Verify that the login list is empty
            verifyNotSavedLoginFromPrompt()
        }.goBack {
        }.openLoginExceptions {
            // Verify localhost was added to exceptions list
            verifyLocalhostExceptionAdded()
        }
    }

    @Test
    fun saveLoginsAndPasswordsOptions() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openLoginsAndPasswordSubMenu {
        }.saveLoginsAndPasswordsOptions {
            verifySaveLoginsOptionsView()
        }
    }

    @Test
    fun verifyPrivateBrowsingMenuItemsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openPrivateBrowsingSubMenu {
            verifyAddPrivateBrowsingShortcutButton()
            verifyOpenLinksInPrivateTab()
            verifyOpenLinksInPrivateTabOff()
        }.goBack {
            verifySettingsView()
        }
    }

    @Test
    fun openExternalLinksInPrivateTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val secondWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)

        setOpenLinksInPrivateOn()

        openAppFromExternalLink(firstWebPage.url.toString())

        browserScreen {
        }.openTabDrawer {
            verifyPrivateModeSelected()
        }.closeTabDrawer {
        }.goToHomescreen { }

        setOpenLinksInPrivateOff()

        // We need to open a different link, otherwise it will open the same session
        openAppFromExternalLink(secondWebPage.url.toString())

        browserScreen {
        }.openTabDrawer {
            verifyNormalModeSelected()
        }
    }

    @Test
    fun launchPageShortcutInPrivateModeTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        setOpenLinksInPrivateOn()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.openAddToHomeScreen {
            addShortcutName(pageShortcutName)
            clickAddShortcutButton()
            clickAddAutomaticallyButton()
        }

        mDevice.waitForIdle()
        // We need to close the existing tab here, to open a different session
        restartApp(activityTestRule)
        browserScreen {
        }.openTabDrawer {
            closeTab()
        }

        addToHomeScreen {
        }.searchAndOpenHomeScreenShortcut(pageShortcutName) {
        }.openTabDrawer {
            verifyPrivateModeSelected()
        }
    }

    @Test
    fun launchLinksInPrivateToggleOffStateDoesntChangeTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        setOpenLinksInPrivateOn()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
            expandMenu()
        }.openAddToHomeScreen {
            addShortcutName(pageShortcutName)
            clickAddShortcutButton()
            clickAddAutomaticallyButton()
        }.openHomeScreenShortcut(pageShortcutName) {
        }.goToHomescreen { }

        setOpenLinksInPrivateOff()
        restartApp(activityTestRule)
        mDevice.waitForIdle()

        addToHomeScreen {
        }.searchAndOpenHomeScreenShortcut(pageShortcutName) {
        }.openTabDrawer {
            verifyNormalModeSelected()
        }.closeTabDrawer {
        }.openThreeDotMenu {
        }.openSettings {
        }.openPrivateBrowsingSubMenu {
            verifyOpenLinksInPrivateTabOff()
        }
    }

    @Test
    fun addPrivateBrowsingShortcut() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openPrivateBrowsingSubMenu {
            addPrivateShortcutToHomescreen()
            verifyPrivateBrowsingShortcutIcon()
        }.openPrivateBrowsingShortcut {
            verifySearchView()
        }.openBrowser {
        }.openTabDrawer {
            verifyPrivateModeSelected()
        }
    }

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun toggleTrackingProtection() {
        // Open static test website to verify TP is turned on (default): https://github.com/rpappalax/testapp
        // (static content needs to be migrated to assets folder)
        // Open 3dot (main) menu
        // Select settings
        // Toggle Tracking Protection to 'off'
        // Back arrow to Home
        // Open static test website to verify TP is now off: https://github.com/rpappalax/testapp
    }

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun verifySitePermissions() {
        // Open 3dot (main) menu
        // Select settings
        // Click on: "Site permissions"
        // Verify sub-menu items...
        // Click on: "Exceptions"
        // Verify: "No site exceptions"
        // TBD: create a site exception
        // TBD: return to this UI and verify

        //
        // Open browser to static test website: https://github.com/rpappalax/testapp
        // Click on "Test site permissions: geolocation"
        // Verify that geolocation permissions dialogue is opened
        // Verify text: "Allow <website URL> to use your geolocation?
        // Verify toggle: 'Remember decision for this site?"
        // Verify button: "Don't Allow"
        // Verify button: "Allow" (default)
        // Select "Remember decision for this site"
        // Refresh page
        // Click on "Test site permissions: geolocation"
        // Verify that geolocation permissions dialogue is not opened
        //
        //
        // Open browser to static test website: https://github.com/rpappalax/testapp
        // Click on "Test site permissions: camera"
        // Verify that camera permissions dialogue is opened
        // Verify text: "Allow <website URL> to use your camera?
        // Verify toggle: 'Remember decision for this site?"
        // Verify button: "Don't Allow"
        // Verify button: "Allow" (default)
        // Select "Remember decision for this site"
        // Refresh page
        // Click on "Test site permissions: camera"
        // Verify that camera permissions dialogue is not opened
        //
        //
        // Open browser to static test website: https://github.com/rpappalax/testapp
        // Click on "Test site permissions: microphone"
        // Verify that microphone permissions dialogue is opened
        // Verify text: "Allow <website URL> to use your microphone?
        // Verify toggle: 'Remember decision for this site?"
        // Verify button: "Don't Allow"
        // Verify button: "Allow" (default)
        // Select "Remember decision for this site"
        // Refresh page
        // Click on "Test site permissions: microphone"
        // Verify that microphone permissions dialogue is not opened
        //
        //
        // Open browser to static test website: https://github.com/rpappalax/testapp
        // Click on "Test site permissions: notifications dialogue"
        // Verify that notifications dialogue permissions dialogue is opened
        // Verify text: "Allow <website URL> to send notifications?
        // Verify toggle: 'Remember decision for this site?"
        // Verify button: "Never"
        // Verify button: "Always" (default)
        // Select "Remember decision for this site"
        // Refresh page
        // Click on "Test site permissions: notifications dialogue"
        // Verify that notifications dialogue permissions dialogue is not opened
        //

        // Open 3dot (main) menu
        // Select settings
        // Click on: "Site permissions"
        // Select: Camera
        // Switch from "ask to allow" (default) to "blocked"
        // Click back arrow
        //
        // Select: Location
        // Switch from "ask to allow" (default) to "blocked"
        // Click back arrow
        //
        // Select: Microphone
        // Switch from "ask to allow" (default) to "blocked"
        // Click back arrow
        //
        // Select: Notification
        // Switch from "ask to allow" (default) to "blocked"
        // Click back arrow
        //

        // Open browser to static test website: https://github.com/rpappalax/testapp
        // Click on "Test site permissions: camera dialogue"
        // Verify that notifications dialogue permissions dialogue is not opened
        //
        // Open browser to static test website: https://github.com/rpappalax/testapp
        // Click on "Test site permissions: geolocation dialogue"
        // Verify that notifications dialogue permissions dialogue is not opened
        //
        // Open browser to static test website: https://github.com/rpappalax/testapp
        // Click on "Test site permissions: microphone dialogue"
        // Verify that notifications dialogue permissions dialogue is not opened
        //
        // Open browser to static test website: https://github.com/rpappalax/testapp
        // Click on "Test site permissions: notifications dialogue"
        // Verify that notifications dialogue permissions dialogue is not opened
    }

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun deleteBrowsingData() {
        // Setup:
        // Open 2 websites as 2 tabs
        // Save as 1 collection
        // Open 2 more websites in 2 other tabs
        // Save as a 2nd collection

        // Open 3dot (main) menu
        // Select settings
        // Click on "Delete browsing data"
        // Verify correct number of tabs, addresses and collections are indicated
        // Select all 3 checkboxes
        // Click on "Delete browsing data button"
        // Return to home screen and verify that all tabs, history and collection are gone
        //
        // Verify xxx
        //
        // New: If coming from  tab -> settings -> delete browsing data
        // then expect to return to home screen
        // If coming from tab -> home -> settings -> delete browsing data
        // then expect return to settings (after which you can return to home manually)
    }

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun verifyDataCollection() {
        // Open 3dot (main) menu
        // Select settings
        // Click on "Data collection"
        // Verify header: "Usage and technical data"
        // Verify text: "Shares performance, usage, hardware and customization data about your browser with Mozilla"
        //               " to help us make Firefox preview better"
        // Verify toggle is on by default
        // TBD:
        // see: telemetry testcases
    }

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun openPrivacyNotice() {
        // Open 3dot (main) menu
        // Select settings
        // Click on "Privacy notice"
        // Verify redirect to: mozilla.org Privacy notice page"
    }

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun checkLeakCanary() {
        // Open 3dot (main) menu
        // Select settings
        // Click on Leak Canary toggle
        // Verify 'dump' message
    }
}

private fun setOpenLinksInPrivateOn() {
    homeScreen {
    }.openThreeDotMenu {
    }.openSettings {
    }.openPrivateBrowsingSubMenu {
        verifyOpenLinksInPrivateTabEnabled()
        clickOpenLinksInPrivateTabSwitch()
    }.goBack {
    }.goBack {
        verifyHomeComponent()
    }
}

private fun setOpenLinksInPrivateOff() {
    homeScreen {
    }.openThreeDotMenu {
    }.openSettings {
    }.openPrivateBrowsingSubMenu {
        clickOpenLinksInPrivateTabSwitch()
        verifyOpenLinksInPrivateTabOff()
    }.goBack {
    }.goBack {
        verifyHomeComponent()
    }
}
