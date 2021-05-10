/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import org.junit.Rule
import org.junit.Test
import org.mozilla.focus.R
import org.mozilla.focus.activity.robots.homeScreen
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper.exitToTop
import org.mozilla.focus.helpers.TestHelper.getStringResource

// These tests verify the Safe Browsing feature by visiting unsafe URLs and checking they are blocked
class SafeBrowsingTest {
    private val malwareWarning = getStringResource(R.string.mozac_browser_errorpages_safe_browsing_malware_uri_title)
    private val phishingWarning = getStringResource(R.string.mozac_browser_errorpages_safe_phishing_uri_title)
    private val unwantedSoftwareWarning =
            getStringResource(R.string.mozac_browser_errorpages_safe_browsing_unwanted_uri_title)
    private val harmfulSiteWarning = getStringResource(R.string.mozac_browser_errorpages_safe_harmful_uri_title)
    private val tryAgainButton = getStringResource(R.string.errorpage_refresh)

    @get: Rule
    val mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @Test
    fun blockMalwarePageTest() {
        val malwareURl = "http://itisatrap.org/firefox/its-an-attack.html"

        searchScreen {
        }.loadPage(malwareURl) {
            verifyPageContent(malwareWarning)
            verifyPageContent(tryAgainButton)
        }
    }

    @Test
    fun blockPhishingPageTest() {
        val phishingURl = "http://itisatrap.org/firefox/its-a-trap.html"

        searchScreen {
        }.loadPage(phishingURl) {
            verifyPageContent(phishingWarning)
            verifyPageContent(tryAgainButton)
        }
    }

    @Test
    fun blockUnwantedSoftwarePageTest() {
        val unwantedURl = "http://itisatrap.org/firefox/unwanted.html"

        searchScreen {
        }.loadPage(unwantedURl) {
            verifyPageContent(unwantedSoftwareWarning)
            verifyPageContent(tryAgainButton)
        }
    }

    @Test
    fun blockHarmfulPageTest() {
        val harmfulURl = "https://www.itisatrap.org/firefox/harmful.html"

        searchScreen {
        }.loadPage(harmfulURl) {
            verifyPageContent(harmfulSiteWarning)
            verifyPageContent(tryAgainButton)
        }
    }

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
}
