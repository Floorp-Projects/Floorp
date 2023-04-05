/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper.getGenericAsset
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying the new Cookie protection & homescreen feature hints.
 *  Note: This involves setting the feature flags On for CFRs which are disabled elsewhere.
 *
 */
class ContextualHintsTest {
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val activityTestRule = HomeActivityTestRule(
        isJumpBackInCFREnabled = true,
        isTCPCFREnabled = true,
        isPocketEnabled = false,
        isRecentlyVisitedFeatureEnabled = false,
        isCookieBannerReductionDialogEnabled = false,
    )

    @Before
    fun setUp() {
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    @Test
    fun jumpBackInCFRTest() {
        val genericPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericPage.url) {
            verifyCookiesProtectionHintIsDisplayed(true)
            // One back press to dismiss the TCP hint
            mDevice.pressBack()
        }.goToHomescreen {
            verifyJumpBackInMessage()
        }
    }

    @SmokeTest
    @Test
    fun openTotalCookieProtectionLearnMoreLinkTest() {
        val genericPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericPage.url) {
            verifyCookiesProtectionHintIsDisplayed(true)
            clickTotalCookieProtectionLearnMoreLink()
            verifyUrl("support.mozilla.org/en-US/kb/enhanced-tracking-protection-firefox-android")
        }
    }

    @SmokeTest
    @Test
    fun dismissTotalCookieProtectionHintTest() {
        val genericPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericPage.url) {
            verifyCookiesProtectionHintIsDisplayed(true)
            clickTotalCookieProtectionCloseButton()
            verifyCookiesProtectionHintIsDisplayed(false)
        }
    }
}
