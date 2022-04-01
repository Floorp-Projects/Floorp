/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.activity.robots.downloadRobot
import org.mozilla.focus.activity.robots.notificationTray
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.DeleteFilesHelper.deleteFileUsingDisplayName
import org.mozilla.focus.helpers.FeatureSettingsHelper
import org.mozilla.focus.helpers.MainActivityIntentsTestRule
import org.mozilla.focus.helpers.MockWebServerHelper
import org.mozilla.focus.helpers.RetryTestRule
import org.mozilla.focus.helpers.StringsHelper.GOOGLE_DRIVE
import org.mozilla.focus.helpers.StringsHelper.GOOGLE_PHOTOS
import org.mozilla.focus.helpers.TestAssetHelper.getImageTestAsset
import org.mozilla.focus.helpers.TestHelper.assertNativeAppOpens
import org.mozilla.focus.helpers.TestHelper.getTargetContext
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.permAllowBtn
import org.mozilla.focus.helpers.TestHelper.verifySnackBarText
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.testAnnotations.SmokeTest
import java.io.IOException

@RunWith(AndroidJUnit4ClassRunner::class)
class DownloadFileTest {
    private lateinit var webServer: MockWebServer
    private val featureSettingsHelper = FeatureSettingsHelper()
    private val downloadTestPage = "https://storage.googleapis.com/mobile_test_assets/test_app/downloads.html"
    private var downloadFileName: String = ""

    @get:Rule
    var mActivityTestRule = MainActivityIntentsTestRule(showFirstRun = false)

    @Rule
    @JvmField
    val retryTestRule = RetryTestRule(3)

    @Before
    fun setUp() {
        featureSettingsHelper.setCfrForTrackingProtectionEnabled(false)
        featureSettingsHelper.setNumberOfTabsOpened(4)
        webServer = MockWebServer().apply {
            dispatcher = MockWebServerHelper.AndroidAssetDispatcher()
            start()
        }
    }

    @After
    fun tearDown() {
        try {
            webServer.shutdown()
        } catch (e: IOException) {
            throw AssertionError("Could not stop web server", e)
        }
        deleteFileUsingDisplayName(getTargetContext.applicationContext, downloadFileName)
        featureSettingsHelper.resetAllFeatureFlags()
    }

    @SmokeTest
    @Test
    fun downloadNotificationTest() {
        val downloadPageUrl = getImageTestAsset(webServer).url
        downloadFileName = "download.jpg"

        notificationTray {
            mDevice.openNotification()
            clearNotifications()
        }

        // Load website with service worker
        searchScreen {
        }.loadPage(downloadPageUrl) { }

        downloadRobot {
            clickDownloadIconAsset()
            // If permission dialog appears, grant it
            if (permAllowBtn.waitForExists(waitingTime)) {
                permAllowBtn.click()
            }
            verifyDownloadDialog(downloadFileName)
            clickDownloadButton()
            verifySnackBarText("finished")
            mDevice.openNotification()
            notificationTray {
                verifyDownloadNotification("Download completed", downloadFileName)
            }
        }
    }

    @SmokeTest
    @Test
    fun cancelDownloadTest() {
        val downloadPageUrl = getImageTestAsset(webServer).url

        searchScreen {
        }.loadPage(downloadPageUrl) { }

        downloadRobot {
            clickDownloadIconAsset()
            // If permission dialog appears, grant it
            if (permAllowBtn.waitForExists(waitingTime)) {
                permAllowBtn.click()
            }
            clickCancelDownloadButton()
            verifyDownloadDialogGone()
        }
    }

    @SmokeTest
    @Test
    fun downloadAndOpenJpgFileTest() {
        val downloadPageUrl = getImageTestAsset(webServer).url
        downloadFileName = "download.jpg"

        // Load website with service worker
        searchScreen {
        }.loadPage(downloadPageUrl) { }

        downloadRobot {
            clickDownloadIconAsset()
            // If permission dialog appears on devices with API<30, grant it
            if (permAllowBtn.waitForExists(waitingTime)) {
                permAllowBtn.click()
            }
            verifyDownloadDialog(downloadFileName)
            clickDownloadButton()
            verifyDownloadConfirmationMessage(downloadFileName)
            openDownloadedFile()
            assertNativeAppOpens(GOOGLE_PHOTOS)
        }
    }

    @SmokeTest
    @Test
    fun downloadAndOpenPdfFileTest() {
        downloadFileName = "washington.pdf"

        searchScreen {
        }.loadPage(downloadTestPage) {
            progressBar.waitUntilGone(waitingTime)
            clickLinkMatchingText(downloadFileName)
        }
        // If permission dialog appears on devices with API<30, grant it
        if (permAllowBtn.waitForExists(waitingTime)) {
            permAllowBtn.click()
        }
        downloadRobot {
            verifyDownloadDialog(downloadFileName)
            clickDownloadButton()
            verifyDownloadConfirmationMessage(downloadFileName)
            openDownloadedFile()
            assertNativeAppOpens(GOOGLE_DRIVE)
        }
    }

    @SmokeTest
    @Test
    fun downloadAndOpenWebmFileTest() {
        downloadFileName = "videoSample.webm"

        searchScreen {
        }.loadPage(downloadTestPage) {
            progressBar.waitUntilGone(waitingTime)
            clickLinkMatchingText(downloadFileName)
        }
        // If permission dialog appears on devices with API<30, grant it
        if (permAllowBtn.waitForExists(waitingTime)) {
            permAllowBtn.click()
        }
        downloadRobot {
            verifyDownloadDialog(downloadFileName)
            clickDownloadButton()
            verifyDownloadConfirmationMessage(downloadFileName)
            openDownloadedFile()
            assertNativeAppOpens(GOOGLE_PHOTOS)
        }
    }
}
