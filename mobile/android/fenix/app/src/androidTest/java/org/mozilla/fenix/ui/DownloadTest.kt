/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.rule.GrantPermissionRule
import androidx.test.uiautomator.UiDevice
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestAssetHelper.downloadFileName
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.ui.robots.downloadRobot
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.robots.notificationShade

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
            dispatcher = AndroidAssetDispatcher()
            start()
        }
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()

        TestHelper.deleteDownloadFromStorage(downloadFileName)
    }

    @Test
    fun testDownloadPrompt() {
        val defaultWebPage = TestAssetHelper.getDownloadAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            mDevice.waitForIdle()
        }

        downloadRobot {
            verifyDownloadPrompt()
        }.closePrompt {}
    }

    @Test
    fun testDownloadNotification() {
        val defaultWebPage = TestAssetHelper.getDownloadAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            mDevice.waitForIdle()
        }

        downloadRobot {
            verifyDownloadPrompt()
        }.clickDownload {
            verifyDownloadNotificationPopup()
        }

        mDevice.openNotification()
        notificationShade {
            verifySystemNotificationExists("Download completed")
        }
        // close notification shade before the next test
        mDevice.pressBack()
    }
}
