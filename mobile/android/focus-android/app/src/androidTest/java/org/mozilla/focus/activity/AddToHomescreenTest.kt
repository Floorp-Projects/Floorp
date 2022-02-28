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
import org.mozilla.focus.helpers.RetryTestRule
import org.mozilla.focus.helpers.TestHelper.readTestAsset
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.testAnnotations.SmokeTest
import java.io.IOException

/**
 * Tests to verify the functionality of Add to homescreen from the main menu
 */
@RunWith(AndroidJUnit4ClassRunner::class)
class AddToHomescreenTest {
    private lateinit var webServer: MockWebServer
    private val featureSettingsHelper = FeatureSettingsHelper()

    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @Rule
    @JvmField
    val retryTestRule = RetryTestRule(3)

    @Before
    fun setup() {
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
        featureSettingsHelper.setCfrForTrackingProtectionEnabled(false)
        featureSettingsHelper.setNumberOfTabsOpened(4)
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
    fun addPageToHomeScreenTest() {
        val pageUrl = webServer.url("").toString()
        val pageTitle = "test1"

        // Open website, and click 'Add to homescreen'
        searchScreen {
        }.loadPage(pageUrl) {
            progressBar.waitUntilGone(waitingTime)
        }.openMainMenu {
        }.openAddToHSDialog {
            addShortcutWithTitle(pageTitle)
            handleAddAutomaticallyDialog()
        }.searchAndOpenHomeScreenShortcut(pageTitle) {
            verifyPageURL(pageUrl)
        }
    }

    @SmokeTest
    @Test
    fun noNameShortcutTest() {
        val pageUrl = webServer.url("").toString()

        // Open website, and click 'Add to homescreen'
        searchScreen {
        }.loadPage(pageUrl) {
        }.openMainMenu {
        }.openAddToHSDialog {
            // leave shortcut title empty and add it to HS
            addShortcutNoTitle()
            handleAddAutomaticallyDialog()
        }.searchAndOpenHomeScreenShortcut(webServer.hostName) {
            verifyPageURL(pageUrl)
        }
    }
}
