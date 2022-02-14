/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.FeatureSettingsHelper
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.readTestAsset
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.testAnnotations.SmokeTest
import java.io.IOException

// This test opens and verifies the share overlay
@RunWith(AndroidJUnit4ClassRunner::class)
class ShareWebsiteTest {
    private lateinit var webServer: MockWebServer
    private val featureSettingsHelper = FeatureSettingsHelper()

    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @Before
    fun setUp() {
        featureSettingsHelper.setShieldIconCFREnabled(false)
        webServer = MockWebServer()
        try {
            webServer.enqueue(
                MockResponse()
                    .setBody(readTestAsset("plain_test.html"))
            )
            webServer.start()
        } catch (e: IOException) {
            throw AssertionError("Could not start web server", e)
        }
    }

    @After
    fun tearDown() {
        try {
            webServer.shutdown()
        } catch (e: IOException) {
            throw AssertionError("Could not stop web server", e)
        }
        featureSettingsHelper.resetAllFeatureFlags()
    }

    @SmokeTest
    @Test
    fun shareTabTest() {
        /* Go to a webpage */
        searchScreen {
        }.loadPage(webServer.url("").toString()) {
            progressBar.waitUntilGone(waitingTime)
        }.openMainMenu {
        }.openShareScreen {
            verifyShareAppsListOpened()
            mDevice.pressBack()
        }
    }
}
