/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.searchScreen

class AddToHomeScreenTest {
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val composeTestRule =
        AndroidComposeTestRule(HomeActivityTestRule.withDefaultSettingsOverrides()) { it.activity }

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

    // Verifies the Add to home screen option in a tab's 3 dot menu
    @SmokeTest
    @Test
    fun mainMenuAddToHomeScreenTest() {
        val website = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val shortcutTitle = TestHelper.generateRandomString(5)

        homeScreen {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(website.url) {
        }.openThreeDotMenu {
            expandMenu()
        }.openAddToHomeScreen {
            clickCancelShortcutButton()
        }

        browserScreen {
        }.openThreeDotMenu {
            expandMenu()
        }.openAddToHomeScreen {
            verifyShortcutTextFieldTitle("Test_Page_1")
            addShortcutName(shortcutTitle)
            clickAddShortcutButton()
            clickAddAutomaticallyButton()
        }.openHomeScreenShortcut(shortcutTitle) {
            verifyUrl(website.url.toString())
            verifyTabCounter("1")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/414970
    @Ignore("Failure, more details at: https://bugzilla.mozilla.org/show_bug.cgi?id=1830005")
    @SmokeTest
    @Test
    fun addPrivateBrowsingShortcutFromHomeScreenCFRTest() {
        homeScreen {
        }.triggerPrivateBrowsingShortcutPrompt {
            verifyNoThanksPrivateBrowsingShortcutButton(composeTestRule)
            verifyAddPrivateBrowsingShortcutButton(composeTestRule)
            clickAddPrivateBrowsingShortcutButton(composeTestRule)
            clickAddAutomaticallyButton()
        }.openHomeScreenShortcut("Private ${TestHelper.appName}") {}
        searchScreen {
            verifySearchView()
        }.dismissSearchBar {
            verifyCommonMythsLink()
        }
    }
}
