/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.core.net.toUri
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.activity.robots.homeScreen
import org.mozilla.focus.helpers.FeatureSettingsHelper
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.RetryTestRule
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.testAnnotations.SmokeTest

// These tests the Privacy and Security settings menus and options
@RunWith(AndroidJUnit4ClassRunner::class)
class SettingsPrivacyTest {
    private val featureSettingsHelper = FeatureSettingsHelper()

    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @Rule
    @JvmField
    val retryTestRule = RetryTestRule(3)

    @Before
    fun setup() {
        featureSettingsHelper.setCfrForTrackingProtectionEnabled(false)
    }

    @After
    fun tearDown() {
        featureSettingsHelper.resetAllFeatureFlags()
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
    fun verifyCookiesEnabledTest() {
        val cookiesEnabledURL = "https://www.whatismybrowser.com/detect/are-cookies-enabled"

        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openPrivacySettingsMenu {
            clickBlockCookies()
            clickYesPleaseOption()
        }.goBackToSettings {
        }.goBackToHomeScreen {
        }.loadPage(cookiesEnabledURL.toUri().toString()) {
            progressBar.waitUntilGone(waitingTime)
            verifyCookiesEnabled("No")
        }
    }

    @SmokeTest
    @Test
    fun verify3rdPartyCookiesEnabledTest() {
        val cookiesEnabledURL = "https://www.whatismybrowser.com/detect/are-third-party-cookies-enabled"

        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openPrivacySettingsMenu {
            clickBlockCookies()
            clickYesPleaseOption()
        }.goBackToSettings {
        }.goBackToHomeScreen {
        }.loadPage(cookiesEnabledURL.toUri().toString()) {
            progressBar.waitUntilGone(waitingTime)
            verifyCookiesEnabled("No")
        }
    }
}
