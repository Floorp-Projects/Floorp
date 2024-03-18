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
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.FeatureSettingsHelper
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.MockWebServerHelper
import org.mozilla.focus.helpers.RetryTestRule
import org.mozilla.focus.helpers.TestAssetHelper.getStorageTestAsset
import org.mozilla.focus.helpers.TestHelper.exitToTop
import org.mozilla.focus.helpers.TestHelper.progressBar
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.testAnnotations.SmokeTest

// These tests the Privacy and Security settings menus and options
@RunWith(AndroidJUnit4ClassRunner::class)
class SettingsPrivacyTest {
    private val featureSettingsHelper = FeatureSettingsHelper()
    private lateinit var webServer: MockWebServer

    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @Rule
    @JvmField
    val retryTestRule = RetryTestRule(3)

    @Before
    fun setup() {
        featureSettingsHelper.setCfrForTrackingProtectionEnabled(false)
        featureSettingsHelper.setSearchWidgetDialogEnabled(false)
        webServer = MockWebServer().apply {
            dispatcher = MockWebServerHelper.AndroidAssetDispatcher()
            start()
        }
    }

    @After
    fun tearDown() {
        featureSettingsHelper.resetAllFeatureFlags()
        webServer.shutdown()
    }

    @SmokeTest
    @Test
    fun verifyCookiesAndSiteDataItemsTest() {
        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openPrivacySettingsMenu {
            verifyCookiesAndSiteDataSection()
            clickBlockCookies()
            verifyBlockCookiesPrompt()
            clickCancelBlockCookiesPrompt()
        }
    }

    @SmokeTest
    @Test
    fun verifyAllCookiesBlockedTest() {
        val sameSiteCookiesUrl = getStorageTestAsset(webServer, "same-site-cookies.html").url
        val thirdPartyCookiesUrl = getStorageTestAsset(webServer, "cross-site-cookies.html").url

        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openPrivacySettingsMenu {
            clickBlockCookies()
            clickYesPleaseOption()
            exitToTop()
        }
        searchScreen {
        }.loadPage(sameSiteCookiesUrl) {
            progressBar.waitUntilGone(waitingTime)
            verifyCookiesEnabled("BLOCKED")
        }.clearBrowsingData {
        }.openSearchBar {
        }.loadPage(thirdPartyCookiesUrl) {
            progressBar.waitUntilGone(waitingTime)
            verifyCookiesEnabled("BLOCKED")
        }
    }

    @SmokeTest
    @Test
    fun verify3rdPartyCookiesBlockedTest() {
        val sameSiteCookiesUrl = getStorageTestAsset(webServer, "same-site-cookies.html").url
        val thirdPartyCookiesURL = getStorageTestAsset(webServer, "cross-site-cookies.html").url

        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openPrivacySettingsMenu {
            clickBlockCookies()
            clickBlockThirdPartyCookiesOnly()
        }.goBackToSettings {
        }.goBackToHomeScreen {
        }.loadPage(thirdPartyCookiesURL) {
            progressBar.waitUntilGone(waitingTime)
            verifyCookiesEnabled("BLOCKED")
        }.clearBrowsingData {
        }.openSearchBar {
        }.loadPage(sameSiteCookiesUrl) {
            progressBar.waitUntilGone(waitingTime)
            verifyCookiesEnabled("UNRESTRICTED")
        }
    }

    @Test
    fun verifyCrossSiteCookiesBlockedTest() {
        val sameSiteCookiesUrl = getStorageTestAsset(webServer, "same-site-cookies.html").url
        val crossSiteCookiesURL = getStorageTestAsset(webServer, "cross-site-cookies.html").url

        searchScreen {
        }.loadPage(crossSiteCookiesURL) {
            progressBar.waitUntilGone(waitingTime)
            verifyCookiesEnabled("PARTITIONED")
        }.clearBrowsingData {
        }.openSearchBar {
        }.loadPage(sameSiteCookiesUrl) {
            progressBar.waitUntilGone(waitingTime)
            verifyCookiesEnabled("UNRESTRICTED")
        }
    }
}
