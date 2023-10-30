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
import org.mozilla.fenix.helpers.AppAndSystemHelper.assertExternalAppOpens
import org.mozilla.fenix.helpers.AppAndSystemHelper.deleteDownloadedFileOnStorage
import org.mozilla.fenix.helpers.AppAndSystemHelper.setNetworkEnabled
import org.mozilla.fenix.helpers.Constants.PackageName.GOOGLE_APPS_PHOTOS
import org.mozilla.fenix.helpers.Constants.PackageName.GOOGLE_DOCS
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.clickSnackbarButton
import org.mozilla.fenix.helpers.TestHelper.exitMenu
import org.mozilla.fenix.helpers.TestHelper.mDevice
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/243844
    @Test
    fun verifyTheDownloadPromptsTest() {
        downloadFile = "web_icon.png"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(downloadTestPage.toUri()) {
            waitForPageToLoad()
        }.clickDownloadLink(downloadFile) {
            verifyDownloadPrompt(downloadFile)
        }.clickDownload {
            verifyDownloadCompleteNotificationPopup()
        }.clickOpen("image/png") {}
        downloadRobot {
            verifyPhotosAppOpens()
        }
        mDevice.pressBack()
        deleteDownloadedFileOnStorage(downloadFile)
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2299405
    @Test
    fun verifyTheDownloadFailedNotificationsTest() {
        downloadFile = "1GB.zip"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(downloadTestPage.toUri()) {
            waitForPageToLoad()
        }.clickDownloadLink(downloadFile) {
            verifyDownloadPrompt(downloadFile)
        }.clickDownload {
            setNetworkEnabled(enabled = false)
            verifyDownloadFailedPrompt(downloadFile)
            setNetworkEnabled(enabled = true)
            clickTryAgainButton()
        }
        mDevice.openNotification()
        notificationShade {
            verifySystemNotificationDoesNotExist("Download failed")
            verifySystemNotificationExists(downloadFile)
        }.closeNotificationTray {}
        deleteDownloadedFileOnStorage(downloadFile)
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2298616
    @Test
    fun verifyDownloadCompleteNotificationTest() {
        downloadFile = "web_icon.png"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(downloadTestPage.toUri()) {
            waitForPageToLoad()
        }.clickDownloadLink(downloadFile) {
            verifyDownloadPrompt(downloadFile)
        }.clickDownload {
            verifyDownloadCompleteNotificationPopup()
        }
        mDevice.openNotification()
        notificationShade {
            verifySystemNotificationExists("Download completed")
            clickNotification("Download completed")
            assertExternalAppOpens(GOOGLE_APPS_PHOTOS)
            mDevice.pressBack()
            mDevice.openNotification()
            verifySystemNotificationExists("Download completed")
            swipeDownloadNotification(
                direction = "Left",
                shouldDismissNotification = true,
                canExpandNotification = false,
            )
            verifySystemNotificationDoesNotExist("Firefox Fenix")
        }.closeNotificationTray {}
        deleteDownloadedFileOnStorage(downloadFile)
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/451563
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
            verifySystemNotificationExists("Download paused")
            clickDownloadNotificationControlButton("RESUME")
            clickDownloadNotificationControlButton("CANCEL")
            verifySystemNotificationDoesNotExist(downloadFile)
            mDevice.pressBack()
        }
        browserScreen {
        }.openThreeDotMenu {
        }.openDownloadsManager {
            verifyEmptyDownloadsList()
        }
        deleteDownloadedFileOnStorage(downloadFile)
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2301474
    @Test
    fun openDownloadedFileFromDownloadsMenuTest() {
        downloadFile = "web_icon.png"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(downloadTestPage.toUri()) {
            waitForPageToLoad()
        }.clickDownloadLink(downloadFile) {
            verifyDownloadPrompt(downloadFile)
        }.clickDownload {
            verifyDownloadCompleteNotificationPopup()
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1114970
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
            clickSnackbarButton("UNDO")
            verifyDownloadedFileName(downloadFile)
            deleteDownloadedItem(downloadFile)
            verifyEmptyDownloadsList()
        }
        deleteDownloadedFileOnStorage(downloadFile)
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2302662
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
            clickSnackbarButton("UNDO")
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2301537
    @Test
    fun fileDeletedFromStorageIsDeletedEverywhereTest() {
        val downloadFile = "smallZip.zip"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(downloadTestPage.toUri()) {
            waitForPageToLoad()
        }.clickDownloadLink(downloadFile) {
            verifyDownloadPrompt(downloadFile)
        }.clickDownload {
            verifyDownloadCompleteNotificationPopup()
        }
        browserScreen {
        }.openThreeDotMenu {
        }.openDownloadsManager {
            waitForDownloadsListToExist()
            verifyDownloadedFileName(downloadFile)
            deleteDownloadedFileOnStorage(downloadFile)
        }.exitDownloadsManagerToBrowser {
        }.openThreeDotMenu {
        }.openDownloadsManager {
            verifyEmptyDownloadsList()
            exitMenu()
        }

        navigationToolbar {
        }.enterURLAndEnterToBrowser(downloadTestPage.toUri()) {
            waitForPageToLoad()
        }.clickDownloadLink(downloadFile) {
            verifyDownloadPrompt(downloadFile)
        }.clickDownload {
            verifyDownloadCompleteNotificationPopup()
        }
        browserScreen {
        }.openThreeDotMenu {
        }.openDownloadsManager {
            waitForDownloadsListToExist()
            verifyDownloadedFileName(downloadFile)
        }
        deleteDownloadedFileOnStorage(downloadFile)
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/457112
    @Ignore("Failing: https://bugzilla.mozilla.org/show_bug.cgi?id=1840994")
    @Test
    fun systemNotificationCantBeDismissedWhileInProgressTest() {
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
            swipeDownloadNotification(direction = "Left", shouldDismissNotification = false)
            clickDownloadNotificationControlButton("PAUSE")
            swipeDownloadNotification(direction = "Right", shouldDismissNotification = false)
            clickDownloadNotificationControlButton("CANCEL")
        }
        deleteDownloadedFileOnStorage(downloadFile)
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2299297
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
        }.clickDownload {}

        setNetworkEnabled(enabled = false)

        browserScreen {
        }.openNotificationShade {
            verifySystemNotificationExists("Download failed")
            expandNotificationMessage()
            swipeDownloadNotification("Left", true)
            verifySystemNotificationDoesNotExist("Firefox Fenix")
        }.closeNotificationTray {}

        downloadRobot {
        }.closeDownloadPrompt {
            verifyDownloadPromptIsDismissed()
        }
        deleteDownloadedFileOnStorage(downloadFile)
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1632384
    @Test
    fun warningWhenClosingPrivateTabsWhileDownloadingTest() {
        downloadFile = "1GB.zip"

        // Clear the "Firefox Fenix default browser notification"
        notificationShade {
            cancelAllShownNotifications()
        }

        homeScreen {
        }.togglePrivateBrowsingMode()
        navigationToolbar {
        }.enterURLAndEnterToBrowser(downloadTestPage.toUri()) {
            waitForPageToLoad()
        }.clickDownloadLink(downloadFile) {
            verifyDownloadPrompt(downloadFile)
        }.clickDownload {}
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2302663
    @Test
    fun cancelActivePrivateBrowsingDownloadsTest() {
        downloadFile = "1GB.zip"

        // Clear the "Firefox Fenix default browser notification"
        notificationShade {
            cancelAllShownNotifications()
        }

        homeScreen {
        }.togglePrivateBrowsingMode()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(downloadTestPage.toUri()) {
            waitForPageToLoad()
        }.clickDownloadLink(downloadFile) {
            verifyDownloadPrompt(downloadFile)
        }.clickDownload {}
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2048448
    // Save edited PDF file from the share overlay
    @SmokeTest
    @Test
    fun saveAsPdfFunctionalityTest() {
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
