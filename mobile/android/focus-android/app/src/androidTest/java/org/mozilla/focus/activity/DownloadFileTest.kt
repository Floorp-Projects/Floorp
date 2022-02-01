/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.rule.GrantPermissionRule
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.activity.robots.downloadRobot
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.DeleteFilesHelper.deleteFileUsingDisplayName
import org.mozilla.focus.helpers.FeatureSettingsHelper
import org.mozilla.focus.helpers.MainActivityIntentsTestRule
import org.mozilla.focus.helpers.TestHelper
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.readTestAsset
import org.mozilla.focus.helpers.TestHelper.verifySnackBarText
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.testAnnotations.SmokeTest
import java.io.IOException

@RunWith(AndroidJUnit4ClassRunner::class)
class DownloadFileTest {
    private lateinit var webServer: MockWebServer
    private val featureSettingsHelper = FeatureSettingsHelper()

    @get:Rule
    var mActivityTestRule = MainActivityIntentsTestRule(showFirstRun = false)

    @get:Rule
    var mGrantPermissions = GrantPermissionRule.grant(
        android.Manifest.permission.WRITE_EXTERNAL_STORAGE,
        android.Manifest.permission.READ_EXTERNAL_STORAGE
    )

    @Before
    fun setUp() {
        featureSettingsHelper.setShieldIconCFREnabled(false)
        webServer = MockWebServer()
        try {
            webServer.enqueue(
                MockResponse()
                    .setBody(readTestAsset("image_test.html"))
                    .addHeader(
                        "Set-Cookie",
                        "sphere=battery; Expires=Wed, 21 Oct 2035 07:28:00 GMT;"
                    )
            )
            webServer.enqueue(
                MockResponse()
                    .setBody(readTestAsset("download.jpg"))
            )
            webServer.enqueue(
                MockResponse()
                    .setBody(readTestAsset("download.jpg"))
            )
            webServer.enqueue(
                MockResponse()
                    .setBody(readTestAsset("download.jpg"))
            )
            webServer.start()
        } catch (e: IOException) {
            throw AssertionError("Could not start web server", e)
        }
    }

    @After
    fun tearDown() {
        try {
            webServer.close()
            webServer.shutdown()
        } catch (e: IOException) {
            throw AssertionError("Could not stop web server", e)
        }
        val context = InstrumentationRegistry.getInstrumentation().targetContext.applicationContext
        deleteFileUsingDisplayName(context, "download.jpg")
        featureSettingsHelper.resetAllFeatureFlags()
    }

    @SmokeTest
    @Test
    fun downloadNotificationTest() {
        val downloadPageUrl = webServer.url("").toString()
        val downloadFileName = "download.jpg"

        // Load website with service worker
        searchScreen {
        }.loadPage(downloadPageUrl) { }

        downloadRobot {
            clickDownloadIconAsset()
            // If permission dialog appears, grant it
            if (TestHelper.permAllowBtn.waitForExists(waitingTime)) {
                TestHelper.permAllowBtn.click()
            }
            verifyDownloadDialog(downloadFileName)
            clickDownloadButton()
            verifySnackBarText("finished")
            mDevice.openNotification()
            verifyDownloadNotification()
        }
    }

    @SmokeTest
    @Test
    fun cancelDownloadTest() {
        val downloadPageUrl = webServer.url("").toString()

        searchScreen {
        }.loadPage(downloadPageUrl) { }

        downloadRobot {
            clickDownloadIconAsset()
            // If permission dialog appears, grant it
            if (TestHelper.permAllowBtn.waitForExists(waitingTime)) {
                TestHelper.permAllowBtn.click()
            }
            clickCancelDownloadButton()
            verifyDownloadDialogGone()
        }
    }

    @SmokeTest
    @Test
    fun openDownloadFileTest() {
        val downloadPageUrl = webServer.url("").toString()
        val downloadFileName = "download.jpg"

        // Load website with service worker
        searchScreen {
        }.loadPage(downloadPageUrl) { }

        downloadRobot {
            clickDownloadIconAsset()
            // If permission dialog appears, grant it
            if (TestHelper.permAllowBtn.waitForExists(waitingTime)) {
                TestHelper.permAllowBtn.click()
            }
            verifyDownloadDialog(downloadFileName)
            clickDownloadButton()
            verifySnackBarText("finished")
            openDownloadedFile()
            verifyPhotosOpens()
        }
    }
}
