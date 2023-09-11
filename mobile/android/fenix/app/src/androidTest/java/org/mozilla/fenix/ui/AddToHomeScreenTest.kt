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
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.searchScreen

class AddToHomeScreenTest {
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val activityIntentTestRule = HomeActivityIntentTestRule.withDefaultSettingsOverrides()

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

    @SmokeTest
    @Test
    fun addPrivateBrowsingShortcutTest() {
        homeScreen {
        }.triggerPrivateBrowsingShortcutPrompt {
            verifyNoThanksPrivateBrowsingShortcutButton()
            verifyAddPrivateBrowsingShortcutButton()
            clickAddPrivateBrowsingShortcutButton()
            clickAddAutomaticallyButton()
        }.openHomeScreenShortcut("Private ${TestHelper.appName}") {}
        searchScreen {
            verifySearchView()
        }.dismissSearchBar {
            verifyCommonMythsLink()
        }
    }
}
