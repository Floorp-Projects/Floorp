/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.focus.R
import org.mozilla.focus.activity.robots.homeScreen
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper.createMockResponseFromAsset
import org.mozilla.focus.helpers.TestHelper.exitToTop
import org.mozilla.focus.helpers.TestHelper.getStringResource
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.testAnnotations.SmokeTest

// These tests verify the Safe Browsing feature by visiting unsafe URLs and checking they are blocked
class SafeBrowsingTest {
    private lateinit var webServer: MockWebServer
    private val malwareWarning = getStringResource(R.string.mozac_browser_errorpages_safe_browsing_malware_uri_title)
    private val phishingWarning = getStringResource(R.string.mozac_browser_errorpages_safe_phishing_uri_title)
    private val unwantedSoftwareWarning =
            getStringResource(R.string.mozac_browser_errorpages_safe_browsing_unwanted_uri_title)
    private val harmfulSiteWarning = getStringResource(R.string.mozac_browser_errorpages_safe_harmful_uri_title)
    private val tryAgainButton = getStringResource(R.string.mozac_browser_errorpages_page_refresh)

    @get: Rule
    val mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @Before
    fun setUp() {
        webServer = MockWebServer()
        webServer.start()
    }

    @After
    fun tearDown() {
        webServer.shutdown()
    }

    @SmokeTest
    @Test
    fun blockMalwarePageTest() {
        val malwareURl = "http://itisatrap.org/firefox/its-an-attack.html"

        searchScreen {
        }.loadPage(malwareURl) {
            verifyPageContent(malwareWarning)
            verifyPageContent(tryAgainButton)
        }
    }

    @SmokeTest
    @Test
    fun blockPhishingPageTest() {
        val phishingURl = "http://itisatrap.org/firefox/its-a-trap.html"

        searchScreen {
        }.loadPage(phishingURl) {
            verifyPageContent(phishingWarning)
            verifyPageContent(tryAgainButton)
        }
    }

    @SmokeTest
    @Test
    fun blockUnwantedSoftwarePageTest() {
        val unwantedURl = "http://itisatrap.org/firefox/unwanted.html"

        searchScreen {
        }.loadPage(unwantedURl) {
            verifyPageContent(unwantedSoftwareWarning)
            verifyPageContent(tryAgainButton)
        }
    }

    @SmokeTest
    @Test
    fun blockHarmfulPageTest() {
        val harmfulURl = "https://www.itisatrap.org/firefox/harmful.html"

        searchScreen {
        }.loadPage(harmfulURl) {
            verifyPageContent(harmfulSiteWarning)
            verifyPageContent(tryAgainButton)
        }
    }

    @SmokeTest
    @Test
    fun unblockSafeBrowsingTest() {
        val malwareURl = "http://itisatrap.org/firefox/its-an-attack.html"

        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openPrivacySettingsMenu {
            switchSafeBrowsingToggle()
            exitToTop()
        }
        searchScreen {
        }.loadPage(malwareURl) {
            verifyPageContent("It’s an Attack!")
        }
    }

    @SmokeTest
    @Test
    fun verifyPageSecurityIconAndInfo() {
        val safePageUrl = "https://rpappalax.github.io/testapp"
        val insecurePageUrl = "http://itisatrap.org/firefox/its-a-trap.html"

        searchScreen {
        }.loadPage(safePageUrl) {
            verifyPageContent("Privacy & Security")
            verifySiteSecurityIconShown()
            verifySiteConnectionInfoIsSecure(true)
            mDevice.pressBack()
        }.openSearchBar {
        }.loadPage(insecurePageUrl) {
            verifyPageURL(insecurePageUrl)
            verifySiteSecurityIconShown()
            verifySiteConnectionInfoIsSecure(false)
            mDevice.pressBack()
        }
    }

    @Test
    fun testLocationSharingNotAllowed() {
        webServer.enqueue(createMockResponseFromAsset("permissionsPage.html"))
        val permissionsPage = webServer.url("permissionsPage.html").toString()

        searchScreen {
        }.loadPage(permissionsPage) {
            clickGetLocationButton()
            verifyPageContent("No location info")
        }
    }
}
