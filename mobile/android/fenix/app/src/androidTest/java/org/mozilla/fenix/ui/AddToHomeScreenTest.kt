/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.core.net.toUri
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.clickPageObject
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.robots.searchScreen

class AddToHomeScreenTest {
    private lateinit var mockWebServer: MockWebServer
    private val downloadTestPage =
        "https://storage.googleapis.com/mobile_test_assets/test_app/downloads.html"
    private val pdfFileName = "washington.pdf"
    private val pdfFileURL = "storage.googleapis.com/mobile_test_assets/public/washington.pdf"
    private val pdfFileContent = "Washington Crossing the Delaware"

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

    @SmokeTest
    @Test
    fun addPDFToHomeScreenTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(downloadTestPage.toUri()) {
            clickPageObject(itemContainingText(pdfFileName))
            verifyUrl(pdfFileURL)
            verifyPageContent(pdfFileContent)
        }.openThreeDotMenu {
            expandMenu()
        }.openAddToHomeScreen {
            verifyShortcutTextFieldTitle(pdfFileName)
            clickAddShortcutButton()
            clickAddAutomaticallyButton()
        }.openHomeScreenShortcut(pdfFileName) {
            verifyUrl(pdfFileURL)
        }
    }
}
