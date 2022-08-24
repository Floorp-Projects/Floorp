/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.test.espresso.Espresso.pressBack
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.activity.robots.browserScreen
import org.mozilla.focus.activity.robots.homeScreen
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.FeatureSettingsHelper
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.MockWebServerHelper
import org.mozilla.focus.helpers.RetryTestRule
import org.mozilla.focus.helpers.TestAssetHelper.getEnhancedTrackingProtectionAsset
import org.mozilla.focus.helpers.TestAssetHelper.getGenericAsset
import org.mozilla.focus.helpers.TestHelper.exitToBrowser
import org.mozilla.focus.helpers.TestHelper.exitToTop
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.testAnnotations.SmokeTest
import java.io.IOException

@RunWith(AndroidJUnit4ClassRunner::class)
class EnhancedTrackingProtectionSettingsTest {
    private lateinit var webServer: MockWebServer
    private val featureSettingsHelper = FeatureSettingsHelper()

    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @Rule
    @JvmField
    val retryTestRule = RetryTestRule(3)

    @Before
    fun setUp() {
        featureSettingsHelper.setCfrForTrackingProtectionEnabled(false)
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
        featureSettingsHelper.resetAllFeatureFlags()
    }

    @SmokeTest
    @Test
    fun trackingProtectionTogglesListTest() {
        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openPrivacySettingsMenu {
            verifyBlockAdTrackersEnabled(true)
            verifyBlockAnalyticTrackersEnabled(true)
            verifyBlockSocialTrackersEnabled(true)
            verifyBlockOtherTrackersEnabled(false)
        }
    }

    @SmokeTest
    @Test
    fun blockAdTrackersTest() {
        val genericPage = getGenericAsset(webServer)
        val trackingPage = getEnhancedTrackingProtectionAsset(webServer, "adsTrackers")

        searchScreen {
        }.loadPage(genericPage.url) {
            // loading a generic page to allow GV to fully load on first run
            verifyPageContent(genericPage.content)
            pressBack()
        }
        searchScreen {
        }.loadPage(trackingPage.url) {
            verifyTrackingProtectionAlert("ads trackers blocked")
        }
    }

    @SmokeTest
    @Test
    fun allowAdTrackersTest() {
        val genericPage = getGenericAsset(webServer)
        val trackingPage = getEnhancedTrackingProtectionAsset(webServer, "adsTrackers")

        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openPrivacySettingsMenu {
            clickAdTrackersBlockSwitch()
            verifyBlockAdTrackersEnabled(false)
            exitToTop()
        }
        searchScreen {
        }.loadPage(genericPage.url) {
            // loading a generic page to allow GV to fully load on first run
            verifyPageContent(genericPage.content)
            pressBack()
        }
        searchScreen {
        }.loadPage(trackingPage.url) {
            verifyPageContent("ads trackers not blocked")
        }
    }

    @Ignore("Failing , see https://github.com/mozilla-mobile/focus-android/issues/6812")
    @SmokeTest
    @Test
    fun blockAnalyticsTrackersTest() {
        val genericPage = getGenericAsset(webServer)
        val trackingPage = getEnhancedTrackingProtectionAsset(webServer, "analyticsTrackers")

        searchScreen {
        }.loadPage(genericPage.url) {
            // loading a generic page to allow GV to fully load on first run
            verifyPageContent(genericPage.content)
            pressBack()
        }
        searchScreen {
        }.loadPage(trackingPage.url) {
            verifyTrackingProtectionAlert("analytics trackers blocked")
        }
    }

    @SmokeTest
    @Test
    fun allowAnalyticsTrackersTest() {
        val genericPage = getGenericAsset(webServer)
        val trackingPage = getEnhancedTrackingProtectionAsset(webServer, "analyticsTrackers")

        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openPrivacySettingsMenu {
            clickAnalyticsTrackersBlockSwitch()
            verifyBlockAnalyticTrackersEnabled(false)
            exitToTop()
        }
        searchScreen {
        }.loadPage(genericPage.url) {
            // loading a generic page to allow GV to fully load on first run
            verifyPageContent(genericPage.content)
            pressBack()
        }
        searchScreen {
        }.loadPage(trackingPage.url) {
            verifyPageContent("analytics trackers not blocked")
        }
    }

    @SmokeTest
    @Test
    fun blockSocialTrackersTest() {
        val genericPage = getGenericAsset(webServer)
        val trackingPage = getEnhancedTrackingProtectionAsset(webServer, "socialTrackers")

        searchScreen {
        }.loadPage(genericPage.url) {
            // loading a generic page to allow GV to fully load on first run
            verifyPageContent(genericPage.content)
            pressBack()
        }
        searchScreen {
        }.loadPage(trackingPage.url) {
            verifyTrackingProtectionAlert("social trackers blocked")
        }
    }

    @SmokeTest
    @Test
    fun allowSocialTrackersTest() {
        val genericPage = getGenericAsset(webServer)
        val trackingPage = getEnhancedTrackingProtectionAsset(webServer, "socialTrackers")

        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openPrivacySettingsMenu {
            clickSocialTrackersBlockSwitch()
            verifyBlockSocialTrackersEnabled(false)
            exitToTop()
        }
        searchScreen {
        }.loadPage(genericPage.url) {
            // loading a generic page to allow GV to fully load on first run
            verifyPageContent(genericPage.content)
            pressBack()
        }
        searchScreen {
        }.loadPage(trackingPage.url) {
            verifyPageContent("social trackers not blocked")
        }
    }

    @SmokeTest
    @Test
    fun allowOtherContentTrackersTest() {
        val genericPage = getGenericAsset(webServer)
        val trackingPage = getEnhancedTrackingProtectionAsset(webServer, "otherTrackers")

        searchScreen {
        }.loadPage(genericPage.url) {
            // loading a generic page to allow GV to fully load on first run
            verifyPageContent(genericPage.content)
            pressBack()
        }
        searchScreen {
        }.loadPage(trackingPage.url) {
            verifyPageContent("other content trackers not blocked")
        }
    }

    @SmokeTest
    @Test
    fun blockOtherContentTrackersTest() {
        val genericPage = getGenericAsset(webServer)
        val trackingPage = getEnhancedTrackingProtectionAsset(webServer, "otherTrackers")

        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openPrivacySettingsMenu {
            clickOtherContentTrackersBlockSwitch()
            verifyBlockOtherTrackersEnabled(true)
            exitToTop()
        }
        searchScreen {
        }.loadPage(genericPage.url) {
            // loading a generic page to allow GV to fully load on first run
            verifyPageContent(genericPage.content)
            pressBack()
        }
        searchScreen {
        }.loadPage(trackingPage.url) {
            verifyTrackingProtectionAlert("other content trackers blocked")
        }
    }

    @Ignore("Failing, see https://github.com/mozilla-mobile/focus-android/issues/6661")
    @SmokeTest
    @Test
    fun addURLToTPExceptionsListTest() {
        val genericPage = getGenericAsset(webServer)
        val trackingPage = getEnhancedTrackingProtectionAsset(webServer, "otherTrackers")

        searchScreen {
        }.loadPage(genericPage.url) {
            verifyPageContent(genericPage.content)
        }.openSearchBar {
        }.loadPage(trackingPage.url) {
            verifyPageContent(trackingPage.content)
        }.openSiteSecurityInfoSheet {
        }.clickTrackingProtectionSwitch {
            progressBar.waitUntilGone(waitingTime)
        }.openMainMenu {
        }.openSettings {
        }.openPrivacySettingsMenu {
            openExceptionsList()
            verifyExceptionURL(webServer.hostName)
        }
    }

    @Ignore("Failing, see https://github.com/mozilla-mobile/focus-android/issues/6425")
    @SmokeTest
    @Test
    fun removeOneExceptionURLTest() {
        val genericPage = getGenericAsset(webServer)
        val trackingPage = getEnhancedTrackingProtectionAsset(webServer, "otherTrackers")

        searchScreen {
        }.loadPage(genericPage.url) {
            verifyPageContent(genericPage.content)
        }.openSearchBar {
        }.loadPage(trackingPage.url) {
            verifyPageContent(trackingPage.content)
        }.openSiteSecurityInfoSheet {
        }.clickTrackingProtectionSwitch {
            progressBar.waitUntilGone(waitingTime)
        }.openMainMenu {
        }.openSettings {
        }.openPrivacySettingsMenu {
            openExceptionsList()
            removeException()
            verifyExceptionsListDisabled()
            exitToBrowser()
        }
        browserScreen {
        }.openSiteSecurityInfoSheet {
            verifyTrackingProtectionIsEnabled(true)
        }
    }

    @Ignore("Failing, see https://github.com/mozilla-mobile/focus-android/issues/6679")
    @SmokeTest
    @Test
    fun removeAllExceptionURLTest() {
        val genericPage = getGenericAsset(webServer)
        val trackingPage = getEnhancedTrackingProtectionAsset(webServer, "otherTrackers")

        searchScreen {
        }.loadPage(genericPage.url) {
            verifyPageContent(genericPage.content)
        }.openSearchBar {
        }.loadPage(trackingPage.url) {
            verifyPageContent(trackingPage.content)
        }.openSiteSecurityInfoSheet {
        }.clickTrackingProtectionSwitch {
            progressBar.waitUntilGone(waitingTime)
        }.openMainMenu {
        }.openSettings {
        }.openPrivacySettingsMenu {
            openExceptionsList()
            removeAllExceptions()
            verifyExceptionsListDisabled()
            exitToBrowser()
        }
        browserScreen {
        }.openSiteSecurityInfoSheet {
            verifyTrackingProtectionIsEnabled(true)
        }
    }
}
