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
import org.mozilla.focus.activity.robots.homeScreen
import org.mozilla.focus.helpers.FeatureSettingsHelper
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.RetryTestRule
import org.mozilla.focus.helpers.TestHelper.createMockResponseFromAsset
import org.mozilla.focus.helpers.TestHelper.waitingTimeShort
import org.mozilla.focus.testAnnotations.SmokeTest

// These tests check the advanced settings options
@RunWith(AndroidJUnit4ClassRunner::class)
class SettingsAdvancedTest {
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
        webServer.enqueue(createMockResponseFromAsset("tab3.html"))
        webServer.enqueue(createMockResponseFromAsset("tab3.html"))
        featureSettingsHelper.setCfrForTrackingProtectionEnabled(false)
        featureSettingsHelper.setNumberOfTabsOpened(4)
    }

    @After
    fun tearDown() {
        webServer.shutdown()
        featureSettingsHelper.resetAllFeatureFlags()
    }

    @SmokeTest
    @Test
    fun openLinksInAppsTest() {
        val tab3Url = webServer.url("tab3.html").toString()
        val youtubeLink = "https://www.youtube.com/c/MozillaChannel/videos"

        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openAdvancedSettingsMenu {
            verifyOpenLinksInAppsSwitchState(false)
            clickOpenLinksInAppsSwitch()
            verifyOpenLinksInAppsSwitchState(true)
        }.goBackToSettings {
        }.goBackToHomeScreen {
        }.loadPage(tab3Url) {
            progressBar.waitUntilGone(waitingTimeShort)
            clickLinkMatchingText("Mozilla Youtube link")
            verifyOpenLinksInAppsPrompt(true, youtubeLink)
            clickOpenLinksInAppsCancelButton()
        }.clearBrowsingData {
        }.openMainMenu {
        }.openSettings {
        }.openAdvancedSettingsMenu {
            verifyOpenLinksInAppsSwitchState(true)
            clickOpenLinksInAppsSwitch()
            verifyOpenLinksInAppsSwitchState(false)
        }.goBackToSettings {
        }.goBackToHomeScreen {
        }.loadPage(tab3Url) {
            progressBar.waitUntilGone(waitingTimeShort)
            clickLinkMatchingText("Mozilla Youtube link")
            verifyOpenLinksInAppsPrompt(false, youtubeLink)
        }
    }
}
