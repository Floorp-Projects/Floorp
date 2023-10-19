/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.components.toolbar.CFR_MINIMUM_NUMBER_OPENED_TABS
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper.getGenericAsset
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying the new Cookie protection & homescreen feature hints.
 *  Note: This involves setting the feature flags On for CFRs which are disabled elsewhere.
 *
 */
class TotalCookieProtectionTest {
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val composeTestRule = AndroidComposeTestRule(
        HomeActivityTestRule(
            isTCPCFREnabled = true,
        ),
    ) { it.activity }

    @Before
    fun setUp() {
        CFR_MINIMUM_NUMBER_OPENED_TABS = 0
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
        CFR_MINIMUM_NUMBER_OPENED_TABS = 5
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2260552
    @Test
    fun openTotalCookieProtectionLearnMoreLinkTest() {
        val genericPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowserForTCPCFR(genericPage.url) {
            waitForPageToLoad()
            verifyCookiesProtectionHintIsDisplayed(composeTestRule, true)
            clickTCPCFRLearnMore(composeTestRule)
            verifyUrl("support.mozilla.org/en-US/kb/enhanced-tracking-protection-firefox-android")
            verifyShouldShowCFRTCP(false, composeTestRule.activity.settings())
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1913589
    @Test
    fun dismissTotalCookieProtectionHintTest() {
        val genericPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowserForTCPCFR(genericPage.url) {
            waitForPageToLoad()
            verifyCookiesProtectionHintIsDisplayed(composeTestRule, true)
            dismissTCPCFRPopup(composeTestRule)
            verifyCookiesProtectionHintIsDisplayed(composeTestRule, false)
            verifyShouldShowCFRTCP(false, composeTestRule.activity.settings())
        }
    }
}
