/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import android.os.Environment
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.rule.GrantPermissionRule
import androidx.test.uiautomator.UiDevice
import kotlinx.coroutines.runBlocking
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.ui.robots.downloadRobot
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar
import java.io.File

/**
 *  Tests for verifying basic functionality of download prompt UI
 *
 *  - Initiates a download
 *  - Verifies download prompt
 *  - Verifies download notification
 **/

class DownloadTest {

    private val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

    private lateinit var mockWebServer: MockWebServer

    /* ktlint-disable no-blank-line-before-rbrace */ // This imposes unreadable grouping.
    @get:Rule
    val activityTestRule = HomeActivityTestRule()

    @get:Rule
    var mGrantPermissions = GrantPermissionRule.grant(
        android.Manifest.permission.WRITE_EXTERNAL_STORAGE,
        android.Manifest.permission.READ_EXTERNAL_STORAGE
    )

    @Before
    fun setUp() {
        mockWebServer = MockWebServer().apply {
            setDispatcher(AndroidAssetDispatcher())
            start()
        }
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()

        // Clear Download
        runBlocking {
            val downloadedFile = File(
                Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS),
                "Globe.svg.html"
            )

            if (downloadedFile.exists()) {
                downloadedFile.delete()
            }
        }
    }

    @Test
    @Ignore("Temp disable flaky test - see: https://github.com/mozilla-mobile/fenix/issues/10798")
    fun testDownloadPrompt() {
        homeScreen { }.dismissOnboarding()

        val defaultWebPage = TestAssetHelper.getDownloadAsset(mockWebServer)

        navigationToolbar {
        }.openNewTabAndEnterToBrowser(defaultWebPage.url) {
            // verifyPageContent(defaultWebPage.content)
            clickLinkMatchingText(defaultWebPage.content)
        }

        downloadRobot {
            verifyDownloadPrompt()
        }.closePrompt {}
    }

    @Test
    @Ignore("Temp disable flakey test - see: https://github.com/mozilla-mobile/fenix/issues/5462")
    fun testDownloadNotification() {
        homeScreen { }.dismissOnboarding()

        val defaultWebPage = TestAssetHelper.getDownloadAsset(mockWebServer)

        navigationToolbar {
        }.openNewTabAndEnterToBrowser(defaultWebPage.url) {
            // verifyPageContent(defaultWebPage.content)
            clickLinkMatchingText(defaultWebPage.content)
        }

        downloadRobot {
            verifyDownloadPrompt()
        }.clickDownload {
            verifyDownloadNotificationPopup()
            verifyDownloadNotificationShade()
        }
    }
}
