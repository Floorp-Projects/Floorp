/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import android.Manifest
import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import androidx.core.net.toUri
import androidx.test.espresso.Espresso.pressBack
import androidx.test.rule.GrantPermissionRule
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MatcherHelper
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestAssetHelper.getStorageTestAsset
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.helpers.TestHelper.deleteDownloadedFileOnStorage
import org.mozilla.fenix.helpers.TestHelper.exitMenu
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.restartApp
import org.mozilla.fenix.helpers.TestHelper.setNetworkEnabled
import org.mozilla.fenix.ui.robots.clickPageObject
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying the Settings for:
 *  Delete Browsing Data on quit
 *
 */
class ComposeSettingsDeleteBrowsingDataOnQuitTest {
    private lateinit var mockWebServer: MockWebServer

    @get:Rule(order = 0)
    val composeTestRule =
        AndroidComposeTestRule(
            HomeActivityIntentTestRule.withDefaultSettingsOverrides(
                skipOnboarding = true,
                tabsTrayRewriteEnabled = true,
            ),
        ) { it.activity }

    // Automatically allows app permissions, avoiding a system dialog showing up.
    @get:Rule(order = 1)
    val grantPermissionRule: GrantPermissionRule = GrantPermissionRule.grant(
        Manifest.permission.RECORD_AUDIO,
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
    fun deleteBrowsingDataOnQuitSettingTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuDeleteBrowsingDataOnQuit {
            verifyNavigationToolBarHeader()
            verifyDeleteBrowsingOnQuitEnabled(false)
            verifyDeleteBrowsingOnQuitButtonSummary()
            verifyDeleteBrowsingOnQuitEnabled(false)
            clickDeleteBrowsingOnQuitButtonSwitch()
            verifyDeleteBrowsingOnQuitEnabled(true)
            verifyAllTheCheckBoxesText()
            verifyAllTheCheckBoxesChecked(true)
        }.goBack {
            verifySettingsOptionSummary("Delete browsing data on quit", "On")
        }.goBack {
        }.openThreeDotMenu {
            verifyQuitButtonExists()
            pressBack()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser("test".toUri()) {
        }.openThreeDotMenu {
            verifyQuitButtonExists()
        }
    }

    @Test
    fun deleteOpenTabsOnQuitTest() {
        val testPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuDeleteBrowsingDataOnQuit {
            clickDeleteBrowsingOnQuitButtonSwitch()
            exitMenu()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
        }.goToHomescreen {
        }.openThreeDotMenu {
            clickQuit()
            restartApp(composeTestRule.activityRule)
        }
        homeScreen {
        }.openComposeTabDrawer(composeTestRule) {
            verifyNoOpenTabsInNormalBrowsing()
        }
    }

    @Test
    fun deleteBrowsingHistoryOnQuitTest() {
        val genericPage =
            getStorageTestAsset(mockWebServer, "generic1.html")

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuDeleteBrowsingDataOnQuit {
            clickDeleteBrowsingOnQuitButtonSwitch()
            exitMenu()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericPage.url) {
        }.goToHomescreen {
        }.openThreeDotMenu {
            clickQuit()
            restartApp(composeTestRule.activityRule)
        }

        homeScreen {
        }.openThreeDotMenu {
        }.openHistory {
            verifyEmptyHistoryView()
            exitMenu()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/416051
    @Test
    fun deleteCookiesAndSiteDataOnQuitTest() {
        val storageWritePage =
            getStorageTestAsset(mockWebServer, "storage_write.html")
        val storageCheckPage =
            getStorageTestAsset(mockWebServer, "storage_check.html")

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuDeleteBrowsingDataOnQuit {
            clickDeleteBrowsingOnQuitButtonSwitch()
            exitMenu()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(storageWritePage.url) {
            clickPageObject(MatcherHelper.itemWithText("Set cookies"))
            verifyPageContent("Values written to storage")
        }.goToHomescreen {
        }.openThreeDotMenu {
            clickQuit()
            restartApp(composeTestRule.activityRule)
        }

        navigationToolbar {
        }.enterURLAndEnterToBrowser(storageCheckPage.url) {
            verifyPageContent("Session storage empty")
            verifyPageContent("Local storage empty")
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(storageWritePage.url) {
            verifyPageContent("No cookies set")
        }
    }

    @SmokeTest
    @Test
    fun deleteDownloadsOnQuitTest() {
        val downloadTestPage = "https://storage.googleapis.com/mobile_test_assets/test_app/downloads.html"

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuDeleteBrowsingDataOnQuit {
            clickDeleteBrowsingOnQuitButtonSwitch()
            exitMenu()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(downloadTestPage.toUri()) {
        }.clickDownloadLink("smallZip.zip") {
            verifyDownloadPrompt("smallZip.zip")
        }.clickDownload {
            verifyDownloadNotificationPopup()
        }.closeCompletedDownloadPrompt {
        }.goToHomescreen {
        }.openThreeDotMenu {
            clickQuit()
            mDevice.waitForIdle()
        }
        restartApp(composeTestRule.activityRule)
        homeScreen {
        }.openThreeDotMenu {
        }.openDownloadsManager {
            verifyEmptyDownloadsList()
        }
        deleteDownloadedFileOnStorage("smallZip.zip")
    }

    @SmokeTest
    @Test
    fun deleteSitePermissionsOnQuitTest() {
        val testPage = "https://mozilla-mobile.github.io/testapp/permissions"
        val testPageSubstring = "https://mozilla-mobile.github.io:443"

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuDeleteBrowsingDataOnQuit {
            clickDeleteBrowsingOnQuitButtonSwitch()
            exitMenu()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
            waitForPageToLoad()
        }.clickStartMicrophoneButton {
            verifyMicrophonePermissionPrompt(testPageSubstring)
            selectRememberPermissionDecision()
        }.clickPagePermissionButton(false) {
            verifyPageContent("Microphone not allowed")
        }.goToHomescreen {
        }.openThreeDotMenu {
            clickQuit()
            mDevice.waitForIdle()
        }
        restartApp(composeTestRule.activityRule)
        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
            waitForPageToLoad()
        }.clickStartMicrophoneButton {
            verifyMicrophonePermissionPrompt(testPageSubstring)
        }
    }

    @Test
    fun deleteCachedFilesOnQuitTest() {
        val pocketTopArticles = TestHelper.getStringResource(R.string.pocket_pinned_top_articles)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuDeleteBrowsingDataOnQuit {
            clickDeleteBrowsingOnQuitButtonSwitch()
            exitMenu()
        }
        homeScreen {
            verifyExistingTopSitesTabs(pocketTopArticles)
        }.openTopSiteTabWithTitle(pocketTopArticles) {
            waitForPageToLoad()
        }.goToHomescreen {
        }.openThreeDotMenu {
            clickQuit()
            mDevice.waitForIdle()
        }
        // disabling wifi to prevent downloads in the background
        setNetworkEnabled(enabled = false)
        restartApp(composeTestRule.activityRule)
        navigationToolbar {
        }.enterURLAndEnterToBrowser("about:cache".toUri()) {
            verifyNetworkCacheIsEmpty("memory")
            verifyNetworkCacheIsEmpty("disk")
        }
        setNetworkEnabled(enabled = true)
    }
}
