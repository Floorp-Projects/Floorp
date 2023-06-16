/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import android.Manifest
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
class SettingsDeleteBrowsingDataOnQuitTest {
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val activityTestRule = HomeActivityIntentTestRule.withDefaultSettingsOverrides(skipOnboarding = true)

    // Automatically allows app permissions, avoiding a system dialog showing up.
    @get:Rule
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
    fun deleteBrowsingDataOnQuitSettingsItemsTest() {
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
            restartApp(activityTestRule)
        }
        navigationToolbar {
        }.openTabTray {
            verifyNoOpenTabsInNormalBrowsing()
        }
    }

    @Test
    fun deleteHistoryAndSiteStorageOnQuitTest() {
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
            restartApp(activityTestRule)
        }

        homeScreen {
        }.openThreeDotMenu {
        }.openHistory {
            verifyEmptyHistoryView()
            exitMenu()
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
        restartApp(activityTestRule)
        homeScreen {
        }.openThreeDotMenu {
        }.openDownloadsManager {
            verifyEmptyDownloadsList()
        }
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
        restartApp(activityTestRule)
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
        restartApp(activityTestRule)
        navigationToolbar {
        }.enterURLAndEnterToBrowser("about:cache".toUri()) {
            verifyNetworkCacheIsEmpty("memory")
            verifyNetworkCacheIsEmpty("disk")
        }
        setNetworkEnabled(enabled = true)
    }
}
