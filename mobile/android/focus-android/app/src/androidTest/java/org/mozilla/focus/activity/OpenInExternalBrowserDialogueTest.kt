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
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.FeatureSettingsHelper
import org.mozilla.focus.helpers.MainActivityIntentsTestRule
import org.mozilla.focus.helpers.MockWebServerHelper
import org.mozilla.focus.helpers.StringsHelper.GOOGLE_CHROME
import org.mozilla.focus.helpers.TestAssetHelper.getPlainPageAsset
import org.mozilla.focus.helpers.TestHelper.assertNativeAppOpens
import org.mozilla.focus.testAnnotations.SmokeTest

// This test verifies the "Open in..." option from the main menu
@RunWith(AndroidJUnit4ClassRunner::class)
class OpenInExternalBrowserDialogueTest {
    private lateinit var webServer: MockWebServer
    private val featureSettingsHelper = FeatureSettingsHelper()

    @get: Rule
    var mActivityTestRule = MainActivityIntentsTestRule(showFirstRun = false)

    @Before
    fun setUp() {
        featureSettingsHelper.setCfrForTrackingProtectionEnabled(false)
        featureSettingsHelper.setNumberOfTabsOpened(4)
        webServer = MockWebServer().apply {
            dispatcher = MockWebServerHelper.AndroidAssetDispatcher()
            start()
        }
    }

    @After
    fun tearDown() {
        mActivityTestRule.activity.finishAndRemoveTask()
        webServer.shutdown()
        featureSettingsHelper.resetAllFeatureFlags()
    }

    @SmokeTest
    @Test
    fun openPageInExternalAppTest() {
        val pageUrl = getPlainPageAsset(webServer).url

        searchScreen {
        }.loadPage(pageUrl) {
        }.openMainMenu {
            clickOpenInOption()
            verifyOpenInDialog()
            clickOpenInChrome()
            assertNativeAppOpens(GOOGLE_CHROME)
        }
    }
}
