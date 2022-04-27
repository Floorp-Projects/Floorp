/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.core.net.toUri
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.FeatureSettingsHelper
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.TestHelper.deleteDownloadFromStorage
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.downloadRobot
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.robots.notificationShade

/**
 *  Tests for verifying basic functionality of download
 *
 *  - Initiates a download
 *  - Verifies download prompt
 *  - Verifies download notification and actions
 *  - Verifies managing downloads inside the Downloads listing.
 **/
class DownloadTest {
    private val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
    private val featureSettingsHelper = FeatureSettingsHelper()
    /* Remote test page managed by Mozilla Mobile QA team at https://github.com/mozilla-mobile/testapp */
    private val downloadTestPage = "https://storage.googleapis.com/mobile_test_assets/test_app/downloads.html"
    private var downloadFile: String = ""

    @get:Rule
    val activityTestRule = HomeActivityIntentTestRule()

    @Before
    fun setUp() {
        // disabling the jump-back-in pop-up that interferes with the tests.
        featureSettingsHelper.setJumpBackCFREnabled(false)
        // disabling the PWA CFR on 3rd visit
        featureSettingsHelper.disablePwaCFR(true)
        // clear all existing notifications
        notificationShade {
            mDevice.openNotification()
            clearNotifications()
        }
    }

    @After
    fun tearDown() {
        featureSettingsHelper.resetAllFeatureFlags()
        notificationShade {
            cancelAllShownNotifications()
        }
    }

    @Test
    fun testDownloadPrompt() {
        downloadFile = "web_icon.png"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(downloadTestPage.toUri()) {
            waitForPageToLoad()
        }.clickDownloadLink(downloadFile) {
            verifyDownloadPrompt(downloadFile)
        }.clickDownload {
            verifyDownloadNotificationPopup()
        }.clickOpen("image/png") {}
        downloadRobot {
            verifyPhotosAppOpens()
        }
        mDevice.pressBack()
    }

    @Test
    fun testCloseDownloadPrompt() {
        downloadFile = "smallZip.zip"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(downloadTestPage.toUri()) {
            waitForPageToLoad()
        }.clickDownloadLink(downloadFile) {
            verifyDownloadPrompt(downloadFile)
        }.closePrompt {
        }.openThreeDotMenu {
        }.openDownloadsManager {
            verifyEmptyDownloadsList()
        }
    }

    @Test
    fun testDownloadCompleteNotification() {
        downloadFile = "smallZip.zip"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(downloadTestPage.toUri()) {
            waitForPageToLoad()
        }.clickDownloadLink(downloadFile) {
            verifyDownloadPrompt(downloadFile)
        }.clickDownload {
            verifyDownloadNotificationPopup()
        }.closePrompt { }
        mDevice.openNotification()
        notificationShade {
            verifySystemNotificationExists("Download completed")
        }
    }

    @SmokeTest
    @Test
    fun pauseResumeCancelDownloadTest() {
        downloadFile = "1GB.zip"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(downloadTestPage.toUri()) {
            waitForPageToLoad()
        }.clickDownloadLink(downloadFile) {
            verifyDownloadPrompt(downloadFile)
        }.clickDownload {}
        mDevice.openNotification()
        notificationShade {
            verifySystemNotificationExists("Firefox Fenix")
            expandNotificationMessage()
            clickDownloadNotificationControlButton("PAUSE")
            clickDownloadNotificationControlButton("RESUME")
            clickDownloadNotificationControlButton("CANCEL")
            mDevice.pressBack()
        }
        browserScreen {
        }.openThreeDotMenu {
        }.openDownloadsManager {
            verifyEmptyDownloadsList()
        }
    }

    @SmokeTest
    @Test
        /* Verifies downloads in the Downloads Menu:
          - downloads appear in the list
          - deleting a download from device storage, removes it from the Downloads Menu too
        */
    fun manageDownloadsInDownloadsMenuTest() {
        // a long filename to verify it's correctly displayed on the prompt and in the Downloads menu
        downloadFile = "tAJwqaWjJsXS8AhzSninBMCfIZbHBGgcc001lx5DIdDwIcfEgQ6vE5Gb5VgAled17DFZ2A7ZDOHA0NpQPHXXFt.svg"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(downloadTestPage.toUri()) {
            waitForPageToLoad()
        }.clickDownloadLink(downloadFile) {
            verifyDownloadPrompt(downloadFile)
        }.clickDownload {
            verifyDownloadNotificationPopup()
        }
        browserScreen {
        }.openThreeDotMenu {
        }.openDownloadsManager {
            waitForDownloadsListToExist()
            verifyDownloadedFileName(downloadFile)
            verifyDownloadedFileIcon()
            openDownloadedFile(downloadFile)
            verifyPhotosAppOpens()
            deleteDownloadFromStorage()
            waitForDownloadsListToExist()
        }.exitDownloadsManagerToBrowser {
        }.openThreeDotMenu {
        }.openDownloadsManager {
            verifyEmptyDownloadsList()
        }
    }
}
