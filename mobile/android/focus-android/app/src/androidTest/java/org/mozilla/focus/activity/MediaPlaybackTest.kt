/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.FeatureSettingsHelper
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper.createMockResponseFromAsset
import org.mozilla.focus.testAnnotations.SmokeTest

class MediaPlaybackTest {
    private lateinit var webServer: MockWebServer
    private val featureSettingsHelper = FeatureSettingsHelper()

    @get:Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @Before
    fun setUp() {
        featureSettingsHelper.setShieldIconCFREnabled(false)
        featureSettingsHelper.setNumberOfTabsOpened(4)
        webServer = MockWebServer()
        webServer.start()
    }

    @After
    fun tearDown() {
        webServer.shutdown()
        featureSettingsHelper.resetAllFeatureFlags()
    }

    @SmokeTest
    @Test
    fun testVideoPlayback() {
        webServer.enqueue(createMockResponseFromAsset("videoPage.html"))
        webServer.enqueue(createMockResponseFromAsset("resources/videoSample.webm"))
        val videoPageUrl = webServer.url("videoPage.html").toString()

        searchScreen {
        }.loadPage(videoPageUrl) {
            clickPlayButton()
            waitForPlaybackToStart()
            // need this alert hack to check the video is playing,
            // currently the test cannot verify the text in the page
            dismissMediaPlayingAlert()
            clickPauseButton()
            verifyPlaybackStopped()
            dismissMediaPlayingAlert()
        }
    }

    @SmokeTest
    @Test
    fun testAudioPlayback() {
        webServer.enqueue(createMockResponseFromAsset("audioPage.html"))
        webServer.enqueue(createMockResponseFromAsset("resources/audioSample.mp3"))
        val audioPageUrl = webServer.url("audioPage.html").toString()

        searchScreen {
        }.loadPage(audioPageUrl) {
            clickPlayButton()
            waitForPlaybackToStart()
            // need this alert hack to check the video is playing,
            // currently the test cannot verify the text in the page
            dismissMediaPlayingAlert()
            clickPauseButton()
            verifyPlaybackStopped()
            dismissMediaPlayingAlert()
        }
    }
}
