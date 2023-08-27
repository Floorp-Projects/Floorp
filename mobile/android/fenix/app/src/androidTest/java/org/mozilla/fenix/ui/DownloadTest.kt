/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.core.net.toUri
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.Constants.PackageName.GOOGLE_DOCS
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.assertExternalAppOpens
import org.mozilla.fenix.helpers.TestHelper.clickSnackbarButton
import org.mozilla.fenix.helpers.TestHelper.deleteDownloadedFileOnStorage
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.setNetworkEnabled
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.clickPageObject
import org.mozilla.fenix.ui.robots.downloadRobot
import org.mozilla.fenix.ui.robots.homeScreen
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
    private lateinit var mockWebServer: MockWebServer

    /* Remote test page managed by Mozilla Mobile QA team at https://github.com/mozilla-mobile/testapp */
    private val downloadTestPage = "https://storage.googleapis.com/mobile_test_assets/test_app/downloads.html"
    private var downloadFile: String = ""

    @get:Rule
    val activityTestRule = HomeActivityIntentTestRule.withDefaultSettingsOverrides()

    @Before
    fun setUp() {
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }

        // clear all existing notifications
        notificationShade {
            mDevice.openNotification()
            clearNotifications()
        }
    }

    @After
    fun tearDown() {
        notificationShade {
            cancelAllShownNotifications()
        }

        mockWebServer.shutdown()

        setNetworkEnabled(enabled = true)
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
        deleteDownloadedFileOnStorage(downloadFile)
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
        }
        mDevice.openNotification()
        notificationShade {
            verifySystemNotificationExists("Download completed")
        }
        deleteDownloadedFileOnStorage(downloadFile)
    }

    @Ignore("Failing: Bug https://bugzilla.mozilla.org/show_bug.cgi?id=1813521")
    @SmokeTest
    @Test
    fun pauseResumeCancelDownloadTest() {
        // Clear the "Firefox Fenix default browser notification"
        notificationShade {
            cancelAllShownNotifications()
        }

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
        deleteDownloadedFileOnStorage(downloadFile)
    }

    /* Verifies downloads in the Downloads Menu:
          - downloads appear in the list
          - deleting a download from device storage, removes it from the Downloads Menu too
     */
    @SmokeTest
    @Test
    fun manageDownloadsInDownloadsMenuTest() {
        // a long filename to verify it's correctly displayed on the prompt and in the Downloads menu
        downloadFile =
            "tAJwqaWjJsXS8AhzSninBMCfIZbHBGgcc001lx5DIdDwIcfEgQ6vE5Gb5VgAled17DFZ2A7ZDOHA0NpQPHXXFt.svg"

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
            deleteDownloadedFileOnStorage(downloadFile)
        }.exitDownloadsManagerToBrowser {
        }.openThreeDotMenu {
        }.openDownloadsManager {
            verifyEmptyDownloadsList()
        }
    }

    @SmokeTest
    @Test
    fun openDownloadedFileTest() {
        downloadFile = "web_icon.png"

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
            verifyDownloadedFileName(downloadFile)
            openDownloadedFile(downloadFile)
            verifyPhotosAppOpens()
            mDevice.pressBack()
        }
        deleteDownloadedFileOnStorage(downloadFile)
    }

    // Save PDF file from the share overlay
    @SmokeTest
    @Test
    fun saveAndOpenPdfTest() {
        val genericURL =
            TestAssetHelper.getGenericAsset(mockWebServer, 3)
        downloadFile = "pdfForm.pdf"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            clickPageObject(itemWithText("PDF form file"))
        }.openThreeDotMenu {
        }.clickShareButton {
        }.clickSaveAsPDF {
            verifyDownloadPrompt(downloadFile)
        }.clickDownload {
        }.clickOpen("application/pdf") {
            assertExternalAppOpens(GOOGLE_DOCS)
        }
        deleteDownloadedFileOnStorage(downloadFile)
    }

    @Test
    fun deleteDownloadedFileTest() {
        downloadFile = "smallZip.zip"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(downloadTestPage.toUri()) {
            waitForPageToLoad()
        }.clickDownloadLink(downloadFile) {
            verifyDownloadPrompt(downloadFile)
        }.clickDownload {
            verifyDownloadedFileName(downloadFile)
        }
        browserScreen {
        }.openThreeDotMenu {
        }.openDownloadsManager {
            verifyDownloadedFileName(downloadFile)
            deleteDownloadedItem(downloadFile)
            verifyEmptyDownloadsList()
        }
        deleteDownloadedFileOnStorage(downloadFile)
    }

    @Test
    fun undoDeleteDownloadedFileTest() {
        downloadFile = "smallZip.zip"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(downloadTestPage.toUri()) {
            waitForPageToLoad()
        }.clickDownloadLink(downloadFile) {
            verifyDownloadPrompt(downloadFile)
        }.clickDownload {
            verifyDownloadedFileName(downloadFile)
        }
        browserScreen {
        }.openThreeDotMenu {
        }.openDownloadsManager {
            verifyDownloadedFileName(downloadFile)
            deleteDownloadedItem(downloadFile)
            clickSnackbarButton("UNDO")
            verifyDownloadedFileName(downloadFile)
        }
        deleteDownloadedFileOnStorage(downloadFile)
    }

    @Test
    fun deleteMultipleDownloadedFilesTest() {
        val firstDownloadedFile = "smallZip.zip"
        val secondDownloadedFile = "textfile.txt"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(downloadTestPage.toUri()) {
            waitForPageToLoad()
        }.clickDownloadLink(firstDownloadedFile) {
            verifyDownloadPrompt(firstDownloadedFile)
        }.clickDownload {
            verifyDownloadedFileName(firstDownloadedFile)
        }.closeCompletedDownloadPrompt {
        }.clickDownloadLink(secondDownloadedFile) {
            verifyDownloadPrompt(secondDownloadedFile)
        }.clickDownload {
            verifyDownloadedFileName(secondDownloadedFile)
        }
        browserScreen {
        }.openThreeDotMenu {
        }.openDownloadsManager {
            verifyDownloadedFileName(firstDownloadedFile)
            verifyDownloadedFileName(secondDownloadedFile)
            longClickDownloadedItem(firstDownloadedFile)
            selectDownloadedItem(secondDownloadedFile)
            openMultiSelectMoreOptionsMenu()
            clickMultiSelectRemoveButton()
            verifyEmptyDownloadsList()
        }
        deleteDownloadedFileOnStorage(firstDownloadedFile)
        deleteDownloadedFileOnStorage(secondDownloadedFile)
    }

    @Test
    fun undoDeleteMultipleDownloadedFilesTest() {
        val firstDownloadedFile = "smallZip.zip"
        val secondDownloadedFile = "textfile.txt"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(downloadTestPage.toUri()) {
            waitForPageToLoad()
        }.clickDownloadLink(firstDownloadedFile) {
            verifyDownloadPrompt(firstDownloadedFile)
        }.clickDownload {
            verifyDownloadedFileName(firstDownloadedFile)
        }.closeCompletedDownloadPrompt {
        }.clickDownloadLink(secondDownloadedFile) {
            verifyDownloadPrompt(secondDownloadedFile)
        }.clickDownload {
            verifyDownloadedFileName(secondDownloadedFile)
        }
        browserScreen {
        }.openThreeDotMenu {
        }.openDownloadsManager {
            verifyDownloadedFileName(firstDownloadedFile)
            verifyDownloadedFileName(secondDownloadedFile)
            longClickDownloadedItem(firstDownloadedFile)
            selectDownloadedItem(secondDownloadedFile)
            openMultiSelectMoreOptionsMenu()
            clickMultiSelectRemoveButton()
            clickSnackbarButton("UNDO")
            verifyDownloadedFileName(firstDownloadedFile)
            verifyDownloadedFileName(secondDownloadedFile)
        }
        deleteDownloadedFileOnStorage(firstDownloadedFile)
        deleteDownloadedFileOnStorage(secondDownloadedFile)
    }

    @Ignore("Failing: https://bugzilla.mozilla.org/show_bug.cgi?id=1840994")
    @Test
    fun systemNotificationCantBeDismissedWhileDownloadingTest() {
        // Clear the "Firefox Fenix default browser notification"
        notificationShade {
            cancelAllShownNotifications()
        }

        downloadFile = "1GB.zip"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(downloadTestPage.toUri()) {
            waitForPageToLoad()
        }.clickDownloadLink(downloadFile) {
            verifyDownloadPrompt(downloadFile)
        }.clickDownload {
        }
        browserScreen {
        }.openNotificationShade {
            verifySystemNotificationExists("Firefox Fenix")
            expandNotificationMessage()
            swipeDownloadNotification("Left", false)
            clickDownloadNotificationControlButton("CANCEL")
        }
        deleteDownloadedFileOnStorage(downloadFile)
    }

    @Test
    fun systemNotificationCantBeDismissedWhileDownloadIsPausedTest() {
        // Clear the "Firefox Fenix default browser notification"
        notificationShade {
            cancelAllShownNotifications()
        }

        downloadFile = "1GB.zip"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(downloadTestPage.toUri()) {
            waitForPageToLoad()
        }.clickDownloadLink(downloadFile) {
            verifyDownloadPrompt(downloadFile)
        }.clickDownload {
        }
        browserScreen {
        }.openNotificationShade {
            verifySystemNotificationExists("Firefox Fenix")
            expandNotificationMessage()
            clickDownloadNotificationControlButton("PAUSE")
            swipeDownloadNotification("Left", false)
            verifySystemNotificationExists("Firefox Fenix")
        }.closeNotificationTray {
        }.openNotificationShade {
            verifySystemNotificationExists("Firefox Fenix")
            expandNotificationMessage()
            swipeDownloadNotification("Right", false)
            verifySystemNotificationExists("Firefox Fenix")
            clickDownloadNotificationControlButton("CANCEL")
        }
        deleteDownloadedFileOnStorage(downloadFile)
    }

    @Test
    fun notificationCanBeDismissedIfDownloadIsInterruptedTest() {
        // Clear the "Firefox Fenix default browser notification"
        notificationShade {
            cancelAllShownNotifications()
        }

        downloadFile = "1GB.zip"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(downloadTestPage.toUri()) {
            waitForPageToLoad()
        }.clickDownloadLink(downloadFile) {
            verifyDownloadPrompt(downloadFile)
        }.clickDownload {
        }

        setNetworkEnabled(enabled = false)

        browserScreen {
        }.openNotificationShade {
            verifySystemNotificationExists("Download failed")
            expandNotificationMessage()
            swipeDownloadNotification("Left", true)
            verifySystemNotificationDoesNotExist("Firefox Fenix")
        }.closeNotificationTray {
        }

        downloadRobot {
        }.closeDownloadPrompt {
            verifyDownloadPromptIsDismissed()
        }
        deleteDownloadedFileOnStorage(downloadFile)
    }

    @Test
    fun notificationCanBeDismissedIfDownloadIsCompletedTest() {
        // Clear the "Firefox Fenix default browser notification"
        notificationShade {
            cancelAllShownNotifications()
        }

        downloadFile = "smallZip.zip"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(downloadTestPage.toUri()) {
            waitForPageToLoad()
        }.clickDownloadLink(downloadFile) {
            verifyDownloadPrompt(downloadFile)
        }.clickDownload {
        }

        browserScreen {
        }.openNotificationShade {
            verifySystemNotificationExists("Download completed")
            swipeDownloadNotification("Left", true, false)
            verifySystemNotificationDoesNotExist("Firefox Fenix")
        }.closeNotificationTray {
        }

        downloadRobot {
        }.closeDownloadPrompt {
            verifyDownloadPromptIsDismissed()
        }
        deleteDownloadedFileOnStorage(downloadFile)
    }

    @Test
    fun stayInPrivateBrowsingPromptTest() {
        // Clear the "Firefox Fenix default browser notification"
        notificationShade {
            cancelAllShownNotifications()
        }

        downloadFile = "1GB.zip"

        homeScreen {
        }.togglePrivateBrowsingMode()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(downloadTestPage.toUri()) {
            waitForPageToLoad()
        }.clickDownloadLink(downloadFile) {
            verifyDownloadPrompt(downloadFile)
        }.clickDownload {
        }
        browserScreen {
        }.openTabDrawer {
            closeTab()
        }
        browserScreen {
            verifyCancelPrivateDownloadsPrompt("1")
            clickStayInPrivateBrowsingPromptButton()
        }.openNotificationShade {
            verifySystemNotificationExists("Firefox Fenix")
        }
        deleteDownloadedFileOnStorage(downloadFile)
    }

    @Test
    fun cancelActiveDownloadsFromPrivateBrowsingPromptTest() {
        // Clear the "Firefox Fenix default browser notification"
        notificationShade {
            cancelAllShownNotifications()
        }

        downloadFile = "1GB.zip"

        homeScreen {
        }.togglePrivateBrowsingMode()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(downloadTestPage.toUri()) {
            waitForPageToLoad()
        }.clickDownloadLink(downloadFile) {
            verifyDownloadPrompt(downloadFile)
        }.clickDownload {
        }
        browserScreen {
        }.openTabDrawer {
            closeTab()
        }
        browserScreen {
            verifyCancelPrivateDownloadsPrompt("1")
            clickCancelPrivateDownloadsPromptButton()
        }.openNotificationShade {
            verifySystemNotificationDoesNotExist("Firefox Fenix")
        }
        deleteDownloadedFileOnStorage(downloadFile)
    }

    // Save edited PDF file from the share overlay
    @Test
    fun saveEditedPdfTest() {
        val genericURL =
            TestAssetHelper.getGenericAsset(mockWebServer, 3)
        downloadFile = "pdfForm.pdf"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            clickPageObject(itemWithText("PDF form file"))
            waitForPageToLoad()
            fillPdfForm("Firefox")
        }.openThreeDotMenu {
        }.clickShareButton {
        }.clickSaveAsPDF {
            verifyDownloadPrompt("pdfForm.pdf")
        }.clickDownload {
        }.clickOpen("application/pdf") {
            assertExternalAppOpens(GOOGLE_DOCS)
        }
        deleteDownloadedFileOnStorage(downloadFile)
    }
}
