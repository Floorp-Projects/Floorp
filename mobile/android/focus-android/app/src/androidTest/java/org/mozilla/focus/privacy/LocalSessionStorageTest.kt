/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.privacy

import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper.readTestAsset
import org.mozilla.focus.testAnnotations.SmokeTest
import java.io.IOException

/**
 * Make sure that session storage values are kept and written but removed at the end of a session.
 */
@RunWith(AndroidJUnit4ClassRunner::class)
class LocalSessionStorageTest {
    private lateinit var webServer: MockWebServer

    companion object {
        const val SESSION_STORAGE_HIT = "Session storage has value"
        const val LOCAL_STORAGE_MISS = "Local storage empty"
    }

    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @Before
    fun setUp() {
        webServer = MockWebServer()
        try {
            webServer.enqueue(
                MockResponse()
                    .setBody(readTestAsset("storage_start.html"))
            )
            webServer.enqueue(
                MockResponse()
                    .setBody(readTestAsset("storage_check.html"))
            )
            webServer.enqueue(
                MockResponse()
                    .setBody(readTestAsset("storage_check.html"))
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
    }

    @SmokeTest
    @Test
    fun testLocalAndSessionStorageIsWrittenAndRemoved() {
        val storageStartUrl = webServer.url("/sessionStorage_start").toString()
        val storageCheckUrl = webServer.url("/sessionStorage_check").toString()

        // Go to first website that saves a value in the session/local store.
        searchScreen {
        }.loadPage(storageStartUrl) {
            // Assert website is loaded and values are written.
            verifyPageContent("Values written to storage")
        }.openSearchBar {
            // Now load the next website and assert that the values are still in the storage
        }.loadPage(storageCheckUrl) {
            verifyPageContent(SESSION_STORAGE_HIT)
            verifyPageContent(LOCAL_STORAGE_MISS)
            // Press the "erase" button and end the session
        }.clearBrowsingData {}
        // Go to the website again and make sure that both values are not in the session/local store
        searchScreen {
        }.loadPage(storageCheckUrl) {
            // Session storage is empty
            verifyPageContent("Session storage empty")
            // Local storage is empty
            verifyPageContent("Local storage empty")
        }
    }
}
