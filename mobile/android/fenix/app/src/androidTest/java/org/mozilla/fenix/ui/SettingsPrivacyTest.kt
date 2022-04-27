/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import android.os.Build
import android.view.autofill.AutofillManager
import androidx.core.net.toUri
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.RetryTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.helpers.TestHelper.appContext
import org.mozilla.fenix.helpers.TestHelper.openAppFromExternalLink
import org.mozilla.fenix.helpers.TestHelper.restartApp
import org.mozilla.fenix.ui.robots.addToHomeScreen
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.robots.settingsScreen

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
    val activityTestRule = HomeActivityIntentTestRule()

    @Rule
    @JvmField
    val retryTestRule = RetryTestRule(3)

    @Before
    fun setUp() {
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }

        val settings = activityTestRule.activity.applicationContext.settings()
        settings.shouldShowJumpBackInCFR = false

        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.R) {
            val autofillManager: AutofillManager =
                appContext.getSystemService(AutofillManager::class.java)
            autofillManager.disableAutofillServices()
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
            // HTTPS-Only Mode
            verifyHTTPSOnlyModeButton()
            verifyHTTPSOnlyModeState("Off")

            // ENHANCED TRACKING PROTECTION
            verifyEnhancedTrackingProtectionButton()
            verifyEnhancedTrackingProtectionState("On")
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
            verifyDefaultValueAutofillLogins(InstrumentationRegistry.getInstrumentation().targetContext)
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
        }
        browserScreen {
        }.openThreeDotMenu {
        }.openSettings {
            TestHelper.scrollToElementByText("Logins and passwords")
        }.openLoginsAndPasswordSubMenu {
            verifyDefaultView()
        }.openSavedLogins {
            verifySecurityPromptForLogins()
            tapSetupLater()
            // Verify that the login appears correctly
            verifySavedLoginFromPrompt("test@example.com")
        }
    }

    @Test
    fun neverSaveLoginFromPromptTest() {
        val saveLoginTest = TestAssetHelper.getSaveLoginAsset(mockWebServer)
        val settings = activityTestRule.activity.settings()
        settings.shouldShowJumpBackInCFR = false

        navigationToolbar {
        }.enterURLAndEnterToBrowser(saveLoginTest.url) {
            verifySaveLoginPromptIsShown()
            // Don't save the login, add to exceptions
            saveLoginFromPrompt("Never save")
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

    @SmokeTest
    @Test
    fun openWebsiteForSavedLoginTest() {
        val loginPage = "https://mozilla-mobile.github.io/testapp/loginForm"
        val originWebsite = "mozilla-mobile.github.io"
        val userName = "test"
        val password = "pass"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(loginPage.toUri()) {
            fillAndSubmitLoginCredentials(userName, password)
            saveLoginFromPrompt("Save")
        }.openThreeDotMenu {
        }.openSettings {
        }.openLoginsAndPasswordSubMenu {
        }.openSavedLogins {
            verifySecurityPromptForLogins()
            tapSetupLater()
            viewSavedLoginDetails(userName)
        }.goToSavedWebsite {
            verifyUrl(originWebsite)
        }
    }

    @SmokeTest
    @Test
    fun verifyMultipleLoginsSelectionsTest() {
        val loginPage = "https://mozilla-mobile.github.io/testapp/loginForm"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(loginPage.toUri()) {
            fillAndSubmitLoginCredentials("mozilla", "firefox")
            saveLoginFromPrompt("Save")
            fillAndSubmitLoginCredentials("firefox", "mozilla")
            saveLoginFromPrompt("Save")
            clearUserNameLoginCredential()
            clickSuggestedLoginsButton()
            verifySuggestedUserName("firefox")
            verifySuggestedUserName("mozilla")
            clickLoginSuggestion("mozilla")
            verifyPrefilledLoginCredentials("mozilla")
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

    @Ignore("Failing with frequent ANR: https://bugzilla.mozilla.org/show_bug.cgi?id=1764605")
    @Test
    fun launchLinksInPrivateToggleOffStateDoesntChangeTest() {
        val settings = activityTestRule.activity.applicationContext.settings()
        settings.shouldShowJumpBackInCFR = false
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
            cancelPrivateShortcutAddition()
            addPrivateShortcutToHomescreen()
            verifyPrivateBrowsingShortcutIcon()
        }.openPrivateBrowsingShortcut {
            verifySearchView()
        }.openBrowser {
        }.openTabDrawer {
            verifyPrivateModeSelected()
        }
    }

    @Test
    fun deleteBrowsingDataOptionStatesTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuDeleteBrowsingData {
            verifyAllCheckBoxesAreChecked()
            switchBrowsingHistoryCheckBox()
            switchCachedFilesCheckBox()
            verifyOpenTabsCheckBox(true)
            verifyBrowsingHistoryDetails(false)
            verifyCookiesCheckBox(true)
            verifyCachedFilesCheckBox(false)
            verifySitePermissionsCheckBox(true)
            verifyDownloadsCheckBox(true)
        }

        restartApp(activityTestRule)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuDeleteBrowsingData {
            verifyOpenTabsCheckBox(true)
            verifyBrowsingHistoryDetails(false)
            verifyCookiesCheckBox(true)
            verifyCachedFilesCheckBox(false)
            verifySitePermissionsCheckBox(true)
            verifyDownloadsCheckBox(true)
            switchOpenTabsCheckBox()
            switchBrowsingHistoryCheckBox()
            switchCookiesCheckBox()
            switchCachedFilesCheckBox()
            switchSitePermissionsCheckBox()
            switchDownloadsCheckBox()
            verifyOpenTabsCheckBox(false)
            verifyBrowsingHistoryDetails(true)
            verifyCookiesCheckBox(false)
            verifyCachedFilesCheckBox(true)
            verifySitePermissionsCheckBox(false)
            verifyDownloadsCheckBox(false)
        }

        restartApp(activityTestRule)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuDeleteBrowsingData {
            verifyOpenTabsCheckBox(false)
            verifyBrowsingHistoryDetails(true)
            verifyCookiesCheckBox(false)
            verifyCachedFilesCheckBox(true)
            verifySitePermissionsCheckBox(false)
            verifyDownloadsCheckBox(false)
        }
    }

    @Test
    fun deleteTabsDataWithNoOpenTabsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuDeleteBrowsingData {
            verifyAllCheckBoxesAreChecked()
            selectOnlyOpenTabsCheckBox()
            clickDeleteBrowsingDataButton()
            confirmDeletionAndAssertSnackbar()
        }
        settingsScreen {
            verifyGeneralHeading()
        }
    }

    @SmokeTest
    @Test
    fun deleteTabsDataTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            mDevice.waitForIdle()
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuDeleteBrowsingData {
            verifyAllCheckBoxesAreChecked()
            selectOnlyOpenTabsCheckBox()
            clickDeleteBrowsingDataButton()
            clickDialogCancelButton()
            verifyOpenTabsCheckBox(true)
            clickDeleteBrowsingDataButton()
            confirmDeletionAndAssertSnackbar()
        }
        settingsScreen {
            verifyGeneralHeading()
        }.openSettingsSubMenuDeleteBrowsingData {
            verifyOpenTabsDetails("0")
        }.goBack {
        }.goBack {
        }.openTabDrawer {
            verifyNoOpenTabsInNormalBrowsing()
        }
    }

    @SmokeTest
    @Test
    fun deleteDeleteBrowsingHistoryDataTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val secondWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
            mDevice.waitForIdle()
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(secondWebPage.url) {
            mDevice.waitForIdle()
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuDeleteBrowsingData {
            verifyBrowsingHistoryDetails("2")
            selectOnlyBrowsingHistoryCheckBox()
            clickDeleteBrowsingDataButton()
            clickDialogCancelButton()
            verifyBrowsingHistoryDetails(true)
            clickDeleteBrowsingDataButton()
            confirmDeletionAndAssertSnackbar()
            verifyBrowsingHistoryDetails("0")
        }.goBack {
            verifyGeneralHeading()
        }.goBack {
        }
        navigationToolbar {
        }.openThreeDotMenu {
        }.openHistory {
            verifyEmptyHistoryView()
        }
    }

    @SmokeTest
    @Test
    fun saveLoginsInPWATest() {
        val pwaPage = "https://mozilla-mobile.github.io/testapp/loginForm"
        val shortcutTitle = "TEST_APP"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(pwaPage.toUri()) {
            verifyNotificationDotOnMainMenu()
        }.openThreeDotMenu {
        }.clickInstall {
            clickAddAutomaticallyButton()
        }.openHomeScreenShortcut(shortcutTitle) {
            mDevice.waitForIdle()
            fillAndSubmitLoginCredentials("mozilla", "firefox")
            verifySaveLoginPromptIsDisplayed()
            saveLoginFromPrompt("Save")
            openAppFromExternalLink(pwaPage)

            browserScreen {
            }.openThreeDotMenu {
            }.openSettings {
            }.openLoginsAndPasswordSubMenu {
            }.openSavedLogins {
                verifySecurityPromptForLogins()
                tapSetupLater()
                verifySavedLoginFromPrompt("mozilla")
            }

            addToHomeScreen {
            }.searchAndOpenHomeScreenShortcut(shortcutTitle) {
                verifyPrefilledPWALoginCredentials("mozilla", shortcutTitle)
            }
        }
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
