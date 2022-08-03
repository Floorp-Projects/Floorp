/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.FeatureSettingsHelper
import org.mozilla.focus.helpers.MainActivityIntentsTestRule
import org.mozilla.focus.helpers.MockWebServerHelper
import org.mozilla.focus.helpers.RetryTestRule
import org.mozilla.focus.helpers.StringsHelper.GMAIL_APP
import org.mozilla.focus.helpers.StringsHelper.PHONE_APP
import org.mozilla.focus.helpers.TestAssetHelper
import org.mozilla.focus.helpers.TestHelper.assertNativeAppOpens
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.testAnnotations.SmokeTest

// These tests check that various web controls work properly
@RunWith(AndroidJUnit4ClassRunner::class)
class WebControlsTest {
    private lateinit var webServer: MockWebServer

    private val featureSettingsHelper = FeatureSettingsHelper()

    @get: Rule
    var mActivityTestRule = MainActivityIntentsTestRule(showFirstRun = false)

    @Rule
    @JvmField
    val retryTestRule = RetryTestRule(3)

    @Before
    fun setup() {
        webServer = MockWebServer().apply {
            dispatcher = MockWebServerHelper.AndroidAssetDispatcher()
            start()
        }
        featureSettingsHelper.setCfrForTrackingProtectionEnabled(false)
        featureSettingsHelper.setNumberOfTabsOpened(4)
    }

    @After
    fun tearDown() {
        webServer.shutdown()
        featureSettingsHelper.resetAllFeatureFlags()
    }

    @SmokeTest
    @Test
    fun verifyTextInputTest() {
        val htmlControlsPage = TestAssetHelper.getHTMLControlsPageAsset(webServer).url

        searchScreen {
        }.loadPage(htmlControlsPage) {
            progressBar.waitUntilGone(waitingTime)
            clickAndWriteTextInInputBox("wiki")
            clickSubmitTextInputButton()
            verifyPageContent("You entered: wiki")
        }
    }

    @SmokeTest
    @Test
    fun verifyDropdownMenuTest() {
        val htmlControlsPage = TestAssetHelper.getHTMLControlsPageAsset(webServer).url

        searchScreen {
        }.loadPage(htmlControlsPage) {
            progressBar.waitUntilGone(waitingTime)
            clickDropDownForm()
            selectDropDownOption("The National")
            clickSubmitDropDownButton()
            verifySelectedDropDownOption("The National")
        }
    }

    @SmokeTest
    @Test
    fun verifyExternalLinksTest() {
        val htmlControlsPage = TestAssetHelper.getHTMLControlsPageAsset(webServer).url

        searchScreen {
        }.loadPage(htmlControlsPage) {
            progressBar.waitUntilGone(waitingTime)
            clickLinkMatchingText("External link")
            progressBar.waitUntilGone(waitingTime)
            verifyPageURL("DuckDuckGo")
        }
    }

    @SmokeTest
    @Test
    fun emailLinkTest() {
        val htmlControlsPage = TestAssetHelper.getHTMLControlsPageAsset(webServer).url

        searchScreen {
        }.loadPage(htmlControlsPage) {
            clickLinkMatchingText("Email link")
            clickOpenLinksInAppsOpenButton()
            assertNativeAppOpens(GMAIL_APP)
        }
    }

    @SmokeTest
    @Test
    fun telephoneLinkTest() {
        val htmlControlsPage = TestAssetHelper.getHTMLControlsPageAsset(webServer).url

        searchScreen {
        }.loadPage(htmlControlsPage) {
            clickLinkMatchingText("Telephone link")
            clickOpenLinksInAppsOpenButton()
            assertNativeAppOpens(PHONE_APP)
        }
    }

    @SmokeTest
    @Test
    @Ignore("Failing, see https://github.com/mozilla-mobile/focus-android/issues/7363")
    fun verifyDismissTextSelectionToolbarTest() {
        val tab1Url = TestAssetHelper.getGenericTabAsset(webServer, 1).url
        val htmlControlsPage = TestAssetHelper.getHTMLControlsPageAsset(webServer).url

        searchScreen {
        }.loadPage(tab1Url) {
            progressBar.waitUntilGone(waitingTime)
        }.openSearchBar {
        }.loadPage(htmlControlsPage) {
            progressBar.waitUntilGone(waitingTime)
            longClickText("Copy")
        }.goToPreviousPage {
            verifyCopyOptionDoesNotExist()
        }
    }

    @SmokeTest
    @Test
    @Ignore("Failing, see https://github.com/mozilla-mobile/focus-android/issues/7363")
    fun verifySelectTextTest() {
        val htmlControlsPage = TestAssetHelper.getHTMLControlsPageAsset(webServer).url

        searchScreen {
        }.loadPage(htmlControlsPage) {
            progressBar.waitUntilGone(waitingTime)
            longClickAndCopyText("Copy")
            clickAndPasteTextInInputBox()
            clickSubmitTextInputButton()
            verifyPageContent("You entered: Copy")
        }
    }

    @SmokeTest
    @Test
    fun verifyCalendarFormTest() {
        val htmlControlsPage = TestAssetHelper.getHTMLControlsPageAsset(webServer).url

        searchScreen {
        }.loadPage(htmlControlsPage) {
            progressBar.waitUntilGone(waitingTime)
            clickCalendarForm()
            selectDate()
            clickFormViewButton("OK")
            clickSubmitDateButton()
            verifySelectedDate()
        }
    }
}
