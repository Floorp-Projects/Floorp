/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.privacy

import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.FeatureSettingsHelper
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.MockWebServerHelper
import org.mozilla.focus.helpers.TestAssetHelper.getStorageTestAsset
import org.mozilla.focus.testAnnotations.SmokeTest
import java.io.IOException

/**
 * Make sure that session storage values are kept and written but removed at the end of a session.
 */
@RunWith(AndroidJUnit4ClassRunner::class)
class LocalSessionStorageTest {
    private lateinit var webServer: MockWebServer

    private val featureSettingsHelper = FeatureSettingsHelper()

    companion object {
        const val SESSION_STORAGE_HIT = "Session storage has value"
        const val LOCAL_STORAGE_MISS = "Local storage empty"
    }

    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @Before
    fun setUp() {
        webServer = MockWebServer().apply {
            dispatcher = MockWebServerHelper.AndroidAssetDispatcher()
            start()
        }
        featureSettingsHelper.setCfrForTrackingProtectionEnabled(false)
    }

    @After
    fun tearDown() {
        try {
            webServer.shutdown()
        } catch (e: IOException) {
            throw AssertionError("Could not stop web server", e)
        }
    }

    @SmokeTest
    @Test
    fun testLocalAndSessionStorageIsWrittenAndRemoved() {
        val storageStartUrl = getStorageTestAsset(webServer, "storage_start.html").url
        val storageCheckUrl = getStorageTestAsset(webServer, "storage_check.html").url

        searchScreen {
        }.loadPage(storageStartUrl) {
            // Assert website is loaded and values are written.
            verifyPageContent("Values written to storage")
        }.openSearchBar {
            // Now load the next website and assert that the values are still in the storage
        }.loadPage(storageCheckUrl) {
            verifyPageContent(SESSION_STORAGE_HIT)
            verifyPageContent(LOCAL_STORAGE_MISS)
        }.clearBrowsingData {}
        searchScreen {
        }.loadPage(storageCheckUrl) {
            verifyPageContent("Session storage empty")
            verifyPageContent("Local storage empty")
        }
    }

    @SmokeTest
    @Test
    fun eraseCookiesTest() {
        val storageStartUrl = getStorageTestAsset(webServer, "storage_start.html").url

        searchScreen {
        }.loadPage(storageStartUrl) {
            verifyPageContent("No cookies set")
            clickSetCookiesButton()
            verifyPageContent("user=android")
        }.clearBrowsingData {}
        searchScreen {
        }.loadPage(storageStartUrl) {
            verifyPageContent("No cookies set")
        }
    }
}
